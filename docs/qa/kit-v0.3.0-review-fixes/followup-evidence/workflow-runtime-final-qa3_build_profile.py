#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0

import argparse
import hashlib
import re
import shlex
import sys
from pathlib import Path

TARGETS = {
    ("linux", "x86_64"): "x86_64-pc-linux-gnu",
    ("linux", "aarch64"): "aarch64-unknown-linux-gnu",
    ("windows", "x86_64"): "x86_64-pc-windows-msvc",
    ("mac", "x86_64"): "x86_64-apple-darwin",
    ("mac", "aarch64"): "aarch64-apple-darwin",
}


def boolean(value):
    if value not in {"true", "false"}:
        raise ValueError("boolean must be true or false")
    return value == "true"


def profile(
    platform,
    arch,
    debug,
    pgo,
    mode="",
    artifact="",
    *,
    allow_legacy_debug_pgo=False,
):
    if (
        (platform, arch) not in TARGETS
        or type(debug) is not bool
        or type(pgo) is not bool
        or type(allow_legacy_debug_pgo) is not bool
    ):
        raise ValueError("unsupported target or non-boolean build mode")
    if not isinstance(mode, str) or not isinstance(artifact, str):
        raise ValueError("PGO mode and profile artifact must be strings")
    if debug and pgo and not allow_legacy_debug_pgo:
        raise ValueError("Debug+PGO requires explicit noncanonical legacy policy")
    if not pgo and (mode or artifact):
        raise ValueError("non-PGO builds cannot consume a profile")
    if pgo and (
        mode not in {"generate", "use"} or platform == "linux" and arch == "aarch64"
    ):
        raise ValueError("unsupported PGO mode or target")
    if pgo and ((mode == "use") != bool(artifact)):
        raise ValueError("only profile-use requires a profile artifact")
    role = (
        f"profile-{mode}" if pgo else "canonical-debug" if debug else "production-opt"
    )
    if debug and pgo:
        role = f"legacy-debug-{role}"
    return {
        "target": TARGETS[(platform, arch)],
        "role": role,
        "debug": debug,
        "pgo": pgo,
        "testsOverride": debug and not pgo,
        "qualificationEligible": role == "canonical-debug",
    }


def ac_options(text):
    options = []
    for line in text.splitlines():
        words = shlex.split(line, comments=True)
        if words and words[0] == "ac_add_options":
            options.extend(words[1:])
    return options


def enable_debug_tests(text, debug, pgo):
    if type(debug) is not bool or type(pgo) is not bool:
        raise ValueError("non-boolean build mode")
    if not debug or pgo:
        return text
    lines = [
        line
        for line in text.splitlines()
        if not re.fullmatch(
            r"\s*ac_add_options\s+--(?:disable|enable)-tests\s*(?:#.*)?", line
        )
    ]
    result = "\n".join(lines) + "\nac_add_options --enable-tests\n"
    test_options = [
        v
        for v in ac_options(result)
        if v.startswith(("--enable-tests", "--disable-tests"))
    ]
    if test_options != ["--enable-tests"]:
        raise ValueError("ambiguous tests configuration")
    return result


def verify_config(text, expected):
    options = ac_options(text)
    if [v for v in options if v.startswith("--target=")] != [
        f"--target={expected['target']}"
    ]:
        raise ValueError("mozconfig target mismatch")
    debug_options = [
        v for v in options if v.startswith(("--enable-debug", "--disable-debug"))
    ]
    if debug_options != (["--enable-debug"] if expected["debug"] else []):
        raise ValueError("mozconfig debug mismatch")
    profiling = [
        v for v in options if v.startswith(("--enable-profile-", "--disable-profile-"))
    ]
    role = expected["role"]
    wanted = []
    if expected["pgo"]:
        mode = "generate" if role.endswith("generate") else "use"
        wanted = [f"--enable-profile-{mode}=cross"]
    if profiling != wanted:
        raise ValueError("mozconfig profiling mismatch")
    if expected["testsOverride"]:
        tests = [
            v for v in options if v.startswith(("--enable-tests", "--disable-tests"))
        ]
        if tests != ["--enable-tests"]:
            raise ValueError("canonical Debug requires explicit tests")
    if expected["pgo"] and role.endswith("use"):
        if not re.search(r"^export MOZ_LTO=cross$", text, re.M):
            raise ValueError("profile-use requires cross LTO")
        for prefix in ("--with-pgo-profile-path=", "--with-pgo-jarlog="):
            if len([v for v in options if v.startswith(prefix)]) != 1:
                raise ValueError("profile-use requires profdata and jarlog paths")
    return {
        "kind": "generated-mozconfig-only",
        "sha256": hashlib.sha256(text.encode()).hexdigest(),
        "configureExecuted": False,
        "nativeTestsExecuted": False,
        "publicationAuthorized": False,
    }


def main():
    parser = argparse.ArgumentParser()
    action = parser.add_mutually_exclusive_group(required=True)
    action.add_argument("--validate-inputs", action="store_true")
    action.add_argument("--config", type=Path)
    parser.add_argument("--platform", required=True)
    parser.add_argument("--arch", required=True)
    parser.add_argument("--debug", type=boolean, required=True)
    parser.add_argument("--pgo", type=boolean, required=True)
    parser.add_argument("--mode", default="")
    parser.add_argument("--artifact", default="")
    parser.add_argument("--allow-legacy-debug-pgo", action="store_true")
    args = parser.parse_args()
    try:
        expected = profile(
            args.platform,
            args.arch,
            args.debug,
            args.pgo,
            args.mode,
            args.artifact,
            allow_legacy_debug_pgo=args.allow_legacy_debug_pgo,
        )
        if args.validate_inputs:
            if args.debug and args.pgo:
                print(
                    "Legacy Debug+PGO is noncanonical and ineligible for qualification",
                    file=sys.stderr,
                )
            return
        text = args.config.read_text()
        updated = enable_debug_tests(text, args.debug, args.pgo)
        verify_config(updated, expected)
        if updated != text:
            args.config.write_text(updated)
    except (OSError, ValueError) as error:
        parser.error(str(error))


if __name__ == "__main__":
    main()
