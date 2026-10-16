# SPDX-License-Identifier: MPL-2.0

import ctypes
import errno
import io
import os
import shutil
import signal
import struct
import subprocess
import sys
import tarfile
import tempfile
import time
import unittest
import zipfile
from pathlib import Path
from unittest import mock

import qa3_native_io
from qa3_native_io import (
    GIB,
    ExpansionBudget,
    IOBudget,
    _descendants,
    child_environment,
    copy_file,
    elf_identity,
    extract_archive,
    owned_alive,
    pack_tree,
    read_json,
    run_owned,
    verify_group_limits,
    write_json,
)


def elf(path, machine=62, buildid=b"fixture-id"):
    header = bytearray(64)
    header[:6] = b"\x7fELF\x02\x01"
    struct.pack_into("<H", header, 18, machine)
    struct.pack_into("<Q", header, 32, 64)
    struct.pack_into("<HH", header, 54, 56, 1)
    notes = struct.pack("<III", 4, len(buildid), 3) + b"GNU\0" + buildid
    notes += bytes((-len(buildid)) % 4)
    program = bytearray(56)
    struct.pack_into("<I", program, 0, 4)
    struct.pack_into("<Q", program, 8, 120)
    struct.pack_into("<Q", program, 32, len(notes))
    path.write_bytes(header + program + notes)
    path.chmod(0o755)
    return path


def archive(path, entries):
    with tarfile.open(path, "w:gz") as out:
        for name, content, kind in entries:
            item = tarfile.TarInfo(name)
            if kind == "file":
                item.size = len(content)
                out.addfile(item, io.BytesIO(content))
            else:
                item.type = {
                    "symlink": tarfile.SYMTYPE,
                    "hardlink": tarfile.LNKTYPE,
                    "device": tarfile.CHRTYPE,
                }[kind]
                item.linkname = content
                out.addfile(item)
    return path


class NativeIOTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)

    def tar(self, entries, name="input.tar.gz"):
        return archive(self.root / name, entries)

    def test_archive_roundtrip_and_native_elf_identity(self):
        source = self.root / "source"
        source.mkdir()
        elf(source / "helper")
        (source / "data").write_bytes(b"complete")
        packed = self.root / "bundle.tar.gz"
        pack_tree(source, packed, IOBudget(self.root, 4096))
        destination = self.root / "expanded"
        result = extract_archive(packed, destination, budget=IOBudget(self.root, 4096))
        self.assertEqual(result["members"], 2)
        self.assertEqual(
            elf_identity(source / "helper"), elf_identity(destination / "helper")
        )

    def test_archive_traversal_and_duplicate_names(self):
        for i, entries in enumerate((
            [("../outside", b"bad", "file")],
            [("/absolute", b"bad", "file")],
            [("a//b", b"bad", "file")],
            [("a", b"one", "file"), ("a", b"two", "file")],
        )):
            with self.subTest(i=i), self.assertRaises(ValueError):
                extract_archive(
                    self.tar(entries, str(i) + ".tar.gz"), self.root / str(i)
                )

    def test_archive_special_hardlink_and_external_symlink(self):
        for i, entries in enumerate((
            [("a", "target", "hardlink")],
            [("a", "", "device")],
            [("a", "../outside", "symlink")],
            [("a", "absent", "symlink")],
            [
                ("a", "payload", "symlink"),
                ("a/b", b"bad", "file"),
                ("payload", b"x", "file"),
            ],
        )):
            with self.subTest(i=i), self.assertRaises((OSError, ValueError)):
                extract_archive(
                    self.tar(entries, str(i) + ".tar.gz"), self.root / str(i)
                )

    def test_archive_member_and_expanded_limits(self):
        source = self.tar([("a", b"1234", "file"), ("b", b"1234", "file")])
        for i, kwargs in enumerate(({"expanded": 7}, {"max_files": 1})):
            with self.subTest(i=i), self.assertRaises(ValueError):
                extract_archive(source, self.root / str(i), **kwargs)

    def test_archive_expansion_cumulative_across_inputs(self):
        expansion = ExpansionBudget(maximum=7)
        for i in range(2):
            source = self.tar([(str(i), b"1234", "file")], str(i) + ".tar.gz")
            if i == 0:
                extract_archive(source, self.root / str(i), expansion=expansion)
            else:
                with self.assertRaises(ValueError):
                    extract_archive(source, self.root / str(i), expansion=expansion)

    def test_stage_merge_peak_is_not_just_final_size(self):
        budget = IOBudget(self.root, 7)
        stage = self.root / "stage"
        extract_archive(self.tar([("a", b"1234", "file")]), stage, budget=budget)
        with self.assertRaises(ValueError):
            copy_file(stage / "a", self.root / "copy", budget)
        self.assertEqual(budget.used, 4)
        budget.release_tree(stage)
        self.assertEqual(budget.used, 0)

    def test_copy_pack_and_extract_keep_free_reserve(self):
        source = self.root / "source"
        source.mkdir()
        (source / "file").write_bytes(b"1234")
        budget = IOBudget(self.root, 4096, minimum_free=10)
        with mock.patch(
            "qa3_native_io.shutil.disk_usage",
            return_value=shutil._ntuple_diskusage(100, 90, 10),
        ):
            for operation in (
                lambda: copy_file(source / "file", self.root / "copy", budget),
                lambda: pack_tree(source, self.root / "packed", budget),
                lambda: extract_archive(
                    self.tar([("x", b"1234", "file")]),
                    self.root / "expanded",
                    budget=budget,
                ),
            ):
                with self.assertRaises(ValueError):
                    operation()

    def test_zip_symlink_and_traversal(self):
        for i, name in enumerate(("link", "../outside")):
            path = self.root / (str(i) + ".zip")
            with zipfile.ZipFile(path, "w") as out:
                member = zipfile.ZipInfo(name)
                if i == 0:
                    member.external_attr = 0o120777 << 16
                out.writestr(member, "payload")
            with self.assertRaises(ValueError):
                extract_archive(path, self.root / str(i))

    def test_json_duplicate_keys_and_overwrite(self):
        path = self.root / "data.json"
        path.write_bytes(b'{"a":1,"a":2}')
        with self.assertRaises(ValueError):
            read_json(path)
        with self.assertRaises(FileExistsError):
            write_json(path, {})

    def test_wrong_architecture_and_missing_elf_identity(self):
        for i, (machine, buildid) in enumerate(((183, b"x"), (62, b""))):
            with self.subTest(i=i), self.assertRaises(ValueError):
                elf_identity(elf(self.root / str(i), machine, buildid))

    def command(self, code, name="fixture", **kwargs):
        return run_owned(
            [sys.executable, "-B", "-c", code],
            self.root,
            child_environment(self.root / "state"),
            self.root / "logs",
            name,
            kwargs.pop("timeout", 2),
            **kwargs,
        )

    def test_process_preserves_complete_streams(self):
        result = self.command('print("fixture complete")')
        self.assertEqual(result["exit"], 0)
        self.assertTrue(result["ownedCleanupComplete"])
        self.assertEqual(
            (self.root / "logs/fixture.stdout").read_text(), "fixture complete\n"
        )

    def test_burst_log_hard_cap_and_cumulative_budget(self):
        self.command('print("a" * 31)', name="first", log_limit=40)
        with self.assertRaises(ValueError):
            self.command(
                'import os; os.write(1,b"x" * 100000)', name="second", log_limit=40
            )
        self.assertLessEqual(
            sum(p.stat().st_size for p in (self.root / "logs").glob("*.stdout")), 40
        )
        receipt = read_json(self.root / "logs/second.process.json")
        self.assertEqual(receipt["stop"], "LOG_LIMIT")

    def test_data_stdout_has_distinct_budget(self):
        with self.assertRaises(ValueError):
            self.command(
                'import os; os.write(1,b"x" * 100000)',
                data_stdout=True,
                data_limit=10,
                log_limit=5,
            )
        self.assertEqual((self.root / "logs/fixture.stdout").stat().st_size, 10)
        self.assertEqual(
            read_json(self.root / "logs/fixture.process.json")["stop"], "DATA_LIMIT"
        )

    def test_data_budget_accounting_crosses_supervisor_boundary(self):
        budget = IOBudget(self.root, 10)
        self.command(
            'import os; os.write(1,b"1234")',
            name="first",
            data_stdout=True,
            io_budget=budget,
        )
        self.assertEqual(budget.used, 4)
        with self.assertRaises(ValueError):
            self.command(
                'import os; os.write(1,b"1234567")',
                name="second",
                data_stdout=True,
                io_budget=budget,
            )
        self.assertEqual(budget.used, 4)
        self.assertEqual(
            read_json(self.root / "logs/second.process.json")["stop"], "DISK_LIMIT"
        )

    def test_closed_pipes_do_not_stop_timeout_or_disk_monitoring(self):
        with mock.patch(
            "qa3_native_io.shutil.disk_usage",
            side_effect=[
                shutil._ntuple_diskusage(100, 0, 100),
                shutil._ntuple_diskusage(100, 95, 5),
                *[shutil._ntuple_diskusage(100, 95, 5) for _ in range(1000)],
            ],
        ), self.assertRaises(ValueError):
            self.command(
                "import os,time; os.close(1); os.close(2); time.sleep(30)",
                minimum_free=10,
            )
        self.assertEqual(
            read_json(self.root / "logs/fixture.process.json")["stop"], "DISK_LIMIT"
        )

    def test_daemon_cleanup_does_not_kill_unowned_process(self):
        unowned = subprocess.Popen([
            sys.executable,
            "-c",
            "import time; time.sleep(30)",
        ])
        self.addCleanup(lambda: unowned.kill() if unowned.poll() is None else None)
        self.addCleanup(unowned.wait)
        code = (
            "import os,time; p=os.fork(); "
            "os._exit(0) if p else None; os.setsid(); "
            'open("daemon.pid","w").write(str(os.getpid())); time.sleep(30)'
        )
        result = self.command(code)
        self.assertTrue(result["ownedCleanupComplete"])
        self.assertIsNone(unowned.poll())
        pid = int((self.root / "daemon.pid").read_text())
        self.assertFalse(Path(f"/proc/{pid}").exists())
        unowned.terminate()
        unowned.wait(timeout=2)

    def test_group_memory_pids_and_oom_preconditions(self):
        proc = self.root / "cgroup"
        proc.write_text("0::/qa3-7-2-builder.scope\n")
        group = self.root / "groups/qa3-7-2-builder.scope"
        group.mkdir(parents=True)
        (group / "memory.max").write_text(str(28 * GIB))
        (group / "pids.max").write_text("512")
        (group / "cpu.max").write_text("800000 100000")
        (group / "memory.events").write_text("oom 0\noom_kill 0\n")
        value = verify_group_limits(
            "builder", {"run": 7, "attempt": 2}, self.root / "groups", proc
        )
        self.assertEqual(value["memoryMaxBytes"], 28 * GIB)
        for name, text in (
            ("cpu.max", "max 100000"),
            ("cpu.max", "900000 100000"),
            ("memory.max", "max"),
            ("memory.max", str(29 * GIB)),
            ("pids.max", "max"),
            ("pids.max", "513"),
            ("memory.events", "oom 1\noom_kill 0\n"),
        ):
            old = (group / name).read_text()
            (group / name).write_text(text)
            with self.subTest(name=name, text=text), self.assertRaises(ValueError):
                verify_group_limits(
                    "builder", {"run": 7, "attempt": 2}, self.root / "groups", proc
                )
            (group / name).write_text(old)
        with self.assertRaises(ValueError):
            verify_group_limits(
                "builder", {"run": 7, "attempt": 3}, self.root / "groups", proc
            )

    def test_pid_birth_snapshot_handles_exit_between_reads(self):
        calls = []

        def proc(pid):
            calls.append(pid)
            if pid == 20:
                return (10, "S", 9)
            return (1, "S", 5) if calls.count(10) == 1 else None

        owned = {10: 5}
        with mock.patch(
            "qa3_native_io.Path.iterdir", return_value=iter([Path("20")])
        ), mock.patch("qa3_native_io._proc", side_effect=proc):
            _descendants(owned, set())
            self.assertEqual(owned[20], 9)
            self.assertEqual(calls.count(10), 1)
            self.assertEqual(set(owned_alive(owned)), {20})

    def test_environment_has_no_controller_or_agent_tokens(self):
        with mock.patch.dict(
            os.environ,
            {
                "GITHUB_TOKEN": "fixture-token",
                "OPENAI_API_KEY": "fixture-key",
                "MOZCONFIG": "/untrusted",
                "PYTHONPATH": "/untrusted",
            },
        ):
            env = child_environment(self.root / "state")
        self.assertFalse(
            {"GITHUB_TOKEN", "OPENAI_API_KEY", "PYTHONPATH", "MOZCONFIG"} & set(env)
        )
        with self.assertRaises(ValueError):
            child_environment(self.root / "state2", {"GITHUB_TOKEN": "fixture"})


class ProcessFailureTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.original_popen = subprocess.Popen
        self.original_proc = qa3_native_io._proc
        self.processes = []
        self.owned = {}
        self.prior = {
            (int(p.name), info[2])
            for p in Path("/proc").iterdir()
            if p.name.isdecimal()
            and (info := self.original_proc(int(p.name)))
            and info[0] == os.getpid()
        }
        self.addCleanup(self.reap_fixture_children)

    def reap_fixture_children(self):
        deadline = time.monotonic() + 3
        while time.monotonic() < deadline:
            _descendants(self.owned, self.prior)
            for pid, info in owned_alive(self.owned).items():
                if info[1] != "Z":
                    try:
                        os.kill(pid, signal.SIGKILL)
                    except ProcessLookupError:
                        pass
                try:
                    os.waitpid(pid, os.WNOHANG)
                except ChildProcessError:
                    pass
            for process in self.processes:
                process.poll()
            if not owned_alive(self.owned):
                return
            time.sleep(0.01)
        self.fail("own bounded process fixture did not finish cleanup")

    def spawn(self, *args, **kwargs):
        process = self.original_popen(*args, **kwargs)
        self.processes.append(process)
        info = self.original_proc(process.pid)
        if info:
            self.owned[process.pid] = info[2]
        deadline = time.monotonic() + 2
        while not self.ready.exists() and time.monotonic() < deadline:
            if process.poll() is not None:
                self.fail("own process fixture exited before initialization")
            time.sleep(0.005)
        self.assertTrue(self.ready.exists(), "own process fixture was not ready")
        if wrapper := getattr(self, "pipe_wrapper", None):
            process.stdout = wrapper(process.stdout)
        return process

    def command(self, name="fixture", before_ready="", body="", **kwargs):
        self.ready = self.root / (name + ".ready")
        code = (
            "import json,os,signal,time\nfrom pathlib import Path\n"
            f"root=Path({str(self.root)!r})\n"
            "def identity(name):\n"
            "    data=Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()\n"
            "    (root/(name+'.identity')).write_text(json.dumps("
            "{'pid':os.getpid(),'birth':int(data[19])}))\n"
            "signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
            + before_ready
            + "\nidentity('leader')\n"
            + f"Path({str(self.ready)!r}).write_text('ready')\n"
            + (
                body
                or "while True:\n"
                "    try:\n"
                "        os.write(1,b'stdout\\n'); os.write(2,b'stderr\\n')\n"
                "    except OSError:\n"
                "        pass\n"
                "    time.sleep(0.01)\n"
            )
        )
        with mock.patch("qa3_native_io.subprocess.Popen", side_effect=self.spawn):
            return run_owned(
                [sys.executable, "-B", "-c", code],
                self.root,
                {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"},
                self.root / "logs",
                name,
                kwargs.pop("timeout", 1),
                memory=128 * 1024**2,
                log_limit=1024**2,
                **kwargs,
            )

    def check_failure(self, failure, operation, name="fixture"):
        with self.assertRaises(OSError) as caught:
            operation()
        self.assertEqual(type(caught.exception), type(failure))
        self.assertEqual(caught.exception.args, failure.args)
        self.assertFalse(
            owned_alive({
                process.pid: self.owned[process.pid] for process in self.processes
            })
        )
        self.assertFalse(
            any(
                self.original_proc(read_json(p)["pid"])
                for p in self.root.glob("*.identity")
            )
        )
        receipt = read_json(self.root / "logs" / (name + ".process.json"))
        self.assertTrue(receipt["ownedCleanupComplete"])
        self.assertEqual(receipt["error"]["type"], "OSError")
        self.assertEqual(receipt["error"]["errno"], failure.errno)
        self.assertEqual(receipt["error"]["message"], str(failure))
        self.assertIsNotNone(receipt["stop"])
        return receipt

    def sink_fault(self, failure, name="fixture", suffix=".stdout", when=lambda: True):
        original_open = Path.open

        class Sink:
            def __init__(self, stream):
                self.stream = stream
                self.failed = False

            def __getattr__(self, key):
                return getattr(self.stream, key)

            def __enter__(self):
                return self

            def __exit__(self, *args):
                return self.stream.__exit__(*args)

            def write(self, block):
                if when():
                    if self.failed:
                        raise OSError(errno.EIO, "secondary cleanup sink failure")
                    self.failed = True
                    raise failure
                return self.stream.write(block)

        def open_file(path, *args, **kwargs):
            stream = original_open(path, *args, **kwargs)
            if path == self.root / "logs" / (name + suffix):
                return Sink(stream)
            return stream

        return mock.patch.object(Path, "open", new=open_file)

    def test_initial_snapshot_failure_preserves_error_and_reaps(self):
        failure = OSError(errno.EIO, "initial process snapshot failed")
        failed = False

        def proc(pid):
            nonlocal failed
            if self.processes and pid == self.processes[-1].pid and not failed:
                failed = True
                raise failure
            return self.original_proc(pid)

        with mock.patch("qa3_native_io._proc", side_effect=proc):
            self.check_failure(failure, self.command)

    def test_pipe_setup_failure_preserves_error_and_reaps(self):
        failure = OSError(errno.EBADF, "pipe nonblocking setup failed")
        original = os.set_blocking

        def set_blocking(fd, blocking):
            if (
                self.processes
                and self.processes[-1].stdout
                and fd
                in {
                    self.processes[-1].stdout.fileno(),
                    self.processes[-1].stderr.fileno(),
                }
            ):
                raise failure
            return original(fd, blocking)

        with mock.patch("qa3_native_io.os.set_blocking", side_effect=set_blocking):
            self.check_failure(failure, self.command)

    def test_selector_registration_failure_preserves_error_and_reaps(self):
        factory = qa3_native_io.selectors.DefaultSelector
        for index in (1, 2):
            with self.subTest(index=index):
                name = "register-" + str(index)
                failure = OSError(errno.ENOMEM, "selector registration failed")
                selector = factory()
                self.addCleanup(selector.close)
                register = selector.register
                calls = 0

                def fail_register(*args):
                    nonlocal calls
                    calls += 1
                    if calls == index:
                        raise failure
                    return register(*args)

                with mock.patch.object(
                    selector, "register", side_effect=fail_register
                ), mock.patch(
                    "qa3_native_io.selectors.DefaultSelector", return_value=selector
                ):
                    self.check_failure(
                        failure, lambda: self.command(name=name), name=name
                    )

    def test_selector_read_failure_preserves_error_and_reaps(self):
        failure = OSError(errno.EIO, "selector read failed")
        selector = qa3_native_io.selectors.DefaultSelector()
        self.addCleanup(selector.close)
        with mock.patch.object(selector, "select", side_effect=failure), mock.patch(
            "qa3_native_io.selectors.DefaultSelector", return_value=selector
        ):
            self.check_failure(failure, self.command)

    def test_pipe_read_failure_preserves_error_and_reaps(self):
        failure = OSError(errno.EBADF, "pipe read failed")
        original_read = os.read

        def read(fd, size):
            if self.processes and fd in {
                self.processes[-1].stdout.fileno(),
                self.processes[-1].stderr.fileno(),
            }:
                raise failure
            return original_read(fd, size)

        with mock.patch("qa3_native_io.os.read", side_effect=read):
            self.check_failure(failure, self.command)

    def test_pipe_close_failure_still_reaps_and_saves_first_error(self):
        failure = OSError(errno.EIO, "initial pipe close failed")

        class Pipe:
            def __init__(self, stream):
                self.stream = stream

            def __getattr__(self, key):
                return getattr(self.stream, key)

            def close(self):
                self.stream.close()
                raise failure

        self.pipe_wrapper = Pipe
        receipt = self.check_failure(
            failure,
            lambda: self.command(body="os.close(1); os.close(2); time.sleep(30)\n"),
        )
        self.assertEqual(receipt["error"]["phase"], "RUN")

    def test_sink_errors_preserve_initial_failure_and_reap(self):
        for index, (code, suffix) in enumerate((
            (errno.ENOSPC, ".stdout"),
            (errno.EDQUOT, ".stderr"),
        )):
            with self.subTest(code=code):
                name = "sink-" + str(index)
                failure = OSError(code, "initial sink failure")
                with self.sink_fault(failure, name=name, suffix=suffix):
                    self.check_failure(
                        failure, lambda: self.command(name=name), name=name
                    )

    def test_cleanup_only_sink_failure_cannot_abort_kill_and_reap(self):
        failure = OSError(errno.ENOSPC, "cleanup sink failure")
        marker = self.root / "term-seen"
        handler = (
            "def term(signum,frame):\n"
            "    signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
            f"    Path({str(marker)!r}).write_text('term')\n"
            "    os.write(1,b'cleanup output\\n')\n"
            "signal.signal(signal.SIGTERM,term)\n"
        )
        with self.sink_fault(failure, when=marker.exists):
            receipt = self.check_failure(
                failure, lambda: self.command(before_ready=handler, timeout=0.1)
            )
        self.assertEqual(receipt["stop"], "TIMEOUT")
        self.assertTrue(marker.exists())

    def test_sink_failure_reaps_detached_and_late_fork_children(self):
        failure = OSError(errno.EDQUOT, "initial sink failure")
        unrelated = self.original_popen(
            [sys.executable, "-B", "-c", "import time; time.sleep(30)"],
            env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"},
            start_new_session=True,
        )
        self.processes.append(unrelated)
        self.owned[unrelated.pid] = self.original_proc(unrelated.pid)[2]
        before = (
            "p=os.fork()\n"
            "if p==0:\n"
            "    os.setsid(); identity('detached')\n"
            "    while True: time.sleep(0.01)\n"
            "while not (root/'detached.identity').exists(): time.sleep(0.001)\n"
            "def term(signum,frame):\n"
            "    signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
            "    p=os.fork()\n"
            "    if p==0:\n"
            "        os.setsid(); identity('late')\n"
            "        while True: time.sleep(0.01)\n"
            "signal.signal(signal.SIGTERM,term)\n"
        )
        with self.sink_fault(failure), self.assertRaises(OSError) as caught:
            self.command(before_ready=before)
        self.assertEqual(type(caught.exception), type(failure))
        self.assertEqual(caught.exception.args, failure.args)
        self.assertIsNone(unrelated.poll())
        self.assertTrue((self.root / "late.identity").exists())
        for path in self.root.glob("*.identity"):
            self.assertIsNone(self.original_proc(read_json(path)["pid"]))
        receipt = read_json(self.root / "logs/fixture.process.json")
        self.assertTrue(receipt["ownedCleanupComplete"])
        self.assertEqual(receipt["error"]["errno"], errno.EDQUOT)

    def test_cleanup_signal_error_cannot_abort_later_kill_and_reap(self):
        failure = OSError(errno.EMFILE, "initial cleanup signal failed")
        original_kill = os.kill
        failed = False

        def kill(pid, sig):
            nonlocal failed
            if self.processes and sig == signal.SIGTERM and not failed:
                failed = True
                raise failure
            return original_kill(pid, sig)

        with mock.patch("qa3_native_io.os.kill", side_effect=kill):
            receipt = self.check_failure(failure, lambda: self.command(timeout=0.1))
        self.assertEqual(receipt["error"]["phase"], "CLEANUP_SIGNAL")

    def test_expired_term_grace_still_kills_and_reaps(self):
        failure = OSError(errno.ENOSPC, "initial sink failure")
        original_time = time.monotonic
        original_killpg = os.killpg
        grace_calls = None

        def killpg(pid, sig):
            nonlocal grace_calls
            result = original_killpg(pid, sig)
            if self.processes and sig == signal.SIGTERM:
                grace_calls = 0
            return result

        def clock():
            nonlocal grace_calls
            if grace_calls is not None:
                grace_calls += 1
                if grace_calls == 2:
                    return original_time() + 3
            return original_time()

        with self.sink_fault(failure), mock.patch(
            "qa3_native_io.os.killpg", side_effect=killpg
        ), mock.patch("qa3_native_io.time.monotonic", side_effect=clock):
            self.check_failure(failure, self.command)

    def test_selector_close_error_does_not_replace_first_sink_failure(self):
        failure = OSError(errno.ENOSPC, "initial sink failure")
        selector = qa3_native_io.selectors.DefaultSelector()
        close = selector.close
        self.addCleanup(close)

        def fail_close():
            close()
            raise OSError(errno.EMFILE, "secondary selector close failure")

        with self.sink_fault(failure), mock.patch.object(
            selector, "close", side_effect=fail_close
        ), mock.patch("qa3_native_io.selectors.DefaultSelector", return_value=selector):
            receipt = self.check_failure(failure, self.command)
        self.assertEqual(
            receipt["cleanupErrors"]["RESOURCE_CLOSE"]["errno"], errno.EMFILE
        )

    def test_worker_sigterm_after_leader_exit_is_not_pass(self):
        original_kill = os.kill
        interrupted = False

        def kill(pid, sig):
            nonlocal interrupted
            if self.processes and sig == signal.SIGTERM and not interrupted:
                interrupted = True
                original_kill(os.getpid(), signal.SIGTERM)
            return original_kill(pid, sig)

        before = (
            "p=os.fork()\n"
            "if p==0:\n"
            "    os.setsid(); identity('detached')\n"
            "    while True: time.sleep(0.01)\n"
            "while not (root/'detached.identity').exists(): time.sleep(0.001)\n"
        )
        with mock.patch("qa3_native_io.os.kill", side_effect=kill):
            with self.assertRaises((ValueError, InterruptedError)):
                self.command(before_ready=before, body="os._exit(0)\n", timeout=3)
        receipt = read_json(self.root / "logs/fixture.process.json")
        self.assertEqual(receipt["exit"], 0)
        self.assertTrue(receipt["cancelled"])
        self.assertEqual(receipt["stop"], "CANCELLED")
        self.assertTrue(receipt["ownedCleanupComplete"])
        for path in self.root.glob("*.identity"):
            self.assertIsNone(self.original_proc(read_json(path)["pid"]))

    def test_caller_subreaper_zero_is_not_enabled(self):
        libc = ctypes.CDLL(None, use_errno=True)
        original = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(original), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 0, 0, 0, 0), 0)
        try:
            self.assertEqual(self.command(body="pass\n")["exit"], 0)
            current = ctypes.c_int()
            self.assertEqual(libc.prctl(37, ctypes.byref(current), 0, 0, 0), 0)
            self.assertEqual(current.value, 0)
        finally:
            self.reap_fixture_children()
            self.assertEqual(libc.prctl(36, original.value, 0, 0, 0), 0)

    def test_unrelated_late_adoption_and_caller_subreaper_are_preserved(self):
        libc = ctypes.CDLL(None, use_errno=True)
        original = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(original), 0, 0, 0), 0)
        self.assertEqual(libc.prctl(36, 1, 0, 0, 0), 0)
        ready = self.root / "fixture.ready"
        unrelated_code = (
            "import json,os,signal,time\nfrom pathlib import Path\n"
            f"root=Path({str(self.root)!r})\n"
            f"ready=Path({str(ready)!r})\n"
            "while not ready.exists(): time.sleep(0.005)\n"
            "pid=os.fork()\n"
            "if pid: os._exit(0)\n"
            "os.setsid(); signal.signal(signal.SIGTERM,signal.SIG_IGN)\n"
            "data=Path('/proc/self/stat').read_text().rsplit(')',1)[1].split()\n"
            "(root/'unrelated.json').write_text(json.dumps("
            "{'pid':os.getpid(),'birth':int(data[19])}))\n"
            "while True: time.sleep(0.01)\n"
        )
        unrelated = self.original_popen(
            [sys.executable, "-B", "-c", unrelated_code],
            env={"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"},
            start_new_session=True,
        )
        self.processes.append(unrelated)
        self.owned[unrelated.pid] = self.original_proc(unrelated.pid)[2]
        try:
            result = self.command(
                body=(
                    "while not (root/'unrelated.json').exists(): time.sleep(0.005)\n"
                    "time.sleep(0.1)\n"
                )
            )
            self.assertTrue(result["ownedCleanupComplete"])
            self.assertEqual(result["exit"], 0)
            info = read_json(self.root / "unrelated.json")
            state = self.original_proc(info["pid"])
            self.assertIsNotNone(state)
            self.assertEqual(state[2], info["birth"])
            self.owned[info["pid"]] = info["birth"]
            current = ctypes.c_int()
            self.assertEqual(libc.prctl(37, ctypes.byref(current), 0, 0, 0), 0)
            self.assertEqual(current.value, 1)
        finally:
            self.reap_fixture_children()
            self.assertEqual(libc.prctl(36, original.value, 0, 0, 0), 0)
        current = ctypes.c_int()
        self.assertEqual(libc.prctl(37, ctypes.byref(current), 0, 0, 0), 0)
        self.assertEqual(current.value, original.value)


if __name__ == "__main__":
    unittest.main()
