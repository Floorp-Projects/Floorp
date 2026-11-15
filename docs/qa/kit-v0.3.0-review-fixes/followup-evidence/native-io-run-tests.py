import ctypes
import hashlib
import json
import os
import resource
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path("/workspace/qa-review-v030/fix-qa309/evidence")
SCRIPTS = Path("/workspace/qa-review-v030/repos/Floorp-Runtime/.github/workflows/scripts")
prefix, selection, *variant = sys.argv[1:]
if variant not in ([], ["original"]):
    raise ValueError("only the pinned original alternative is allowed")
if not prefix.startswith("native-io-") or "/" in prefix or selection not in {
    "test_qa3_native_io",
    "test_qa3_native_io.ProcessFailureTests",
    "test_qa3_native_io.SupervisorBoundaryTests",
    "test_qa3_native_io.SupervisorBoundaryTests.test_fork_failure_closes_both_fds_even_if_first_close_fails",
    "test_qa3_native_io.SupervisorBoundaryTests.test_parent_snapshot_error_closes_control_and_reaps",
    "test_qa3_native_io.SupervisorBoundaryTests.test_parent_reader_error_with_missing_snapshot_reaps",
    "test_qa3_native_io.SupervisorBoundaryTests.test_private_envelope_rejects_false_errors_extra_keys_and_bad_results",
    "test_qa3_native_io.SupervisorBoundaryTests.test_private_envelope_never_reconstructs_system_exit_zero",
    "test_qa3_native_io.ProcessFailureTests.test_unrelated_late_adoption_and_caller_subreaper_are_preserved",
    "test_qa3_native_io.ProcessFailureTests.test_caller_subreaper_zero_is_not_enabled",
    "test_qa3_native_io.ProcessFailureTests.test_cleanup_signal_error_cannot_abort_later_kill_and_reap",
    "test_qa3_native_io.ProcessFailureTests.test_pipe_close_failure_still_reaps_and_saves_first_error",
}:
    raise ValueError("fixed local unit selection required")
log = ROOT / (prefix + ".log")
receipt = ROOT / (prefix + ".json")
if log.exists() or receipt.exists():
    raise FileExistsError("preserve the first run under its original name")


def proc(pid):
    try:
        data = Path(f"/proc/{pid}/stat").read_text().rsplit(")", 1)[1].split()
        return int(data[1]), data[0], int(data[19])
    except (OSError, ValueError, IndexError):
        return None


owned = {}
prior = {
    (int(p.name), info[2])
    for p in Path("/proc").iterdir()
    if p.name.isdecimal()
    and (info := proc(int(p.name)))
    and info[0] == os.getpid()
}


def discover():
    for entry in Path("/proc").iterdir():
        if not entry.name.isdecimal():
            continue
        pid = int(entry.name)
        info = proc(pid)
        parent = proc(info[0]) if info and info[0] in owned else None
        if info and (
            info[0] == os.getpid() and (pid, info[2]) not in prior
            or parent and parent[2] == owned[info[0]]
        ):
            owned[pid] = info[2]


def limits():
    resource.setrlimit(resource.RLIMIT_AS, (512 * 1024**2, 32 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (40, 45))
    resource.setrlimit(resource.RLIMIT_FSIZE, (32 * 1024**2, 64 * 1024**3))
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))


if ctypes.CDLL(None, use_errno=True).prctl(36, 1, 0, 0, 0):
    raise OSError("own-fixture orphan recovery unavailable")
started = time.monotonic()
production_source = (SCRIPTS / "qa3_native_io.py").read_bytes()
tested_source = (ROOT / "native-io-original.py").read_bytes() if variant else production_source
test_source = (SCRIPTS / "test_qa3_native_io.py").read_bytes()
timed_out = False
cleaned = []
with tempfile.TemporaryDirectory(prefix="native-io-tmp-", dir=ROOT) as tmp:
    env = {
        "PATH": "/usr/bin:/bin",
        "LANG": "C.UTF-8",
        "TMPDIR": tmp,
        "PYTHONDONTWRITEBYTECODE": "1",
        "GIT_CONFIG_NOSYSTEM": "1",
        "GIT_CONFIG_GLOBAL": "/dev/null",
        "GIT_TERMINAL_PROMPT": "0",
    }
    with log.open("xb") as stream:
        command = [sys.executable, "-B", "-m", "unittest", "-v", selection]
        if variant:
            command = [
                sys.executable, "-B", "-c",
                "import importlib.util,sys,unittest; "
                f"spec=importlib.util.spec_from_file_location('qa3_native_io',{str(ROOT / 'native-io-original.py')!r}); "
                "module=importlib.util.module_from_spec(spec); "
                "sys.modules['qa3_native_io']=module; spec.loader.exec_module(module); "
                f"unittest.main(module=None,argv=['unittest','-v',{selection!r}])",
            ]
        child = subprocess.Popen(
            command,
            cwd=SCRIPTS,
            env=env,
            stdout=stream,
            stderr=subprocess.STDOUT,
            preexec_fn=limits,
            start_new_session=True,
        )
        info = proc(child.pid)
        if info:
            owned[child.pid] = info[2]
        try:
            while child.poll() is None:
                discover()
                if time.monotonic() - started > 90:
                    timed_out = True
                    break
                time.sleep(0.02)
        finally:
            deadline = time.monotonic() + 5
            while time.monotonic() < deadline:
                discover()
                alive = []
                for pid, birth in owned.items():
                    info = proc(pid)
                    if not info or info[2] != birth:
                        continue
                    alive.append(pid)
                    if info[1] != "Z":
                        try:
                            os.kill(pid, signal.SIGKILL)
                            cleaned.append(pid)
                        except ProcessLookupError:
                            pass
                    try:
                        os.waitpid(pid, os.WNOHANG)
                    except ChildProcessError:
                        pass
                child.poll()
                if not alive:
                    break
                time.sleep(0.02)
            child.wait(timeout=1)
leftovers = [pid for pid, birth in owned.items() if (info := proc(pid)) and info[2] == birth]
data = {
    "selection": selection,
    "exitCode": child.returncode,
    "outerTimeout": timed_out,
    "durationSeconds": round(time.monotonic() - started, 3),
    "limits": {"softASMiB": 512, "hardASGiB": 32, "cpuSeconds": 40,
               "softFileMiB": 32, "hardFileGiB": 64, "wallSeconds": 90},
    "outerRecoveryKilledOwnBirthCheckedPids": cleaned,
    "leftovers": leftovers,
    "sourceSha256": hashlib.sha256(production_source).hexdigest(),
    "testedVariant": "pinned-original" if variant else "working-fix",
    "testedSourceSha256": hashlib.sha256(tested_source).hexdigest(),
    "testSha256": hashlib.sha256(test_source).hexdigest(),
    "sourceUnchangedDuringRun": production_source == (SCRIPTS / "qa3_native_io.py").read_bytes(),
    "testsUnchangedDuringRun": test_source == (SCRIPTS / "test_qa3_native_io.py").read_bytes(),
    "logSha256": hashlib.sha256(log.read_bytes()).hexdigest(),
    "credentialsPassed": False,
    "nativeBrowserNetworkBuildExecuted": False,
}
with receipt.open("x") as stream:
    json.dump(data, stream, indent=2)
    stream.write("\n")
print(json.dumps(data))
sys.exit(1 if timed_out or leftovers else child.returncode)
