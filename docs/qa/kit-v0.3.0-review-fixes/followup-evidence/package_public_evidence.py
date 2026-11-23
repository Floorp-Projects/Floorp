"""Copy review evidence without temporary URL credentials or build caches.

Run once after the code and independent reports freeze. Existing destinations
are refused. The original local transport failures remain untouched.
"""

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
import stat
from urllib.parse import parse_qsl, urlsplit
import zipfile


PHASE = Path("/workspace/qa-review-v030/fix-qa309")
DOCS = Path("/workspace/qa-review-v030/worktrees/Floorp-kit-fixes/docs/qa/kit-v0.3.0-review-fixes")
EXCLUDED = {
    "floorp-ci-docs-raw-first.error.log",
    "floorp-ci-source-raw-first.error.log",
}


def digest(value):
    return hashlib.sha256(value).hexdigest()


def assert_public_bytes(data, name):
    text = data.decode("utf-8", errors="replace")
    for value in re.findall(r"https?://[^\s<>\"\x27]+", text):
        if any(
            key.lower() in {"sig", "token", "x-amz-signature", "x-goog-signature", "sv", "se", "skt"}
            for key, _ in parse_qsl(urlsplit(value).query)
        ):
            raise ValueError(f"temporary URL credential in {name}")
    if re.search(
        r"(?:gh[pousr]_[A-Za-z0-9]{30,}|github_pat_[A-Za-z0-9_]{30,}|-----BEGIN (?:RSA |EC |OPENSSH )?PRIVATE KEY-----)",
        text,
    ):
        raise ValueError(f"credential-shaped material in {name}")


destination = DOCS / "followup-evidence"
index_path = DOCS / "followup-evidence-index.json"
archive = DOCS / "FOLLOWUP_QA309_Evidence.zip"
receipt = DOCS / "followup-archive-validation.json"
if any(p.exists() or p.is_symlink() for p in (destination, index_path, archive, receipt)):
    raise ValueError("public evidence destination already exists")

sources = []
for path in sorted((PHASE / "evidence").iterdir()):
    if path.is_symlink():
        raise ValueError("symlink evidence refused")
    if path.is_file() and path.name not in EXCLUDED:
        sources.append((path, Path(path.name)))
for child in (PHASE / "evidence" / "floorp-source106-clean-commit-01").rglob("*"):
    if child.is_symlink():
        raise ValueError("symlink source receipt refused")
    if child.is_file():
        sources.append((child, child.relative_to(PHASE / "evidence")))
sources.append((PHASE / "fixtures" / "run_checks.py", Path("run_checks.py")))
sources.append((PHASE / "fixtures" / "package_public_evidence.py", Path("package_public_evidence.py")))

records = []
total = 0
for source, relative in sorted(sources, key=lambda item: item[1].as_posix()):
    data = source.read_bytes()
    total += len(data)
    if total > 32 * 1024**2 or len(data) > 4 * 1024**2:
        raise ValueError("public evidence byte budget exceeded")
    assert_public_bytes(data, relative.as_posix())
    records.append((relative, data, digest(data)))
if len(records) > 500 or len({r[0] for r in records}) != len(records):
    raise ValueError("duplicate or excessive evidence members")

destination.mkdir()
for relative, data, _ in records:
    target = destination / relative
    target.parent.mkdir(parents=True, exist_ok=True)
    with target.open("xb") as stream:
        stream.write(data)
    target.chmod(0o644)
index = {
    "phase": "QA309",
    "scope": "Code fixes, initial failures, final local checks, independent reviews and public CI observations",
    "files": [
        {"path": (Path("followup-evidence") / relative).as_posix(), "bytes": len(data), "sha256": sha}
        for relative, data, sha in records
    ],
    "exclusions": {
        "originalTemporaryUrlErrors": sorted(EXCLUDED),
        "replacement": "The separately named .error.redacted.log files retain the transport failures with URL queries removed.",
        "privateDownloadDescriptors": "All fixtures/private-* files are excluded; service URLs were not materialized (HTTP 403).",
        "buildCachesAndEphemeralFixtureTrees": "Excluded. The bounded fixture programs and their result logs remain in the copied evidence.",
    },
    "workingPaths": "Absolute /workspace paths identify the original consumer environment; use indexed relative paths to inspect this copy.",
    "nativeQualification": "UNVERIFIED",
    "productArtifactQualification": "UNVERIFIED",
    "publicationAuthorized": False,
}
index_data = (json.dumps(index, indent=2) + "\n").encode()
with index_path.open("xb") as stream:
    stream.write(index_data)

members = [
    ("FOLLOWUP_QA309.md", (DOCS / "FOLLOWUP_QA309.md").read_bytes()),
    ("followup-evidence-index.json", index_data),
    *[((Path("followup-evidence") / relative).as_posix(), data) for relative, data, _ in records],
]
with zipfile.ZipFile(archive, "x", compression=zipfile.ZIP_DEFLATED) as writer:
    for name, data in sorted(members):
        assert_public_bytes(data, name)
        item = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
        item.create_system = 3
        item.compress_type = zipfile.ZIP_DEFLATED
        item.external_attr = (stat.S_IFREG | 0o644) << 16
        writer.writestr(item, data)
with zipfile.ZipFile(archive) as reader:
    bad_crc = reader.testzip()
    if bad_crc is not None:
        raise ValueError(f"bad archive CRC: {bad_crc}")
    for item in reader.infolist():
        name = PurePosixPath(item.filename)
        if name.is_absolute() or ".." in name.parts or "\\" in item.filename or item.flag_bits & 1:
            raise ValueError("unsafe public archive path or encryption")
        if stat.S_IFMT(item.external_attr >> 16) != stat.S_IFREG:
            raise ValueError("nonregular public archive member")
    if sum(item.file_size for item in reader.infolist()) > 32 * 1024**2:
        raise ValueError("public archive expansion budget exceeded")
archive_data = archive.read_bytes()
result = {
    "archive": archive.name,
    "bytes": len(archive_data),
    "sha256": digest(archive_data),
    "entries": len(members),
    "expandedBytes": sum(len(data) for _, data in members),
    "crc": "PASS",
    "pathsTypesEncryption": "PASS",
    "credentialPatternScan": "PASS for temporary URL queries, GitHub-token shapes and private-key headers",
    "index": index_path.name,
    "indexSha256": digest(index_data),
    "originalLocalErrorsPreserved": True,
}
with receipt.open("x") as stream:
    json.dump(result, stream, indent=2)
    stream.write("\n")
print(json.dumps(result))
