#!/usr/bin/env python3
"""Compile a bounded Foundation probe, not the complete Gecko host service."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.request

BASE = "7dd619742b25567e479eeda97ae071d10542ad0b"
PREFIX = "widget/cocoa/appshim/"
INPUTS = {
    "MacWebAppService.mm":"e6897fdd9a235fbdff6ce181ac9d7f61afee91ed9349d8238eda6a423c3f9059",
    "Protocol.h":"846e9d03db7cb05218f5fd201344d20e545e719d46a9fe5314e1d53951380755",
    "Protocol.mm":"c623732d3e2e93b5ce92caf2e26c5d0122def2c20825f4e7e01a097932da2369",
}
PATCH_SHA = "53a02038aaa2c765da162d19daaaa98d149fc3b97767f8a313a8358febe428c2"
PATCHED_SHA = "8f9a7909373d9985e91eaaa7a057f7acf8d04a400aadab660ce7affb6f3043c0"
HERE = Path(__file__).resolve().parent


def digest(data):
    return hashlib.sha256(data).hexdigest()


def require(condition, code):
    if not condition:
        raise ValueError(code)


def capture(argv, directory, timeout):
    with tempfile.TemporaryFile(dir=directory) as stream:
        result = subprocess.run(argv, stdout=stream, stderr=stream,
                                timeout=timeout, check=False)
        stream.seek(0)
        data = stream.read(65537)
    require(len(data) <= 65536, "command-output-limit")
    return result.returncode, data.decode("utf-8", errors="replace")


def fetch_source(destination):
    for name, expected in INPUTS.items():
        target = destination / PREFIX / name
        require(not target.exists(), "source-already-exists")
        url = "https://raw.githubusercontent.com/Floorp-Projects/Floorp-Runtime/" + BASE + "/" + PREFIX + name
        with urllib.request.urlopen(url, timeout=30) as response:
            data = response.read(65537)
        require(len(data) <= 65536 and digest(data) == expected, "source-digest")
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(data)


def fragments(text):
    method = re.findall(r"  void RememberCause\(App& aApp, NSString\* aReason\) \{\n.*?\n  \}", text, re.S)
    payload = re.findall(r'    Notify\(aApp\.id, @"disconnected", (@\{\n.*?\n    \})\);', text, re.S)
    require(len(method) == 1 and len(payload) == 1, "fragment-count")
    return method[0], payload[0]


def prepare(source, destination):
    require(not destination.exists(), "work-already-exists")
    patch = HERE / "preserve-shim-disconnect-cause.patch"
    require(digest(patch.read_bytes()) == PATCH_SHA, "patch-digest")
    for name, expected in INPUTS.items():
        path = source / PREFIX / name
        require(path.is_file() and not path.is_symlink(), "source-file")
        require(path.stat().st_size <= 65536 and digest(path.read_bytes()) == expected,
                "source-digest")
    destination.mkdir(parents=True)
    include = destination / PREFIX
    include.mkdir(parents=True)
    for name in INPUTS:
        shutil.copyfile(source / PREFIX / name, include / name)
    code, _ = capture(["git", "-C", str(destination), "init", "--quiet"], destination, 10)
    require(code == 0, "git-init")
    for check in [True, False]:
        argv = ["git", "-C", str(destination), "apply"]
        if check:
            argv.append("--check")
        code, _ = capture(argv + [str(patch)], destination, 10)
        require(code == 0, "patch-apply")
    service = (include / "MacWebAppService.mm").read_bytes()
    require(digest(service) == PATCHED_SHA, "patched-source-digest")
    method, payload = fragments(service.decode())
    template = (HERE / "cocoa_probe.mm.in").read_text()
    require(template.count("@@REMEMBER_CAUSE@@") == 1 and template.count("@@PAYLOAD@@") == 1,
            "template-placeholder")
    generated = template.replace("@@REMEMBER_CAUSE@@", method).replace("@@PAYLOAD@@", payload)
    require("@@" not in generated, "unresolved-placeholder")
    (destination / "Probe.mm").write_text(generated)
    return {"runtimeBase":BASE, "baseFileSHA256":INPUTS, "patchSHA256":PATCH_SHA,
            "patchedServiceSHA256":digest(service),
            "headerSHA256":digest((include / "DisconnectCause.h").read_bytes()),
            "rememberMethodSHA256":digest(method.encode()),
            "payloadExpressionSHA256":digest(payload.encode()),
            "generatedProbeSHA256":digest(generated.encode())}


def run(work, output):
    report = {"status":"failed", "scope":"source-derived-foundation-smoke",
              "completeHostTranslationUnitCompiled":False, "xulLinked":False,
              "nativeRuntimePatchExecuted":False, "originalFailureCauseResolved":False,
              "productOrDebugQualification":False, "cacheUsed":False,
              "rawCompilerOutputPublished":False}
    started = time.monotonic()
    stage = "platform"
    try:
        require(sys.platform == "darwin" and platform.machine() == "x86_64", "native-target")
        require(not work.exists(), "work-already-exists")
        work.mkdir(parents=True)
        stage = "fetch-source"
        fetch_source(work / "input")
        stage = "portable-guards"
        env = dict(os.environ, COCOA_PROBE_SOURCE_DIR=str(work / "input"), PYTHONDONTWRITEBYTECODE="1")
        with tempfile.TemporaryFile(dir=work) as stream:
            tests = subprocess.run([sys.executable, str(HERE / "test_cocoa_probe.py")],
                                   env=env, stdout=stream, stderr=stream, timeout=30, check=False)
        require(tests.returncode == 0, "portable-guards-failed")
        report["portableGuardTests"] = "passed"
        stage = "prepare"
        report["source"] = prepare(work / "input", work / "generated")
        generated = work / "generated"
        code, sdk = capture(["xcrun", "--sdk", "macosx", "--show-sdk-version"], work, 10)
        require(code == 0 and re.fullmatch(r"[0-9.]+\s*", sdk), "sdk-version")
        code, compiler = capture(["xcrun", "--find", "clang++"], work, 10)
        require(code == 0, "compiler-location")
        code, version = capture([compiler.strip(), "--version"], work, 10)
        require(code == 0, "compiler-version")
        report.update({"nativeArchitecture":platform.machine(), "sdkVersion":sdk.strip(),
                       "compilerVersion":version.splitlines()[0][:256]})
        code, sysroot = capture(["xcrun", "--sdk", "macosx", "--show-sdk-path"], work, 10)
        require(code == 0 and Path(sysroot.strip()).is_dir(), "sdk-path")
        include = generated / PREFIX
        flags = ["-std=c++17", "-fobjc-arc", "-Wall", "-Wextra", "-Werror", "-UNDEBUG", "-ferror-limit=5"]
        report["flags"] = flags
        stage = "compile"
        code, diagnostics = capture([compiler.strip(), *flags, "-isysroot", sysroot.strip(), "-I", str(include),
                                    str(generated / "Probe.mm"), str(include / "Protocol.mm"),
                                    "-framework", "Foundation", "-o", str(generated / "probe")], work, 90)
        report["compileExitCode"] = code
        report["compilerMarkers"] = [s for s in ["error:", "warning:", "not found", "undeclared identifier",
                                                 "cannot initialize", "no member", "unused"] if s in diagnostics]
        require(code == 0, "compile-failed")
        report["binarySHA256"] = digest((generated / "probe").read_bytes())
        stage = "native-probe"
        code, native = capture([str(generated / "probe")], work, 10)
        report["nativeExitCode"] = code
        require(code == 0, "native-probe-failed")
        proof = json.loads(native)
        require(proof == {"status":"passed", "checks":25, "scope":"source-derived-foundation-smoke"},
                "native-proof")
        report["nativeProof"] = proof
        report["status"] = "passed"
    except Exception as error:
        report["error"] = {"stage":stage, "type":type(error).__name__}
    report["elapsedSeconds"] = time.monotonic() - started
    report["workflow"] = {key:os.environ.get(key) for key in ["GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT"]}
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(report, indent=2) + "\n")
    return 0 if report["status"] == "passed" else 1


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    sys.exit(run(args.work, args.output))
