#!/usr/bin/env -S python3 -B
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this file,
# You can obtain one at http://mozilla.org/MPL/2.0/.

import argparse
import enum
import logging
import os
import shutil
import stat
import subprocess
import sys
from pathlib import Path

logging.basicConfig(format="%(levelname)s: %(message)s", level=logging.INFO)


def find_command(names):
    """Search for command in `names`, and returns the first one that exists."""

    for name in names:
        path = shutil.which(name)
        if path is not None:
            return name

    return None


def assert_command(env_var, name):
    """Assert that the command is not empty
    The command name comes from either environment variable or find_command.
    """
    if not name:
        logging.error("{} command not found".format(env_var))
        sys.exit(1)


def parse_version(topsrc_dir):
    """Parse milestone.txt and return the entire milestone and major version."""
    milestone_file = topsrc_dir / "config" / "milestone.txt"
    if not milestone_file.is_file():
        return ("", "", "")

    with milestone_file.open("r") as f:
        for line in f:
            line = line.strip()
            if not line:
                continue
            if line.startswith("#"):
                continue

            v = line.split(".")
            return tuple((v + ["", ""])[:3])

    return ("", "", "")


tmp_dir = Path("/tmp")

tar = os.environ.get("TAR", find_command(["tar"]))
assert_command("TAR", tar)

rsync = os.environ.get("RSYNC", find_command(["rsync"]))
assert_command("RSYNC", rsync)

m4 = os.environ.get("M4", find_command(["m4"]))
assert_command("M4", m4)

awk = os.environ.get("AWK", find_command(["awk"]))
assert_command("AWK", awk)

src_dir = Path(os.environ.get("SRC_DIR", Path(__file__).parent.absolute()))
mozjs_name = os.environ.get("MOZJS_NAME", "mozjs")
staging_dir = Path(os.environ.get("STAGING", tmp_dir / "mozjs-src-pkg"))
dist_dir = Path(os.environ.get("DIST", tmp_dir))
topsrc_dir = src_dir.parent.parent.absolute()

parsed_major_version, parsed_minor_version, parsed_patch_version = parse_version(
    topsrc_dir
)

major_version = os.environ.get("MOZJS_MAJOR_VERSION", parsed_major_version)
minor_version = os.environ.get("MOZJS_MINOR_VERSION", parsed_minor_version)
patch_version = os.environ.get("MOZJS_PATCH_VERSION", parsed_patch_version)
alpha = os.environ.get("MOZJS_ALPHA", "")

version = "{}-{}.{}.{}".format(
    mozjs_name, major_version, minor_version, patch_version or alpha or "0"
)
target_dir = staging_dir / version
package_name = "{}.tar.xz".format(version)
package_file = dist_dir / package_name
tar_opts = ["-Jcf"]

# Given there might be some external program that reads the following output,
# use raw `print`, instead of logging.
print("Environment:")
print("    TAR = {}".format(tar))
print("    RSYNC = {}".format(rsync))
print("    M4 = {}".format(m4))
print("    AWK = {}".format(awk))
print("    STAGING = {}".format(staging_dir))
print("    DIST = {}".format(dist_dir))
print("    SRC_DIR = {}".format(src_dir))
print("    MOZJS_NAME = {}".format(mozjs_name))
print("    MOZJS_MAJOR_VERSION = {}".format(major_version))
print("    MOZJS_MINOR_VERSION = {}".format(minor_version))
print("    MOZJS_PATCH_VERSION = {}".format(patch_version))
print("    MOZJS_ALPHA = {}".format(alpha))
print("")

rsync_filter_list = """
# Top-level config and build files

+ /aclocal.m4
+ /client.mk
+ /configure.py
+ /LICENSE
+ /mach
+ /Makefile.in
+ /moz.build
+ /moz.configure
+ /test.mozbuild
+ /.babel-eslint.rc.js
+ /.eslintignore
+ /.eslintrc.js
+ /.flake8
+ /.gitignore
+ /.hgignore
+ /.lldbinit
+ /.prettierignore
+ /.prettierrc
+ /.ycm_extra_conf.py

# Additional libraries (optionally) used by SpiderMonkey

+ /mfbt/**
+ /nsprpub/**

- /intl/icu/source/data
- /intl/icu/source/test
- /intl/icu/source/tools
+ /intl/icu/**

- /intl/components/gtest
+ /intl/components/**

+ /memory/replace/dmd/dmd.py
+ /memory/build/**
+ /memory/moz.build
+ /memory/mozalloc/**

+ /modules/fdlibm/**
+ /modules/zlib/**

+ /mozglue/baseprofiler/**
+ /mozglue/build/**
+ /mozglue/misc/**
+ /mozglue/moz.build
+ /mozglue/static/**

+ /tools/rb/fix_stacks.py
+ /tools/fuzzing/moz.build
+ /tools/fuzzing/interface/**
+ /tools/fuzzing/registry/**
+ /tools/fuzzing/libfuzzer/**
+ /tools/fuzzing/*.mozbuild

# Build system and dependencies

+ /Cargo.lock
+ /build/**
+ /config/**
+ /python/**

+ /.cargo/config.in

- /third_party/python/gyp
+ /third_party/python/**
+ /third_party/rust/**

+ /layout/tools/reftest/reftest/**

+ /testing/mach_commands.py
+ /testing/moz.build
+ /testing/mozbase/**
+ /testing/performance/**
+ /testing/web-platform/*.ini
+ /testing/web-platform/*.py
+ /testing/web-platform/meta/streams/**
+ /testing/web-platform/mozilla/**
+ /testing/web-platform/tests/resources/**
+ /testing/web-platform/tests/streams/**
+ /testing/web-platform/tests/tools/**

+ /toolkit/crashreporter/tools/symbolstore.py
+ /toolkit/mozapps/installer/package-name.mk

# SpiderMonkey itself

+ /js/src/**
+ /js/app.mozbuild
+ /js/*.configure
+ /js/examples/**
+ /js/public/**

+ */
- /**
"""

INSTALL_CONTENT = """\
Documentation for SpiderMonkey is available at:

  https://spidermonkey.dev/

In particular, it points to build documentation at

  https://firefox-source-docs.mozilla.org/js/build.html

Note that the libraries produced by the build system include symbols,
causing the binaries to be extremely large. It is highly suggested that `strip`
be run over the binaries before deploying them.

Building with default options may be performed as follows:

  ./mach build

This will produce a debug build (much more suitable for developing against the
SpiderMonkey JSAPI). To produce an optimized build:

  export MOZCONFIG=$(pwd)/mozconfig.opt
  ./mach build

You may edit the mozconfig and mozconfig.opt files to configure your own build
appropriately.
"""

MOZCONFIG_DEBUG_CONTENT = """\
# Much slower when running, but adds assertions that are much better for
# developing against the JSAPI.
ac_add_options --enable-debug

# Much faster when running, worse for debugging.
ac_add_options --enable-optimize

mk_add_options MOZ_OBJDIR=obj-debug
"""

MOZCONFIG_OPT_CONTENT = """\
# Much faster when running, but very error-prone to develop against because
# this will skip all the assertions critical to using the JSAPI properly.
ac_add_options --disable-debug

# Much faster when running, worse for debugging.
ac_add_options --enable-optimize

mk_add_options MOZ_OBJDIR=obj-opt
"""

README_CONTENT = """\
This directory contains SpiderMonkey {major_version}.

This release is based on a revision of Mozilla {major_version}:
  https://hg.mozilla.org/releases/
The changes in the patches/ directory were applied.

See https://spidermonkey.dev/ for documentation, examples, and release notes.
""".format(
    major_version=major_version
)


def is_mozjs_cargo_member(line):
    """Checks if the line in workspace.members is mozjs-related"""

    return '"js/' in line


def is_mozjs_crates_io_local_patch(line):
    """Checks if the line in patch.crates-io is mozjs-related"""

    return 'path = "js' in line


def clean():
    """Remove temporary directory and package file."""
    logging.info("Cleaning {} and {} ...".format(package_file, target_dir))
    if package_file.exists():
        package_file.unlink()
    if target_dir.exists():
        shutil.rmtree(str(target_dir))


def assert_clean():
    """Assert that target directory does not contain generated files."""
    makefile_file = target_dir / "js" / "src" / "Makefile"
    if makefile_file.exists():
        logging.error("found js/src/Makefile. Please clean before packaging.")
        sys.exit(1)


def create_target_dir():
    if target_dir.exists():
        logging.warning("dist tree {} already exists!".format(target_dir))
    else:
        target_dir.mkdir(parents=True)


def sync_files():
    # Output of the command should directly go to stdout/stderr.
    p = subprocess.Popen(
        [
            str(rsync),
            "--delete-excluded",
            "--prune-empty-dirs",
            "--quiet",
            "--recursive",
            "{}/".format(topsrc_dir),
            "{}/".format(target_dir),
            "--filter=. -",
        ],
        stdin=subprocess.PIPE,
    )

    p.communicate(rsync_filter_list.encode())

    if p.returncode != 0:
        sys.exit(p.returncode)


def copy_cargo_toml():
    cargo_toml_file = topsrc_dir / "Cargo.toml"
    target_cargo_toml_file = target_dir / "Cargo.toml"

    with cargo_toml_file.open("r") as f:

        class State(enum.Enum):
            BEFORE_MEMBER = 1
            INSIDE_MEMBER = 2
            AFTER_MEMBER = 3
            INSIDE_PATCH = 4
            AFTER_PATCH = 5

        content = ""
        state = State.BEFORE_MEMBER
        for line in f:
            if state == State.BEFORE_MEMBER:
                if line.strip() == "members = [":
                    state = State.INSIDE_MEMBER
            elif state == State.INSIDE_MEMBER:
                if line.strip() == "]":
                    state = State.AFTER_MEMBER
                elif not is_mozjs_cargo_member(line):
                    continue
            elif state == State.AFTER_MEMBER:
                if line.strip() == "[patch.crates-io]":
                    state = State.INSIDE_PATCH
            elif state == State.INSIDE_PATCH:
                if line.startswith("["):
                    state = State.AFTER_PATCH
                if "path = " in line:
                    if not is_mozjs_crates_io_local_patch(line):
                        continue

            content += line

    with target_cargo_toml_file.open("w") as f:
        f.write(content)


def generate_configure():
    """Generate configure files to avoid build dependency on autoconf-2.13"""

    src_configure_in_file = topsrc_dir / "js" / "src" / "configure.in"
    src_old_configure_in_file = topsrc_dir / "js" / "src" / "old-configure.in"
    dest_configure_file = target_dir / "js" / "src" / "configure"
    dest_old_configure_file = target_dir / "js" / "src" / "old-configure"

    shutil.copy2(
        str(src_configure_in_file), str(dest_configure_file), follow_symlinks=False
    )
    st = dest_configure_file.stat()
    dest_configure_file.chmod(st.st_mode | stat.S_IXUSR | stat.S_IXGRP | stat.S_IXOTH)

    js_src_dir = topsrc_dir / "js" / "src"

    env = os.environ.copy()
    env["M4"] = m4
    env["AWK"] = awk
    env["AC_MACRODIR"] = topsrc_dir / "build" / "autoconf"

    with dest_old_configure_file.open("w") as f:
        subprocess.run(
            [
                "sh",
                str(topsrc_dir / "build" / "autoconf" / "autoconf.sh"),
                "--localdir={}".format(js_src_dir),
                str(src_old_configure_in_file),
            ],
            stdout=f,
            check=True,
            env=env,
        )


def copy_file(filename, content):
    """Copy an existing file from the staging area, or create a new file
    with the given contents if it does not exist."""

    staging_file = staging_dir / filename
    target_file = target_dir / filename

    if staging_file.exists():
        shutil.copy2(str(staging_file), str(target_file))
    else:
        with target_file.open("w") as f:
            f.write(content)


def copy_patches():
    """Copy patches dir, if it exists."""

    staging_patches_dir = staging_dir / "patches"
    top_patches_dir = topsrc_dir / "patches"
    target_patches_dir = target_dir / "patches"

    if staging_patches_dir.is_dir():
        shutil.copytree(str(staging_patches_dir), str(target_patches_dir))
    elif top_patches_dir.is_dir():
        shutil.copytree(str(top_patches_dir), str(target_patches_dir))


def remove_python_cache():
    """Remove *.pyc and *.pyo files if any."""
    for f in target_dir.glob("**/*.pyc"):
        f.unlink()
    for f in target_dir.glob("**/*.pyo"):
        f.unlink()


def stage():
    """Stage source tarball content."""
    logging.info("Staging source tarball in {}...".format(target_dir))

    create_target_dir()
    sync_files()
    copy_cargo_toml()
    generate_configure()
    copy_file("INSTALL", INSTALL_CONTENT)
    copy_file("README", README_CONTENT)
    copy_file("mozconfig", MOZCONFIG_DEBUG_CONTENT)
    copy_file("mozconfig.opt", MOZCONFIG_OPT_CONTENT)
    copy_patches()
    remove_python_cache()


def create_tar():
    """Roll the tarball."""

    logging.info("Packaging source tarball at {}...".format(package_file))

    subprocess.run(
        [str(tar)] + tar_opts + [str(package_file), "-C", str(staging_dir), version],
        check=True,
    )


def build():
    assert_clean()
    stage()
    create_tar()


parser = argparse.ArgumentParser(description="Make SpiderMonkey source package")
subparsers = parser.add_subparsers(dest="COMMAND")
subparser_update = subparsers.add_parser("clean", help="")
subparser_update = subparsers.add_parser("build", help="")
args = parser.parse_args()

if args.COMMAND == "clean":
    clean()
elif not args.COMMAND or args.COMMAND == "build":
    build()
