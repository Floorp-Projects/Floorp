#!/usr/bin/env python3
"""Withhold Floorp offers without deleting DEBs or changing other records."""

import argparse
import gzip
import hashlib
import json
import re
import subprocess
import tempfile
from pathlib import Path


def withheld_versions(path):
    policy = json.loads(Path(path).read_text())
    if not isinstance(policy, dict) or set(policy) != {"schemaVersion", "versions"}:
        raise ValueError("Invalid PPA withholding policy")
    versions = policy["versions"]
    if policy["schemaVersion"] != 1 or not isinstance(versions, list):
        raise ValueError("Invalid PPA withholding policy schema")
    if any(not isinstance(v, str) or not re.fullmatch(r"\d+\.\d+\.\d+", v) for v in versions):
        raise ValueError("Withheld versions must be exact stable versions")
    if len(set(versions)) != len(versions):
        raise ValueError("Duplicate withheld version")
    return set(versions)


def paragraphs(data):
    text = data.decode("utf-8")
    if "\r" in text:
        raise ValueError("Unexpected CR in Debian metadata")
    result = []
    for paragraph in re.split(r"\n\n+", text.strip("\n")):
        if not paragraph:
            continue
        fields = {}
        previous = None
        for line in paragraph.splitlines():
            if line.startswith((" ", "\t")):
                if previous is None:
                    raise ValueError("Continuation without field")
                fields[previous] += "\n" + line
                continue
            name, separator, value = line.partition(": ")
            if not separator or not re.fullmatch(r"[A-Za-z0-9-]+", name) or name in fields:
                raise ValueError("Invalid or duplicate metadata field")
            fields[name] = value
            previous = name
        result.append((paragraph, fields))
    if not result:
        raise ValueError("Empty Packages index")
    return result


def product_version(debian_version):
    """Map the published stable DEB versions to their release filename version."""
    match = re.fullmatch(r"(\d+\.\d+\.\d+)(?:~build1)?", debian_version)
    if match is None:
        raise ValueError(f"Unsupported Floorp DEB version: {debian_version!r}")
    return match[1]


def candidate_version(data, expected_product_version):
    matches = [fields["Version"] for _, fields in paragraphs(data)
               if fields.get("Package") == "floorp"
               and product_version(fields.get("Version", "")) == expected_product_version]
    if len(matches) != 1:
        raise ValueError("Expected candidate must occur exactly once")
    return matches[0]


def latest_candidate(data):
    """Select a retained candidate using Debian ordering, never filename order."""
    records = [fields for _, fields in paragraphs(data) if fields.get("Package") == "floorp"]
    if not records:
        raise ValueError("No retained Floorp candidate")
    candidate = records[0]
    for record in records[1:]:
        comparison = subprocess.run(
            ["dpkg", "--compare-versions", record["Version"], "gt", candidate["Version"]],
            check=False, capture_output=True,
        )
        if comparison.returncode == 0:
            candidate = record
        elif comparison.returncode != 1:
            raise ValueError("Invalid Debian version comparison")
    return candidate


def filter_index(data, withheld, expected_version=None, expected_sha256=None):
    kept = []
    removed = []
    floorp = []
    for paragraph, fields in paragraphs(data):
        if fields.get("Package") == "floorp":
            version = fields.get("Version", "")
            release_version = product_version(version)
            filename = fields.get("Filename", "").removeprefix("./")
            if filename != f"floorp-{release_version}.deb":
                raise ValueError(f"Floorp DEB filename does not match its version: {filename!r}, Version={version!r}")
            if fields.get("Architecture") != "amd64":
                raise ValueError("Unexpected Floorp architecture in amd64 index")
            if not re.fullmatch(r"[0-9a-f]{64}", fields.get("SHA256", "")):
                raise ValueError("Floorp record has no valid SHA256")
            if not fields.get("Size", "").isdigit() or int(fields["Size"]) <= 0:
                raise ValueError("Floorp record has no valid byte size")
            if release_version in withheld:
                removed.append(filename)
                continue
            floorp.append(fields)
        kept.append(paragraph)
    if not floorp:
        raise ValueError("Withholding would remove all Floorp candidates")
    if expected_version is not None:
        matches = [f for f in floorp if product_version(f["Version"]) == expected_version]
        if len(matches) != 1:
            raise ValueError("Expected candidate must occur exactly once")
        if not re.fullmatch(r"[0-9a-f]{64}", expected_sha256 or ""):
            raise ValueError("Expected candidate requires the Release asset SHA256")
        if matches[0]["SHA256"] != expected_sha256:
            raise ValueError("Expected candidate differs from the verified Release asset")
        for record in floorp:
            comparison = subprocess.run(
                ["dpkg", "--compare-versions", record["Version"], "gt", matches[0]["Version"]],
                check=False, capture_output=True,
            )
            if comparison.returncode == 0:
                raise ValueError("A version newer than the expected candidate remains")
            if comparison.returncode != 1:
                raise ValueError("Invalid Debian version comparison")
    return ("\n\n".join(kept) + "\n\n").encode(), removed


def verify_release(directory):
    directory = Path(directory)
    release = directory.joinpath("Release").read_text()
    match = re.search(r"^SHA256:\n((?:[ \t]+[^\n]+\n)+)", release, re.MULTILINE)
    if match is None:
        raise ValueError("Signed Release has no SHA256 section")
    entries = {}
    for line in match[1].splitlines():
        digest, size, filename = line.split()
        if filename in entries:
            raise ValueError("Duplicate Release checksum")
        entries[filename] = (digest, size)
    for name in ["Packages", "Packages.gz"]:
        data = directory.joinpath(name).read_bytes()
        if entries.get(name) != (hashlib.sha256(data).hexdigest(), str(len(data))):
            raise ValueError(f"Signed Release checksum mismatch: {name}")
    if gzip.decompress(directory.joinpath("Packages.gz").read_bytes()) != directory.joinpath("Packages").read_bytes():
        raise ValueError("Compressed and uncompressed indexes differ")


def verify_apt_candidates(data, expected_version, installed_version):
    """Read the authenticated index with APT; simulation never installs anything."""
    expected_debian_version = candidate_version(data, expected_version)
    with tempfile.TemporaryDirectory(prefix="floorp-apt-policy-") as temporary:
        root = Path(temporary)
        lists = root.joinpath("lists")
        lists.mkdir()
        lists.joinpath("ppa.floorp.app_amd64_._Packages").write_bytes(data)
        sources = root.joinpath("sources.list")
        sources.write_text("deb https://ppa.floorp.app/amd64 ./\n")
        status = root.joinpath("status")
        options = []
        for key, value in {
            "Dir::Etc::sourcelist": sources,
            "Dir::Etc::sourceparts": "-",
            "Dir::Etc::parts": "-",
            "Dir::Etc::main": "-",
            "Dir::State::status": status,
            "Dir::State::lists": lists,
            "Dir::Cache::pkgcache": root.joinpath("pkgcache.bin"),
            "Dir::Cache::srcpkgcache": root.joinpath("srcpkgcache.bin"),
            "Dir::Cache::archives": root.joinpath("archives"),
        }.items():
            options.extend(["-o", f"{key}={value}"])
        for installed, expected in [(None, expected_debian_version), (installed_version, installed_version)]:
            status.write_text("" if installed is None else (
                "Package: floorp\nStatus: install ok installed\nArchitecture: amd64\n"
                f"Version: {installed}\nDescription: Installed Floorp retained by APT\n\n"
            ))
            result = subprocess.run(["apt-cache", *options, "policy", "floorp"], check=True, text=True, capture_output=True)
            match = re.search(r"^\s*Candidate: (\S+)$", result.stdout, re.MULTILINE)
            if match is None or match[1] != expected:
                raise ValueError(f"APT selected an unexpected candidate: {result.stdout}")
            print(result.stdout.strip())
            if installed is not None:
                simulation = subprocess.run(["apt-get", *options, "--simulate", "upgrade"], check=True, text=True, capture_output=True)
                if re.search(r"^Inst floorp\b", simulation.stdout, re.MULTILINE):
                    raise ValueError("APT upgrade simulation would replace installed Floorp")
                print(simulation.stdout.strip())


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    verify = commands.add_parser("verify-release")
    verify.add_argument("directory")
    reject = commands.add_parser("check-new")
    reject.add_argument("policy")
    reject.add_argument("version")
    apt = commands.add_parser("apt-candidates")
    apt.add_argument("packages")
    apt.add_argument("expected_version")
    apt.add_argument("installed_version")
    filt = commands.add_parser("filter")
    filt.add_argument("policy")
    filt.add_argument("packages")
    filt.add_argument("--expected-version")
    filt.add_argument("--expected-sha256")
    filt.add_argument("--report")
    args = parser.parse_args()
    if args.command == "verify-release":
        verify_release(args.directory)
    elif args.command == "apt-candidates":
        verify_apt_candidates(Path(args.packages).read_bytes(), args.expected_version, args.installed_version)
    elif args.command == "check-new":
        if product_version(args.version) in withheld_versions(args.policy):
            raise ValueError(f"Refusing to publish withheld Floorp {args.version}")
    else:
        path = Path(args.packages)
        before = path.read_bytes()
        after, removed = filter_index(before, withheld_versions(args.policy), args.expected_version, args.expected_sha256)
        report = {
            "beforeSHA256": hashlib.sha256(before).hexdigest(),
            "afterSHA256": hashlib.sha256(after).hexdigest(),
            "withheldDEBs": removed,
            "retainedDEBs": [f["Filename"] for _, f in paragraphs(after) if f.get("Package") == "floorp"],
            "expectedProductVersion": args.expected_version,
            "expectedCandidate": candidate_version(after, args.expected_version) if args.expected_version else None,
        }
        path.write_bytes(after)
        if args.report:
            Path(args.report).write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps(report))


if __name__ == "__main__":
    main()
