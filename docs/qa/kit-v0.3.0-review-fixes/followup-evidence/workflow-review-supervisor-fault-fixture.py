import ctypes
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import signal
import subprocess
import sys
import tempfile
import time
from unittest.mock import patch


EVIDENCE = Path(__file__).resolve().parent
SNAPSHOT = EVIDENCE / "workflow-review-floorp-candidate-01-source.py"
spec = importlib.util.spec_from_file_location("source_review_snapshot", SNAPSHOT)
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
RESULT = {
    "sourceSha256": hashlib.sha256(SNAPSHOT.read_bytes()).hexdigest(),
    "nativeExecution": False,
    "network": False,
    "credentials": False,
    "limits": {"expectedMaximumWallSeconds": 40, "externalWallKillEnforced": False, "cpuSeconds": 12, "memoryMiB": 256, "fileMiB": 16},
    "cases": [],
}


def alive_with_birth(pid, birth):
    info = source._process_info(pid)
    return info is not None and info[2] == birth


def cleanup(record):
    for key in ("child", "supervisor"):
        pid, birth = record[key]
        if alive_with_birth(pid, birth):
            fd = os.pidfd_open(pid)
            try:
                if alive_with_birth(pid, birth):
                    signal.pidfd_send_signal(fd, signal.SIGKILL)
            except ProcessLookupError:
                pass
            finally:
                os.close(fd)
    deadline = time.monotonic() + 3
    while time.monotonic() < deadline:
        for key in ("child", "supervisor"):
            try:
                os.waitpid(record[key][0], os.WNOHANG)
            except ChildProcessError:
                pass
        if not any(alive_with_birth(*record[key]) for key in ("child", "supervisor")):
            return True
        time.sleep(0.02)
    return False


def fault_case(root, name, sig):
    marker = root / (name + ".json")
    command = """
import json,os,pathlib,signal,sys,time
def identity(pid):
    stat=pathlib.Path(f'/proc/{pid}/stat').read_text().rsplit(')',1)[1].split()
    return [pid,int(stat[19])]
supervisor=identity(os.getppid())
child=identity(os.getpid())
pathlib.Path(sys.argv[1]).write_text(json.dumps({'supervisor':supervisor,'child':child}))
fd=os.pidfd_open(supervisor[0])
try:
    if identity(supervisor[0]) == supervisor:
        signal.pidfd_send_signal(fd,int(sys.argv[2]))
finally:
    os.close(fd)
time.sleep(35)
"""
    began = time.monotonic()
    record = None
    result = {"name": name, "injectedSignal": sig, "acceptedPass": False}
    try:
        try:
            result["returnCode"] = source.execute(
                [sys.executable, "-I", "-B", "-c", command, str(marker), str(sig)],
                root / (name + ".log"), root, timeout=0.35,
            )
            result["acceptedPass"] = result["returnCode"] == 0
        except BaseException as error:
            result["raised"] = type(error).__name__
            result["error"] = str(error)
        result["elapsedSeconds"] = round(time.monotonic() - began, 3)
        if not marker.exists():
            raise AssertionError("fault command did not start")
        record = json.loads(marker.read_text())
        result["ownedBirths"] = record
        result["ownedChildAliveAfterExecute"] = alive_with_birth(*record["child"])
        result["supervisorAliveAfterExecute"] = alive_with_birth(*record["supervisor"])
    finally:
        if record is None and marker.exists():
            record = json.loads(marker.read_text())
        if record is not None:
            result["independentOwnedCleanupComplete"] = cleanup(record)
        RESULT["cases"].append(result)


def interrupted_cleanup_case(root):
    result = {"name": "SIGTERM during cleanup after exit code recorded"}
    real_cleanup = source._cleanup_owned

    def interrupt_then_cleanup(process, owned):
        os.kill(os.getpid(), signal.SIGTERM)
        return real_cleanup(process, owned)

    previous_term = signal.getsignal(signal.SIGTERM)
    previous_int = signal.getsignal(signal.SIGINT)
    read_fd, write_fd = os.pipe()
    try:
        with patch.object(source, "_cleanup_owned", interrupt_then_cleanup):
            source._execute_owned(write_fd, 1, [sys.executable, "-I", "-B", "-c", "pass"])
        write_fd = None
        with os.fdopen(read_fd, "rb") as stream:
            result["receipt"] = json.loads(stream.read(4097))
        read_fd = None
        result["acceptedPass"] = result["receipt"] == {"exitCode": 0, "error": None}
    finally:
        signal.signal(signal.SIGTERM, previous_term)
        signal.signal(signal.SIGINT, previous_int)
        if write_fd is not None:
            os.close(write_fd)
        if read_fd is not None:
            os.close(read_fd)
        RESULT["cases"].append(result)


def protocol_cases(root):
    real_popen = subprocess.Popen
    payloads = [
        ("empty", b""),
        ("truncated JSON", b'{"exitCode":0,'),
        ("missing fields", b'{}'),
        ("nonobject", b'[]'),
        ("boolean exit code", b'{"exitCode":true,"error":null}'),
        ("null exit code", b'{"exitCode":null,"error":null}'),
        ("error with zero exit", b'{"exitCode":0,"error":"incomplete cleanup"}'),
        ("oversized", b" " * 4097),
        ("valid nonzero", b'{"exitCode":124,"error":null}'),
        ("valid zero", b'{"exitCode":0,"error":null}'),
    ]
    for index, (name, payload) in enumerate(payloads):
        def fake_supervisor(argv, **kwargs):
            write_fd = kwargs["pass_fds"][0]
            command = "import os,sys; os.write(int(sys.argv[1]),bytes.fromhex(sys.argv[2]))"
            return real_popen(
                [sys.executable, "-I", "-B", "-c", command, str(write_fd), payload.hex()],
                **kwargs,
            )
        result = {"name": "protocol " + name, "acceptedPass": False}
        try:
            with patch.object(source.subprocess, "Popen", fake_supervisor):
                result["returnCode"] = source.execute(
                    ["unused-fixture-command"], root / f"protocol-{index}.log", root, 1,
                )
            result["acceptedPass"] = result["returnCode"] == 0
        except BaseException as error:
            result["raised"] = type(error).__name__
            result["error"] = str(error)
        RESULT["cases"].append(result)


libc = ctypes.CDLL(None, use_errno=True)
prior = ctypes.c_int()
if libc.prctl(37, ctypes.byref(prior), 0, 0, 0) != 0:
    raise OSError("fixture subreaper query failed")
if libc.prctl(36, 1, 0, 0, 0) != 0:
    raise OSError("fixture subreaper setup failed")
try:
    with tempfile.TemporaryDirectory(prefix="workflow-review-owned-") as directory:
        root = Path(directory)
        fault_case(root, "supervisor SIGKILL", int(signal.SIGKILL))
        fault_case(root, "supervisor SIGSTOP", int(signal.SIGSTOP))
        interrupted_cleanup_case(root)
        protocol_cases(root)
finally:
    libc.prctl(36, prior.value, 0, 0, 0)
    output = EVIDENCE / "workflow-review-supervisor-fault-results.json"
    output.write_text(json.dumps(RESULT, indent=2) + "\n")
    print(json.dumps(RESULT, indent=2))
