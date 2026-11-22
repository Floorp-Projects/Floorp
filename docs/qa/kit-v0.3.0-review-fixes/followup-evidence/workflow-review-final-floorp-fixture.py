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
from unittest.mock import patch


EVIDENCE = Path(__file__).resolve().parent
SNAPSHOT = EVIDENCE / "workflow-review-final-floorp-candidate-03-source.py"
spec = importlib.util.spec_from_file_location("final_source_review", SNAPSHOT)
source = importlib.util.module_from_spec(spec)
spec.loader.exec_module(source)
RESULT = {
    "sourceSha256": hashlib.sha256(SNAPSHOT.read_bytes()).hexdigest(),
    "nativeExecution": False,
    "network": False,
    "credentials": False,
    "cases": [],
}


def check(name, passed, details=None):
    RESULT["cases"].append({"name": name, "passed": bool(passed), "details": details})


def term_during_cleanup(root):
    real_cleanup = source._cleanup_owned

    def interrupt_then_cleanup(process, owned):
        os.kill(os.getpid(), signal.SIGTERM)
        return real_cleanup(process, owned)

    read_fd, write_fd = os.pipe()
    prior_term = signal.getsignal(signal.SIGTERM)
    prior_int = signal.getsignal(signal.SIGINT)
    try:
        with patch.object(source, "_cleanup_owned", interrupt_then_cleanup):
            source._execute_owned(write_fd, 2, [sys.executable, "-I", "-B", "-c", "pass"])
        write_fd = None
        with os.fdopen(read_fd, "rb") as stream:
            receipt = json.loads(stream.read(4097))
        read_fd = None
        check(
            "SIGTERM during cleanup cannot report success",
            receipt["exitCode"] == 0
            and receipt["ownedCleanupComplete"] is True
            and receipt["cleanupErrors"] == []
            and "interrupted" in receipt["error"],
            receipt,
        )
    finally:
        signal.signal(signal.SIGTERM, prior_term)
        signal.signal(signal.SIGINT, prior_int)
        if read_fd is not None:
            os.close(read_fd)
        if write_fd is not None:
            os.close(write_fd)


def protocol_cases(root):
    valid = {"exitCode": 0, "error": None, "cleanupErrors": [], "ownedCleanupComplete": True}
    items = [
        ("empty", b"", None),
        ("truncated JSON", b'{"exitCode":0,', None),
        ("non-object", b"[]", None),
        ("empty object", b"{}", None),
        ("oversized", b" " * 4097, None),
    ]
    for field in valid:
        obj = {k: v for k, v in valid.items() if k != field}
        items.append(("missing " + field, json.dumps(obj).encode(), None))
    mutations = [
        ("extra field", "unexpected", True),
        ("boolean exit code", "exitCode", True),
        ("null exit code", "exitCode", None),
        ("string exit code", "exitCode", "0"),
        ("float exit code", "exitCode", 0.0),
        ("numeric error", "error", 0),
        ("boolean error", "error", False),
        ("initial error after complete cleanup", "error", "initial failure"),
        ("cleanup error after complete recovery", "cleanupErrors", ["transient failure"]),
        ("cleanup error numeric item", "cleanupErrors", [1]),
        ("cleanup error non-list", "cleanupErrors", "failure"),
        ("cleanup errors null", "cleanupErrors", None),
        ("too many cleanup errors", "cleanupErrors", ["x"] * 5),
        ("incomplete cleanup", "ownedCleanupComplete", False),
        ("numeric cleanup complete one", "ownedCleanupComplete", 1),
        ("numeric cleanup complete zero", "ownedCleanupComplete", 0),
        ("null cleanup complete", "ownedCleanupComplete", None),
        ("string cleanup complete", "ownedCleanupComplete", "true"),
    ]
    for name, field, value in mutations:
        items.append((name, json.dumps(valid | {field: value}).encode(), None))
    for code in (0, 124, -9):
        items.append(("valid exit " + str(code), json.dumps(valid | {"exitCode": code}).encode(), code))
    real_popen = subprocess.Popen
    for index, (name, payload, expected) in enumerate(items):
        def fake_supervisor(argv, **kwargs):
            fd = kwargs["pass_fds"][0]
            command = "import os,sys; os.write(int(sys.argv[1]),bytes.fromhex(sys.argv[2]))"
            return real_popen([sys.executable, "-I", "-B", "-c", command, str(fd), payload.hex()], **kwargs)
        details = {}
        try:
            with patch.object(source.subprocess, "Popen", fake_supervisor):
                code = source.execute(["unused-fixture-command"], root / f"control-{index}.log", root, 2)
            details["exitCode"] = code
            check("private protocol " + name, expected is not None and code == expected, details)
        except BaseException as error:
            details.update({"raised": type(error).__name__, "error": str(error)})
            check("private protocol " + name, expected is None and isinstance(error, ValueError), details)


def live_fd_and_environment(root):
    real_popen = subprocess.Popen
    pipes = []

    def observe_pipe(argv, **kwargs):
        if kwargs.get("pass_fds"):
            pipes.append(os.readlink(f"/proc/self/fd/{kwargs['pass_fds'][0]}"))
        return real_popen(argv, **kwargs)

    command = """
import json,os,pathlib
targets=[]
for fd in pathlib.Path('/proc/self/fd').iterdir():
    try: targets.append(os.readlink(fd))
    except OSError: pass
print(json.dumps({'fdTargets':targets,'fakeCredentialPresent':'QA309_FAKE_CREDENTIAL' in os.environ,'nodeOptionsPresent':'NODE_OPTIONS' in os.environ,'envKeys':sorted(os.environ)}))
"""
    log = root / "live-fd.log"
    with patch.dict(os.environ, {"QA309_FAKE_CREDENTIAL": "fixture-only", "NODE_OPTIONS": "--throw-deprecation"}), patch.object(source.subprocess, "Popen", observe_pipe):
        code = source.execute([sys.executable, "-I", "-B", "-c", command], log, root, 2)
    outcome = json.loads(log.read_text())
    check("target cannot inherit the private control pipe", code == 0 and len(pipes) == 1 and pipes[0] not in outcome["fdTargets"], {"privatePipe": pipes[0], "childFdTargets": outcome["fdTargets"]})
    check("target cannot inherit fake credentials or runtime options", not outcome["fakeCredentialPresent"] and not outcome["nodeOptionsPresent"], {"envKeys": outcome["envKeys"]})


def bounded_error_control():
    message = source._bounded_error(OSError(('\\"日本語\n' * 5000)))
    payload = {"exitCode": 0, "error": message, "cleanupErrors": [message] * 4, "ownedCleanupComplete": True}
    encoded = json.dumps(payload, ensure_ascii=False).encode("utf-8")
    check("cleanup error strings and complete result fit the bounded pipe", len(message.encode("utf-8")) <= 256 and all(c.isprintable() for c in message) and len(encoded) < 4096, {"messageUtf8Bytes": len(message.encode("utf-8")), "controlUtf8Bytes": len(encoded)})


libc = ctypes.CDLL(None, use_errno=True)
prior = ctypes.c_int()
if libc.prctl(37, ctypes.byref(prior), 0, 0, 0) != 0:
    raise OSError("fixture subreaper query failed")
try:
    with tempfile.TemporaryDirectory(prefix="workflow-review-final-") as directory:
        root = Path(directory)
        term_during_cleanup(root)
        protocol_cases(root)
        live_fd_and_environment(root)
        bounded_error_control()
finally:
    libc.prctl(36, prior.value, 0, 0, 0)
    RESULT["passed"] = sum(c["passed"] for c in RESULT["cases"])
    RESULT["total"] = len(RESULT["cases"])
    RESULT["allPassed"] = RESULT["passed"] == RESULT["total"]
    (EVIDENCE / "workflow-review-final-floorp-results.json").write_text(json.dumps(RESULT, indent=2) + "\n")
    print(json.dumps(RESULT, indent=2))
sys.exit(0 if RESULT["allPassed"] else 1)
