# SPDX-License-Identifier: MPL-2.0

import builtins
import ctypes
import json
import os
import select
import selectors
import shutil
import signal
import stat
import struct
import subprocess
import tarfile
import time
import zipfile
from pathlib import Path, PurePosixPath

from qa3_source_cohort import capture_tree, file_digest, safe_path
from runtime_build_context import parse_rest_json

GIB = 1024**3
MAX_FILES = 200000
MAX_EXPANDED = 50 * GIB
MAX_COMPRESSED = 20 * GIB


def read_json(path, maximum=128 * 1024**2):
    if path.is_symlink() or not path.is_file() or path.stat().st_size > maximum:
        raise ValueError("unsafe or oversized JSON")
    return dict(parse_rest_json(path.read_bytes()))


def write_json(path, value):
    with path.open("x", encoding="utf-8") as stream:
        json.dump(value, stream, indent=2, ensure_ascii=False, allow_nan=False)
        stream.write("\n")


def outside(source, path):
    source = source.resolve()
    if path.is_symlink() or path.resolve().is_relative_to(source):
        raise ValueError("state, OBJDIR or output lies inside consumed source")
    return path.resolve()


def capture_material_tree(root):
    if root.is_symlink() or not root.is_dir():
        raise ValueError("material inventory root is missing or unsafe")
    for name in (".git", ".hg", "mozconfig"):
        path = root / name
        if path.exists() or path.is_symlink():
            raise ValueError("non-source material contains excluded source input")
    return capture_tree(root)


def fresh(path):
    if path.exists() or path.is_symlink():
        raise ValueError("output already exists")
    path.mkdir(parents=True)
    return path.resolve()


def child_environment(state, extra=None):
    state.mkdir(parents=True, exist_ok=True)
    env = {
        k: os.environ[k]
        for k in ("PATH", "HOME", "LANG", "LC_ALL", "TZ")
        if k in os.environ
    }
    env.update(
        PYTHONDONTWRITEBYTECODE="1",
        PYTHONNOUSERSITE="1",
        MOZBUILD_STATE_PATH=str(state / "mozbuild"),
        TMPDIR=str(state / "tmp"),
        XDG_CACHE_HOME=str(state / "cache"),
        XDG_CONFIG_HOME=str(state / "config"),
        XDG_DATA_HOME=str(state / "data"),
        MOZ_CRASHREPORTER_DISABLE="1",
        MOZ_DISABLE_NONLOCAL_CONNECTIONS="1",
        MOZ_AUTOMATION="1",
        GIT_CONFIG_NOSYSTEM="1",
        GIT_CONFIG_GLOBAL="/dev/null",
        GIT_NO_LAZY_FETCH="1",
    )
    for name in ("tmp", "cache", "config", "data"):
        (state / name).mkdir(exist_ok=True)
    allowed = {
        "MOZCONFIG",
        "MOZ_BUILD_DATE",
        "MOZ_NUM_JOBS",
        "PYTHONPATH",
        "LD_LIBRARY_PATH",
        "DISPLAY",
        "LIBGL_ALWAYS_SOFTWARE",
        "MOZ_HEADLESS",
        "MOZ_OBJDIR",
    }
    if extra:
        if set(extra) - allowed or any(not isinstance(v, str) for v in extra.values()):
            raise ValueError("unapproved native environment variable")
        env.update(extra)
    return env


def _proc(pid):
    try:
        data = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return int(data[1]), data[0], int(data[19])
    except (OSError, ValueError, IndexError):
        return None


def _descendants(owned, prior_children):
    for entry in Path("/proc").iterdir():
        if not entry.name.isdecimal():
            continue
        pid = int(entry.name)
        info = _proc(pid)
        direct_orphan = (
            info and info[0] == os.getpid() and (pid, info[2]) not in prior_children
        )
        parent = _proc(info[0]) if info and info[0] in owned else None
        if info and (direct_orphan or parent and parent[2] == owned[info[0]]):
            owned[pid] = info[2]


def owned_alive(owned):
    result = {}
    for pid, birth in owned.items():
        info = _proc(pid)
        if info and info[2] == birth:
            result[pid] = info
    return result


class IOBudget:
    def __init__(self, root, maximum, minimum_free=0):
        self.root = root
        self.maximum = maximum
        self.minimum_free = minimum_free
        self.used = 0

    def consume(self, size):
        if size < 0 or self.used + size > self.maximum:
            raise ValueError("cumulative staging byte budget exceeded")
        self.check(size)
        self.used += size

    def check(self, pending=0):
        if shutil.disk_usage(self.root).free < self.minimum_free + pending:
            raise ValueError("native I/O would consume reserved free space")

    def release_tree(self, path):
        size = (
            sum(
                p.stat().st_size
                for p in path.rglob("*")
                if p.is_file() and not p.is_symlink()
            )
            if path.is_dir()
            else path.stat().st_size
        )
        if size > self.used:
            raise ValueError("native I/O budget accounting underflow")
        if path.is_dir():
            shutil.rmtree(path)
        else:
            path.unlink()
        self.used -= size


class ExpansionBudget:
    def __init__(self, maximum=MAX_EXPANDED, members=MAX_FILES):
        self.maximum = maximum
        self.members = members
        self.bytes = 0
        self.count = 0

    def claim(self, size):
        if (
            size < 0
            or self.bytes + size > self.maximum
            or self.count + 1 > self.members
        ):
            raise ValueError("cumulative archive expansion/member budget exceeded")
        self.bytes += size
        self.count += 1


def copy_file(source, destination, budget):
    with source.open("rb") as inp, destination.open("xb") as out:
        while block := inp.read(1024**2):
            budget.consume(len(block))
            out.write(block)
    destination.chmod(0o755 if source.stat().st_mode & 0o111 else 0o644)


def verify_group_limits(
    role,
    context,
    cgroup_root=Path("/sys/fs/cgroup"),
    proc_cgroup=Path("/proc/self/cgroup"),
):
    if role not in {"builder", "consumer"}:
        raise ValueError("unknown job resource role")
    records = proc_cgroup.read_text().splitlines()
    unified = [v[3:] for v in records if v.startswith("0::")]
    if len(unified) != 1 or not unified[0].startswith("/"):
        raise ValueError("dedicated unified native cgroup is unavailable")
    relative = PurePosixPath(unified[0])
    if (
        ".." in relative.parts
        or relative.name != f"qa3-{context['run']}-{context['attempt']}-{role}.scope"
    ):
        raise ValueError("native process is outside its dedicated run/attempt scope")
    root = cgroup_root / str(relative).lstrip("/")
    memory = (root / "memory.max").read_text().strip()
    pids = (root / "pids.max").read_text().strip()
    cap = (28 if role == "builder" else 14) * GIB
    if (
        not memory.isdecimal()
        or int(memory) != cap
        or not pids.isdecimal()
        or not 1 <= int(pids) <= 512
    ):
        raise ValueError(
            "native descendant group memory/pids caps are missing or oversized"
        )
    quota = (root / "cpu.max").read_text().split()
    cores = 8 if role == "builder" else 4
    if (
        len(quota) != 2
        or not all(v.isdecimal() for v in quota)
        or int(quota[1]) == 0
        or int(quota[0]) != cores * int(quota[1])
    ):
        raise ValueError("native CPU scope quota differs from the budget")
    events = {
        k: int(v)
        for k, v in (
            line.split() for line in (root / "memory.events").read_text().splitlines()
        )
    }
    if events.get("oom", 0) or events.get("oom_kill", 0):
        raise ValueError("native scope has already suffered an OOM")
    return {
        "scope": str(relative),
        "memoryMaxBytes": int(memory),
        "pidsMax": int(pids),
        "cpuQuota": cores,
        "memoryEvents": events,
    }


def _run_owned(
    argv,
    cwd,
    env,
    logs,
    name,
    timeout,
    memory=28 * GIB,
    log_limit=2 * GIB,
    allow_nonzero=False,
    minimum_free=0,
    data_stdout=False,
    io_budget=None,
    data_limit=MAX_EXPANDED,
    cancelled=lambda: False,
):
    safe_path(name)
    if "/" in name or not argv or not all(isinstance(x, str) for x in argv):
        raise ValueError("invalid fixed command")
    logs.mkdir(parents=True, exist_ok=True)
    out = logs / (name + ".stdout")
    err = logs / (name + ".stderr")
    logged = sum(
        p.stat().st_size
        for p in logs.iterdir()
        if p.is_file() and p.suffix in {".stdout", ".stderr"}
    )
    if logged > log_limit:
        raise ValueError("cumulative prior raw logs exceed budget")
    started = time.monotonic()
    owned = {}
    stop = None
    libc = ctypes.CDLL(None, use_errno=True)
    if libc.prctl(36, 1, 0, 0, 0) != 0:
        raise OSError("cannot own orphaned native descendants")
    prior_children = set()
    for entry in Path("/proc").iterdir():
        if entry.name.isdecimal():
            info = _proc(int(entry.name))
            if info and info[0] == os.getpid():
                prior_children.add((int(entry.name), info[2]))
    limiter = "/usr/bin/prlimit"
    if not os.access(limiter, os.X_OK):
        raise ValueError("required native process limiter missing")
    executed = [
        limiter,
        f"--as={memory}",
        f"--fsize={MAX_EXPANDED}",
        "--core=0",
        "--",
        *argv,
    ]
    data_bytes = 0
    process = None
    failure = None
    cleanup_verified = True
    cleanup_errors = {}
    drain_ready = False

    def remember_error(exc, phase):
        nonlocal failure, stop
        if failure is None:
            failure = (exc, exc.__traceback__, phase)
        elif phase not in cleanup_errors:
            cleanup_errors[phase] = {
                "type": type(exc).__name__,
                "errno": getattr(exc, "errno", None),
                "message": str(exc),
            }
        stop = stop or "PROCESS_ERROR"

    def cleanup_call(operation, phase, ownership=False):
        nonlocal cleanup_verified
        try:
            return operation()
        except BaseException as exc:
            remember_error(exc, phase)
            if ownership:
                cleanup_verified = False
            return None

    try:
        with out.open("xb") as stdout, err.open(
            "xb"
        ) as stderr, selectors.DefaultSelector() as selector:
            process = subprocess.Popen(
                executed,
                cwd=cwd,
                env=env,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                start_new_session=True,
            )
            phase = "INITIALIZE"
            try:
                initial = _proc(process.pid)
                if initial:
                    owned[process.pid] = initial[2]
                for pipe, sink in ((process.stdout, stdout), (process.stderr, stderr)):
                    os.set_blocking(pipe.fileno(), False)
                    selector.register(pipe, selectors.EVENT_READ, sink)

                def drain(wait):
                    nonlocal logged, data_bytes, stop
                    for key, _ in selector.select(wait):
                        try:
                            block = os.read(key.fileobj.fileno(), 65536)
                        except BlockingIOError:
                            continue
                        if not block:
                            selector.unregister(key.fileobj)
                            key.fileobj.close()
                            continue
                        is_data = data_stdout and key.data is stdout
                        remaining = (
                            data_limit - data_bytes if is_data else log_limit - logged
                        )
                        if len(block) > remaining:
                            stop = stop or ("DATA_LIMIT" if is_data else "LOG_LIMIT")
                            block = block[: max(remaining, 0)]
                        if block:
                            if minimum_free and shutil.disk_usage(
                                logs
                            ).free < minimum_free + len(block):
                                stop = stop or "DISK_LIMIT"
                                continue
                            if is_data and io_budget:
                                try:
                                    io_budget.consume(len(block))
                                except ValueError:
                                    stop = stop or "DISK_LIMIT"
                                    continue
                            key.data.write(block)
                            if is_data:
                                data_bytes += len(block)
                            else:
                                logged += len(block)

                drain_ready = True
                phase = "RUN"
                leader_done = None
                while selector.get_map() or process.poll() is None:
                    if cancelled():
                        stop = stop or "CANCELLED"
                    _descendants(owned, prior_children)
                    if process.poll() is not None:
                        leader_done = leader_done or time.monotonic()
                        if time.monotonic() - leader_done > 1:
                            break
                    if time.monotonic() - started > timeout:
                        stop = "TIMEOUT"
                    if minimum_free and shutil.disk_usage(logs).free < minimum_free:
                        stop = stop or "DISK_LIMIT"
                    drain(0.05)
                    if stop:
                        break
            except BaseException as exc:
                remember_error(exc, phase)
                drain_ready = False
            finally:

                def cleanup_drain(wait):
                    nonlocal drain_ready
                    if cancelled():
                        stop_on_cancel()
                    if not drain_ready:
                        time.sleep(wait)
                        return
                    try:
                        drain(wait)
                    except BaseException as exc:
                        remember_error(exc, "CLEANUP_IO")
                        drain_ready = False

                def stop_on_cancel():
                    nonlocal stop
                    stop = stop or "CANCELLED"

                leader = cleanup_call(
                    lambda: _proc(process.pid), "CLEANUP_SNAPSHOT", ownership=True
                )
                if leader:
                    owned.setdefault(process.pid, leader[2])
                for sig in (signal.SIGTERM, signal.SIGKILL):
                    signaled = set()
                    leader = cleanup_call(
                        lambda: _proc(process.pid), "CLEANUP_SNAPSHOT", ownership=True
                    )
                    if leader and leader[2] == owned.get(process.pid):
                        try:
                            os.killpg(process.pid, sig)
                        except ProcessLookupError:
                            pass
                        except BaseException as exc:
                            remember_error(exc, "CLEANUP_SIGNAL")
                    alive = None
                    deadline = time.monotonic() + 2
                    while time.monotonic() < deadline:
                        cleanup_call(
                            lambda: _descendants(owned, prior_children),
                            "CLEANUP_DISCOVER",
                            ownership=True,
                        )
                        alive = cleanup_call(
                            lambda: owned_alive(owned),
                            "CLEANUP_SNAPSHOT",
                            ownership=True,
                        )
                        for pid, info in (alive or {}).items():
                            if info[1] != "Z" and (pid, info[2]) not in signaled:
                                try:
                                    os.kill(pid, sig)
                                    signaled.add((pid, info[2]))
                                except ProcessLookupError:
                                    pass
                                except BaseException as exc:
                                    remember_error(exc, "CLEANUP_SIGNAL")
                            if pid != process.pid:
                                try:
                                    os.waitpid(pid, os.WNOHANG)
                                except ChildProcessError:
                                    pass
                                except BaseException as exc:
                                    remember_error(exc, "CLEANUP_REAP")
                                    cleanup_verified = False
                        cleanup_drain(0.02)
                        cleanup_call(process.poll, "CLEANUP_REAP", ownership=True)
                        alive = cleanup_call(
                            lambda: owned_alive(owned),
                            "CLEANUP_SNAPSHOT",
                            ownership=True,
                        )
                        if alive == {}:
                            break
                    if alive == {}:
                        break
                cleanup_call(
                    lambda: process.wait(timeout=2), "CLEANUP_REAP", ownership=True
                )
                if drain_ready:
                    deadline = time.monotonic() + 2
                    while drain_ready and time.monotonic() < deadline:
                        pending = cleanup_call(selector.get_map, "CLEANUP_IO")
                        if not pending:
                            break
                        cleanup_drain(0.02)
                for pipe in (process.stdout, process.stderr):
                    cleanup_call(pipe.close, "CLEANUP_PIPE")
    except BaseException as exc:
        remember_error(exc, "RESOURCE_CLOSE")
    leftovers = cleanup_call(
        lambda: list(owned_alive(owned)), "CLEANUP_SNAPSHOT", ownership=True
    )
    if leftovers is None or leftovers or not cleanup_verified:
        stop = "CLEANUP_FAILED"
    result = {
        "name": name,
        "argv": argv,
        "exit": process.returncode if process else None,
        "stop": stop,
        "durationSeconds": round(time.monotonic() - started, 3),
        "stdoutSha256": cleanup_call(lambda: file_digest(out), "RECEIPT_HASH"),
        "stderrSha256": cleanup_call(lambda: file_digest(err), "RECEIPT_HASH"),
        "ownedCleanupComplete": bool(process) and cleanup_verified and leftovers == [],
        "stdoutIsData": data_stdout,
    }
    if cancelled():
        stop = stop or "CANCELLED"
        result["cancelled"] = True
    result["stop"] = stop
    if failure:
        exc, _, phase = failure
        result["stop"] = stop
        result["error"] = {
            "type": type(exc).__name__,
            "errno": getattr(exc, "errno", None),
            "message": str(exc),
            "phase": phase,
        }
    if cleanup_errors:
        result["cleanupErrors"] = cleanup_errors
    cleanup_call(
        lambda: write_json(logs / (name + ".process.json"), result), "RECEIPT_WRITE"
    )
    if failure:
        exc, traceback, _ = failure
        raise exc.with_traceback(traceback)
    if stop or process.returncode != 0 and not allow_nonzero:
        raise ValueError(f"{name} failed: {stop or 'NONZERO_EXIT'}")
    return result


def _wait_supervisor(pid, timeout, on_error=None):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            waited, status = os.waitpid(pid, os.WNOHANG)
        except ChildProcessError:
            raise
        except BaseException as exc:
            if on_error is None:
                raise
            on_error(exc)
            waited = 0
        if waited:
            return status
        time.sleep(0.02)
    return None


def _supervise(command, timeout, io_budget):
    """Isolate orphan adoption; hard kill/stop recovery needs the external job scope."""
    reader, writer = os.pipe()
    try:
        pid = os.fork()
    except BaseException:
        os.close(reader)
        os.close(writer)
        raise
    if pid == 0:
        os.close(reader)
        cancel_requested = False

        def cancelled(signum, frame):
            nonlocal cancel_requested
            cancel_requested = True

        signal.signal(signal.SIGTERM, cancelled)
        signal.signal(signal.SIGINT, cancelled)
        try:
            try:
                result = command(lambda: cancel_requested)
                if cancel_requested:
                    raise InterruptedError("owned command supervisor cancelled")
                error = None
            except BaseException as exc:
                result = None
                error = {
                    "type": type(exc).__name__,
                    "args": list(exc.args),
                    "errno": getattr(exc, "errno", None),
                    "filename": getattr(exc, "filename", None),
                    "message": str(exc),
                }
            reply = {
                "result": result,
                "error": error,
                "budgetUsed": io_budget.used if io_budget else None,
            }
            data = json.dumps(reply, allow_nan=False).encode()
            if len(data) > 128 * 1024:
                raise ValueError("owned supervisor reply exceeds limit")
            while data:
                data = data[os.write(writer, data) :]
        except BaseException:
            os._exit(1)
        os.close(writer)
        os._exit(0)
    status = None
    failure = None
    data = bytearray()
    birth = None

    def cleanup_error(exc):
        nonlocal failure
        failure = failure or (exc, exc.__traceback__)

    try:
        birth = _proc(pid)
        os.close(writer)
        os.set_blocking(reader, False)
        deadline = time.monotonic() + timeout + 12
        while time.monotonic() < deadline:
            ready, _, _ = select.select([reader], [], [], 0.05)
            if ready:
                block = os.read(reader, 65536)
                if not block:
                    break
                data.extend(block)
                if len(data) > 128 * 1024:
                    raise ValueError("owned supervisor reply exceeds limit")
        else:
            raise TimeoutError("owned supervisor did not finish bounded cleanup")
        status = _wait_supervisor(pid, 1)
    except BaseException as exc:
        failure = (exc, exc.__traceback__)
    finally:
        try:
            os.close(reader)
        except OSError:
            pass
        for sig, interval in ((signal.SIGTERM, 10), (signal.SIGKILL, 2)):
            if status is not None:
                break
            try:
                current = _proc(pid)
                if birth and current and current[2] == birth[2]:
                    os.kill(pid, sig)
            except ProcessLookupError:
                pass
            except BaseException as exc:
                cleanup_error(exc)
            try:
                status = _wait_supervisor(pid, interval, cleanup_error)
            except ChildProcessError as exc:
                cleanup_error(exc)
                break
            except BaseException as exc:
                cleanup_error(exc)
    if failure:
        exc, traceback = failure
        raise exc.with_traceback(traceback)
    if status != 0:
        raise RuntimeError("owned supervisor failed to retain its result")
    reply = dict(parse_rest_json(bytes(data)))
    if io_budget:
        used = reply["budgetUsed"]
        if type(used) is not int or not io_budget.used <= used <= io_budget.maximum:
            raise ValueError("owned supervisor changed the staging budget")
        io_budget.used = used
    if error := reply["error"]:
        kind = getattr(builtins, error["type"], RuntimeError)
        if not isinstance(kind, type) or not issubclass(kind, BaseException):
            kind = RuntimeError
        if issubclass(kind, OSError) and error["filename"] is not None:
            raise kind(*error["args"], error["filename"])
        raise kind(*error["args"])
    return reply["result"]


def run_owned(
    argv,
    cwd,
    env,
    logs,
    name,
    timeout,
    memory=28 * GIB,
    log_limit=2 * GIB,
    allow_nonzero=False,
    minimum_free=0,
    data_stdout=False,
    io_budget=None,
    data_limit=MAX_EXPANDED,
):
    return _supervise(
        lambda cancelled: _run_owned(
            argv,
            cwd,
            env,
            logs,
            name,
            timeout,
            memory,
            log_limit,
            allow_nonzero,
            minimum_free,
            data_stdout,
            io_budget,
            data_limit,
            cancelled,
        ),
        timeout,
        io_budget,
    )


def elf_identity(path):
    if path.is_symlink() or not path.is_file() or not path.stat().st_mode & 0o111:
        raise ValueError("missing regular executable ELF helper")
    with path.open("rb") as stream:
        head = stream.read(64)
        if len(head) != 64 or head[:6] != b"\x7fELF\x02\x01":
            raise ValueError("not a little-endian ELF64 binary")
        if struct.unpack_from("<H", head, 18)[0] != 62:
            raise ValueError("wrong native architecture")
        offset = struct.unpack_from("<Q", head, 32)[0]
        size, count = struct.unpack_from("<HH", head, 54)
        if size != 56 or count > 256:
            raise ValueError("invalid ELF program headers")
        ids = []
        for i in range(count):
            stream.seek(offset + i * size)
            item = stream.read(size)
            if len(item) != size:
                raise ValueError("truncated ELF")
            if struct.unpack_from("<I", item)[0] != 4:
                continue
            start, length = (
                struct.unpack_from("<QQ", item, 8)[0],
                struct.unpack_from("<Q", item, 32)[0],
            )
            if length > 1024**2:
                raise ValueError("oversized ELF notes")
            stream.seek(start)
            data = stream.read(length)
            if len(data) != length:
                raise ValueError("truncated ELF note segment")
            pos = 0
            while pos + 12 <= len(data):
                namesize, descsize, kind = struct.unpack_from("<III", data, pos)
                pos += 12
                name = data[pos : pos + namesize]
                pos += (namesize + 3) & ~3
                desc = data[pos : pos + descsize]
                pos += (descsize + 3) & ~3
                if (
                    len(name) != namesize
                    or len(desc) != descsize
                    or pos > len(data) + 3
                ):
                    raise ValueError("truncated ELF note")
                if kind == 3 and name == b"GNU\0":
                    ids.append(desc.hex())
    if len(ids) != 1 or not ids[0]:
        raise ValueError("missing or ambiguous ELF GNU build ID")
    return {"sha256": file_digest(path), "gnuBuildID": ids[0], "machine": "x86_64"}


def _archive_name(value):
    while value.startswith("./"):
        value = value[2:]
    return safe_path(value.rstrip("/"))


def extract_archive(
    archive,
    destination,
    expanded=MAX_EXPANDED,
    max_files=MAX_FILES,
    budget=None,
    expansion=None,
):
    fresh(destination)
    names = set()
    links = []
    total = 0

    def target(name, size):
        nonlocal total
        name = _archive_name(name)
        if name in names or len(names) >= max_files or size < 0:
            raise ValueError("duplicate or excessive archive member")
        names.add(name)
        if expansion is not None:
            expansion.claim(size)
        total += size
        if total > expanded:
            raise ValueError("expanded archive budget exceeded")
        path = destination / name
        for parent in PurePosixPath(name).parents:
            if parent.as_posix() in {n for n, _ in links}:
                raise ValueError("archive writes through symlink")
        path.parent.mkdir(parents=True, exist_ok=True)
        return path

    def copy(stream, path, size, mode):
        remaining = size
        with path.open("xb") as out:
            while remaining:
                data = stream.read(min(1024**2, remaining))
                if not data:
                    raise ValueError("truncated archive file")
                if budget is not None:
                    budget.consume(len(data))
                out.write(data)
                remaining -= len(data)
        path.chmod(0o755 if mode & 0o111 else 0o644)

    if zipfile.is_zipfile(archive):
        with zipfile.ZipFile(archive) as reader:
            for item in reader.infolist():
                mode = item.external_attr >> 16
                if stat.S_ISLNK(mode) or item.flag_bits & 1:
                    raise ValueError("ZIP symlink or encrypted member")
                if stat.S_IFMT(mode) not in {0, stat.S_IFREG, stat.S_IFDIR}:
                    raise ValueError("special ZIP member")
                path = target(item.filename, 0 if item.is_dir() else item.file_size)
                if item.is_dir():
                    path.mkdir(exist_ok=True)
                else:
                    with reader.open(item) as stream:
                        copy(stream, path, item.file_size, mode)
    else:
        with tarfile.open(archive, "r:*") as reader:
            for item in reader:
                if item.name in {".", "./"} and item.isdir():
                    continue
                if not (item.isfile() or item.isdir() or item.issym()):
                    raise ValueError("hardlink or special TAR member")
                path = target(item.name, item.size if item.isfile() else 0)
                if item.isdir():
                    path.mkdir(exist_ok=True)
                elif item.issym():
                    if (
                        path.exists()
                        or Path(item.linkname).is_absolute()
                        or "\\" in item.linkname
                    ):
                        raise ValueError("unsafe archive symlink")
                    links.append((
                        path.relative_to(destination).as_posix(),
                        item.linkname,
                    ))
                else:
                    stream = reader.extractfile(item)
                    if stream is None:
                        raise ValueError("missing archive data")
                    with stream:
                        copy(stream, path, item.size, item.mode)
    for name, link in links:
        path = destination / name
        resolved = (path.parent / link).resolve()
        if not resolved.is_relative_to(destination.resolve()) or not resolved.exists():
            raise ValueError("external or dangling archive symlink")
        if any(ord(c) < 32 for c in link):
            raise ValueError("invalid archive symlink")
        path.symlink_to(link)
    if not names:
        raise ValueError("empty archive")
    return {"members": len(names), "expandedBytes": total}


def pack_tree(root, archive, budget=None):
    if archive.exists() or archive.is_symlink():
        raise ValueError("archive overwrite refused")

    class Writer:
        def __init__(self, stream):
            self.stream = stream

        def write(self, data):
            if budget is not None:
                budget.consume(len(data))
            return self.stream.write(data)

        def __getattr__(self, name):
            return getattr(self.stream, name)

    with archive.open("xb") as output, tarfile.open(
        fileobj=Writer(output), mode="w:gz", dereference=False
    ) as writer:
        for path in sorted(root.rglob("*")):
            rel = path.relative_to(root).as_posix()
            safe_path(rel)
            if path.is_symlink() and (
                not path.resolve().is_relative_to(root.resolve()) or not path.exists()
            ):
                raise ValueError("unsafe bundle symlink")
            if not (path.is_dir() or path.is_file() or path.is_symlink()):
                raise ValueError("special bundle member")
            writer.add(path, arcname=rel, recursive=False)
    return file_digest(archive)


def verify_budget(root, minimum_free, ram, cpus):
    import shutil

    values = {}
    for line in Path("/proc/meminfo").read_text().splitlines():
        key, value = line.split(":", 1)
        if key == "MemTotal":
            values[key] = int(value.strip().split()[0]) * 1024
    mem_limit = Path("/sys/fs/cgroup/memory.max")
    available = values.get("MemTotal", 0)
    if mem_limit.is_file() and mem_limit.read_text().strip() != "max":
        available = min(available, int(mem_limit.read_text()))
    cpu = len(os.sched_getaffinity(0))
    quota = Path("/sys/fs/cgroup/cpu.max")
    if quota.is_file():
        amount, period = quota.read_text().split()
        if amount != "max":
            cpu = min(cpu, int(amount) / int(period))
    free = shutil.disk_usage(root).free
    if free < minimum_free or available < ram or cpu < cpus:
        raise ValueError("private worker resource prerequisite not met")
    return {"freeBytes": free, "availableMemoryBytes": available, "cpuQuota": cpu}
