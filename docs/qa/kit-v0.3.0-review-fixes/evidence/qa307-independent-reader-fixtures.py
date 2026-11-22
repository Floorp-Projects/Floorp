"""Owned FakeDriver fixtures only: no browser, network, model or CLI invocation."""

import json
import os
import resource
import signal
import subprocess
import sys
import tempfile
import time
from pathlib import Path


KIT = Path("/workspace/qa-review-v030/fixes/kit-v030/Floorp_QA_Implementation_Kit_v0.3.0")
EVIDENCE = Path("/workspace/qa-review-v030/evidence")


def limits():
    resource.setrlimit(resource.RLIMIT_AS, (256 * 1024**2, 256 * 1024**2))
    resource.setrlimit(resource.RLIMIT_CPU, (8, 8))
    resource.setrlimit(resource.RLIMIT_FSIZE, (8 * 1024**2, 8 * 1024**2))


def child(nonblocking):
    sys.path.insert(0, str(KIT / "tools/qa"))
    import agent

    class FakeDriver:
        closed = False

        def __init__(self, repo, browser, output, subject):
            output.mkdir()

        def observe(self):
            return {"synthetic": True}

        def execute(self, action):
            return self.observe()

        def close(self):
            FakeDriver.closed = True

    agent.Driver = FakeDriver
    if nonblocking:
        os.set_blocking(sys.stdin.fileno(), False)
    before = os.get_blocking(sys.stdin.fileno())
    started = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="qa307-independent-campaign-") as td:
        root = Path(td)
        value = agent.execute_campaign(
            root, root / "unused", {"id": "synthetic-review"}, root / "out", None, 2, 1
        )
        value.update(
            fakeDriverClosed=FakeDriver.closed,
            stdinBlockingBefore=before,
            stdinBlockingAfter=os.get_blocking(sys.stdin.fileno()),
            campaignSeconds=round(time.monotonic() - started, 3),
        )
        print(json.dumps(value), flush=True)


def parent():
    cases = [
        ("partial-held-open", b"{", False, False, "budget-exhausted", 0),
        ("empty-eof", b"", True, False, "input-ended", 0),
        ("partial-eof", b"{", True, False, "finding-or-environment-error", 0),
        ("pipelined-frames", b'{"kind":"observe"}\n{"kind":"observe"}\n', False, False, "budget-exhausted", 2),
        ("already-nonblocking", b'{"kind":"observe"}\n{"kind":"observe"}\n', False, True, "budget-exhausted", 2),
        ("oversize-frame", b" " * 65536 + b"\n", True, False, "finding-or-environment-error", 0),
    ]
    results = []
    with tempfile.TemporaryDirectory(prefix="qa307-independent-reader-") as td:
        env = {
            "PATH": os.environ["PATH"],
            "LANG": "C.UTF-8",
            "TMPDIR": td,
            "PYTHONDONTWRITEBYTECODE": "1",
            "GIT_CONFIG_NOSYSTEM": "1",
            "GIT_CONFIG_GLOBAL": "/dev/null",
        }
        for name, payload, eof, nonblocking, status, steps in cases:
            retained = EVIDENCE / ("qa307-independent-reader-" + name + ".log")
            input_file = None
            if name == "oversize-frame":
                input_file = tempfile.TemporaryFile()
                input_file.write(payload)
                input_file.seek(0)
            with retained.open("xb") as stream:
                process = subprocess.Popen(
                    [sys.executable, "-B", str(Path(__file__).resolve()), "--child", str(int(nonblocking))],
                    stdin=input_file if input_file is not None else subprocess.PIPE,
                    stdout=stream,
                    stderr=subprocess.STDOUT,
                    env=env,
                    start_new_session=True,
                    preexec_fn=limits,
                )
                timed_out = False
                try:
                    if input_file is None:
                        process.stdin.write(payload)
                        process.stdin.flush()
                        if eof:
                            process.stdin.close()
                    try:
                        exit_code = process.wait(timeout=3)
                    except subprocess.TimeoutExpired:
                        timed_out = True
                        os.killpg(process.pid, signal.SIGKILL)
                        exit_code = process.wait(timeout=3)
                finally:
                    if process.stdin is not None and not process.stdin.closed:
                        process.stdin.close()
                    if process.poll() is None:
                        os.killpg(process.pid, signal.SIGKILL)
                        process.wait(timeout=3)
                    if input_file is not None:
                        input_file.close()
            raw = retained.read_text()
            value = json.loads(raw.splitlines()[-1]) if raw.splitlines() else {}
            accepted = (
                not timed_out
                and exit_code == 0
                and value.get("status") == status
                and value.get("completedSteps") == steps
                and value.get("fakeDriverClosed") is True
                and value.get("stdinBlockingBefore") == value.get("stdinBlockingAfter")
            )
            results.append({
                "case": name, "passed": accepted, "outerTimeout": timed_out,
                "exitCode": exit_code, "receipt": value, "rawLog": str(retained),
            })
    output = {
        "kind": "independent reviewed FakeDriver fixture execution",
        "nativeOrBrowserExecuted": False,
        "networkOrModelCalled": False,
        "credentialEnvironmentPassed": False,
        "resourceLimits": {"addressSpaceMiB": 256, "cpuSeconds": 8, "fileMiB": 8, "outerSeconds": 3},
        "cases": results,
    }
    path = EVIDENCE / "qa307-independent-reader-results.json"
    path.write_text(json.dumps(output, indent=2) + "\n")
    print(json.dumps({"file": str(path), "cases": len(results), "allPassed": all(r["passed"] for r in results)}))
    return 0 if all(r["passed"] for r in results) else 1


if __name__ == "__main__":
    if len(sys.argv) == 3 and sys.argv[1] == "--child":
        child(bool(int(sys.argv[2])))
    else:
        raise SystemExit(parent())
