import argparse
import ast
import gzip
import hashlib
import io
import json
import os
from pathlib import Path, PurePosixPath
import re
import resource
import stat
import struct
import sys
import tarfile
import time
import zlib

from contract import ARTIFACT, ENTRY, MAX_GZIP, REPOSITORY, RUN, SOURCE, ZIP_SHA, require

MAX_EXPANDED = 64 * 1024**3
MAX_MEMBERS = 250000
MAX_TEXT = 2 * 1024**2
ROOT = "obj-x86_64-apple-darwin"
BOOL_KEYS = {"MOZ_DEBUG", "MOZ_PROFILE_GENERATE", "MOZ_PROFILE_USE", "ENABLE_TESTS",
             "MOZ_DEBUG_SYMBOLS", "MOZ_ASAN", "MOZ_TSAN", "MOZ_LTO"}
KEYS = BOOL_KEYS | {"target", "host", "OS_TARGET", "CPU_ARCH", "CC_TYPE", "CC_VERSION",
    "CXX_VERSION", "RUSTC_VERSION", "MACOSX_DEPLOYMENT_TARGET", "MACOSX_SDK_VERSION",
    "MOZ_BUILD_APP", "CC", "CXX", "LD", "RUSTC", "HOST_CC", "HOST_CXX", "RUST_TARGET"}
FIXED = {"config.status", "mozinfo.json", "config/autoconf.mk", ".mozconfig.json",
    "mozconfig.json", "backend.RecursiveMakeBackend", "backend.RecursiveMakeBackend.in",
    "widget/cocoa/Makefile", "widget/cocoa/backend.mk", "toolkit/library/build/Makefile",
    "toolkit/library/build/backend.mk", "dist/bin/application.ini", "dist/bin/platform.ini",
    "dist/include/mozilla-config.h", "dist/include/source-repo.h", "dist/bin/XUL",
    "toolkit/library/build/XUL"}


def sha(data):
    return hashlib.sha256(data).hexdigest()


def path_in_objdir(name):
    while name.startswith("./"):
        name = name[2:]
    path = PurePosixPath(name)
    require(not path.is_absolute() and ".." not in path.parts and "\\" not in name and
            "\x00" not in name and path.parts and path.parts[0] == ROOT, "tar-path")
    return "/".join(path.parts[1:])


def typed(key, value):
    text = str(value).strip()
    if key in BOOL_KEYS:
        if value is True or text in ("1", "true", "True", "yes"):
            return True
        if value is False or value is None or text in ("", "0", "false", "False", "no"):
            return False
    if key in {"CC", "CXX", "LD", "RUSTC", "HOST_CC", "HOST_CXX"}:
        return {"valueSHA256": sha(text.encode()),
                "locationClass": "mozbuild" if "/.mozbuild/" in text else
                    "system" if text.startswith("/usr/") else "other-or-relative"}
    if text in {"x86_64-apple-darwin", "aarch64-apple-darwin", "x86_64-pc-linux-gnu",
                "x86_64-unknown-linux-gnu", "Darwin", "Linux", "x86_64", "aarch64",
                "clang", "gcc", "clang-cl", "browser"}:
        return text
    if re.fullmatch(r"[0-9]+(?:\.[0-9]+){0,3}", text):
        return text
    return {"valueSHA256": sha(text.encode()), "valuePublished": False}


def config_values(relative, data):
    text = data.decode("utf-8", errors="replace")
    values = {}
    if relative.startswith("config.statusd/"):
        key = relative.rsplit("/", 1)[-1]
        values[key] = typed(key, json.loads(text))
    elif relative == "mozinfo.json":
        source = json.loads(text)
        for key in ("debug", "asan", "tsan", "coverage", "pgo", "artifact", "bits"):
            if key in source and isinstance(source[key], (bool, int)):
                values[key] = source[key]
        for key in ("os", "processor", "appname"):
            if source.get(key) in ("mac", "linux", "x86_64", "aarch64", "floorp", "browser"):
                values[key] = source[key]
    elif relative == "config.status":
        tree = ast.parse(text)
        for node in tree.body:
            if not isinstance(node, ast.Assign):
                continue
            for target in node.targets:
                if isinstance(target, ast.Name) and target.id in ("substs", "defines"):
                    try:
                        mapping = ast.literal_eval(node.value)
                    except (ValueError, TypeError):
                        continue
                    if isinstance(mapping, dict):
                        for key in KEYS & mapping.keys():
                            values[key] = typed(key, mapping[key])
                elif isinstance(target, ast.Name) and target.id in ("topsrcdir", "topobjdir"):
                    try:
                        value = ast.literal_eval(node.value)
                    except (ValueError, TypeError):
                        continue
                    if isinstance(value, str):
                        values[target.id] = {"valueSHA256": sha(value.encode()),
                            "githubRuntimeWorkspaceLayout":
                                "/work/Floorp-Runtime/Floorp-Runtime" in value}
    elif relative == "config/autoconf.mk":
        for key, value in re.findall(r"^([A-Za-z0-9_]+)\s*:?=\s*(.*)$", text, re.M):
            if key in KEYS:
                values[key] = typed(key, value)
    elif relative == "dist/include/mozilla-config.h":
        for key in BOOL_KEYS:
            values[key + "Defined"] = bool(re.search(r"^\s*#\s*define\s+" + key + r"\b", text, re.M))
    elif relative.endswith("application.ini") or relative.endswith("platform.ini"):
        for key, value in re.findall(r"^(BuildID|SourceStamp|Version)=(.*)$", text, re.M):
            if re.fullmatch(r"[0-9]{14}|[0-9a-f]{12,40}|[0-9]+(?:\.[0-9]+){1,3}", value):
                values[key] = value
    return values


def selected(relative):
    parts = relative.split("/")
    return relative in FIXED or (
        len(parts) == 3 and parts[0] == "config.statusd" and
        parts[1] in {"substs", "defines"} and parts[2] in KEYS
    ) or bool(re.fullmatch(
        r"widget/cocoa/(?:MacWebAppService\.(?:o|mm\.o)|libwidget_cocoa\.a(?:\.desc)?)",
        relative
    )) or bool(re.fullmatch(
        r"widget/cocoa/(?:\.deps/)?MacWebAppService\.(?:o\.pp|mm\.o\.d)", relative
    )) or bool(re.fullmatch(
        r"toolkit/library/rust/(?:(?:x86_64-apple-darwin/)?(?:release|debug)/)?libgkrust\.a",
        relative
    ))


def binary_header(data):
    if len(data) >= 16 and data[:4] == b"\xcf\xfa\xed\xfe":
        _, cpu, _, kind = struct.unpack("<4I", data[:16])
        return {"format": "Mach-O64", "architecture":
                "x86_64" if cpu == 0x01000007 else "aarch64" if cpu == 0x0100000c else "other",
                "fileType": kind}
    if data.startswith(b"BC\xc0\xde"):
        return {"format": "LLVM-bitcode"}
    if data.startswith(b"!<arch>\n"):
        return {"format": "archive"}
    if data.startswith(b"\x7fELF"):
        return {"format": "ELF"}
    return {"format": "other"}


class Counter:
    def __init__(self, stream, started):
        self.stream, self.started, self.count = stream, started, 0

    def read(self, size):
        require(time.monotonic() - self.started < 480, "scan-deadline")
        data = self.stream.read(size)
        self.count += len(data)
        require(self.count <= MAX_EXPANDED, "expanded-byte-budget")
        return data


class BoundedInfo(tarfile.TarInfo):
    def _proc_member(self, archive):
        require(self.size <= MAX_EXPANDED and self.type != tarfile.GNUTYPE_SPARSE,
                "tar-member-budget")
        if self.type in (tarfile.XHDTYPE, tarfile.XGLTYPE,
                         tarfile.GNUTYPE_LONGNAME, tarfile.GNUTYPE_LONGLINK):
            require(self.size <= 1024**2, "tar-metadata-budget")
        return super()._proc_member(archive)


def inspect(archive_path, started):
    counts = {key: 0 for key in ("members", "regularFiles", "directories", "symlinks", "hardlinks")}
    files, configs, header_entries, seen = [], {}, [], set()
    with archive_path.open("rb") as compressed, gzip.GzipFile(fileobj=compressed) as uncompressed:
        counter = Counter(uncompressed, started)
        with tarfile.open(fileobj=counter, mode="r|", tarinfo=BoundedInfo) as bundle:
            while True:
                member = bundle.next()
                if member is None:
                    break
                bundle.members.clear()
                counts["members"] += 1
                require(counts["members"] <= MAX_MEMBERS and
                        not any(key.startswith("GNU.sparse.") for key in member.pax_headers), "tar-entry-budget")
                relative = path_in_objdir(member.name)
                require(relative not in seen, "duplicate-tar-path")
                seen.add(relative)
                require(member.isfile() or member.isdir() or member.issym() or member.islnk(), "tar-entry-type")
                header = relative.startswith("dist/include/")
                if member.isdir():
                    counts["directories"] += 1
                    continue
                if member.issym() or member.islnk():
                    kind = "symlink" if member.issym() else "hardlink"
                    counts[kind + "s"] += 1
                    if member.islnk():
                        path_in_objdir(member.linkname)
                    if header:
                        header_entries.append((relative, kind, sha(member.linkname.encode())))
                        require(len(header_entries) <= 50000, "header-budget")
                    if selected(relative):
                        require(len(files) < 128, "selected-file-budget")
                        files.append({"path": relative, "kind": kind,
                                      "targetSHA256": sha(member.linkname.encode()),
                                      "externalAbsoluteTarget": member.linkname.startswith("/")})
                    continue
                counts["regularFiles"] += 1
                wanted = selected(relative)
                if not wanted and not header:
                    continue
                hasher = hashlib.sha256()
                capture = member.size <= MAX_TEXT and wanted
                chunks, first = [], b""
                with bundle.extractfile(member) as stream:
                    while True:
                        data = stream.read(1024**2)
                        if not data:
                            break
                        require(time.monotonic() - started < 480, "scan-deadline")
                        hasher.update(data)
                        if not first:
                            first = data[:32]
                        if capture:
                            chunks.append(data)
                digest = hasher.hexdigest()
                if header:
                    header_entries.append((relative, "regular", digest))
                    require(len(header_entries) <= 50000, "header-budget")
                if wanted:
                    require(len(files) < 128, "selected-file-budget")
                    item = {"path": relative, "kind": "regular", "bytes": member.size,
                            "sha256": digest, "mtime": member.mtime}
                    if relative.endswith((".o", "/XUL", ".a")):
                        item["binary"] = binary_header(first)
                    files.append(item)
                    if capture:
                        data = b"".join(chunks)
                        if relative in {"config.status", "mozinfo.json", "config/autoconf.mk",
                                        "dist/include/mozilla-config.h", "dist/bin/application.ini",
                                        "dist/bin/platform.ini"} or relative.startswith("config.statusd/"):
                            configs[relative] = config_values(relative, data)
                        if relative.endswith(".pp"):
                            item["mentionsMozillaConfigHeader"] = b"mozilla-config.h" in data
            trailing = 0
            while True:
                data = bundle.fileobj.read(1024**2)
                if not data:
                    break
                trailing += len(data)
                require(trailing <= 65536 and not any(data), "tar-trailing-data")
        expanded = counter.count
    header_digest = hashlib.sha256()
    for row in sorted(header_entries):
        header_digest.update(json.dumps(row, separators=(",", ":")).encode() + b"\n")
    return {"counts": counts, "expandedBytesRead": expanded, "gzipConsumedToEOF": True,
            "gzipCRCVerified": True, "tarHeadersParsed": True, "filesExtracted": False,
            "selectedFiles": files, "configuration": configs,
            "headerInventory": {"entries": len(header_entries), "sha256": header_digest.hexdigest(),
                                "externalSymlinkContentsResolved": False}}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--preflight", type=Path, required=True)
    parser.add_argument("--payload", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    resource.setrlimit(resource.RLIMIT_AS, (1024**3, 1024**3))
    resource.setrlimit(resource.RLIMIT_CPU, (480, 490))
    started = time.monotonic()
    report = {"status": "failed", "scope": "objdir-read-only-inspection", "artifactId": ARTIFACT,
              "sourceRun": RUN, "sourceAttempt": 1, "sourceCommit": SOURCE,
              "eligibleForIncrementalBuild": False, "completeHostTUCompiled": False,
              "xulLinked": False, "authenticationExecuted": False, "signingPerformed": False,
              "originalFailureCauseResolved": False, "productOrDebugQualification": False,
              "fullZIPDigestRequiresActionLogGate": True}
    stage = "downloaded-layout"
    try:
        preflight = json.loads(args.preflight.read_bytes())
        require(preflight["status"] == "headers-verified" and
                preflight["providerZIPDigest"] == "sha256:" + ZIP_SHA and
                preflight["artifactId"] == ARTIFACT, "preflight-binding")
        entries = list(args.payload.iterdir())
        archive = args.payload / ENTRY
        require(len(entries) == 1 and entries[0] == archive and not archive.is_symlink(), "downloaded-entries")
        info = archive.stat()
        require(stat.S_ISREG(info.st_mode) and info.st_nlink == 1 and
                info.st_size == preflight["zipHeaders"]["entryBytes"] and info.st_size <= MAX_GZIP,
                "downloaded-file")
        stage = "zip-entry-integrity"
        digest, crc = hashlib.sha256(), 0
        with archive.open("rb") as stream:
            while True:
                data = stream.read(1024**2)
                if not data:
                    break
                require(time.monotonic() - started < 480, "scan-deadline")
                digest.update(data)
                crc = zlib.crc32(data, crc)
        require(crc == preflight["zipHeaders"]["entryCRC32"], "zip-entry-crc")
        report["zipEntry"] = {"bytes": info.st_size, "sha256": digest.hexdigest(),
                              "crc32": crc, "crcMatchesZIPDirectory": True}
        stage = "tar-read-only"
        report["archive"] = inspect(archive, started)
        report["status"] = "inspected"
        report["compatibilityVerdict"] = "NO_GO_PENDING_CONFIG_DEPENDENCY_AND_TRANSPORT_REVIEW"
    except Exception as error:
        report["error"] = {"stage": stage, "type": type(error).__name__}
    report["elapsedSeconds"] = time.monotonic() - started
    report["workflow"] = {key: os.environ.get(key) for key in
                          ("GITHUB_SHA", "GITHUB_RUN_ID", "GITHUB_RUN_ATTEMPT")}
    report["inspectorSHA256"] = sha(Path(__file__).read_bytes())
    text = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if len(text.encode()) > 65536:
        report = {"status": "failed", "scope": "objdir-read-only-inspection",
                  "error": {"stage": "report-budget", "type": "ValueError"}}
        text = json.dumps(report) + "\n"
    args.output.write_text(text)
    return 0 if report["status"] == "inspected" and len(text.encode()) <= 65536 else 1


if __name__ == "__main__":
    sys.exit(main())
