#!/usr/bin/python3
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Only necessary for flake8 to be happy...
from __future__ import print_function

import os
import os.path
import shutil
import subprocess
import platform
import json
import argparse
import fnmatch
import glob
import errno
import re
import sys
import tarfile
from contextlib import contextmanager

from shutil import which

import zstandard


def check_run(args):
    print(" ".join(args), file=sys.stderr, flush=True)
    if args[0] == "cmake":
        # CMake `message(STATUS)` messages, as appearing in failed source code
        # compiles, appear on stdout, so we only capture that.
        p = subprocess.Popen(args, stdout=subprocess.PIPE)
        lines = []
        for line in p.stdout:
            lines.append(line)
            sys.stdout.write(line.decode())
            sys.stdout.flush()
        r = p.wait()
        if r != 0 and os.environ.get("UPLOAD_DIR"):
            cmake_output_re = re.compile(b'See also "(.*/CMakeOutput.log)"')
            cmake_error_re = re.compile(b'See also "(.*/CMakeError.log)"')

            def find_first_match(re):
                for l in lines:
                    match = re.search(l)
                    if match:
                        return match

            output_match = find_first_match(cmake_output_re)
            error_match = find_first_match(cmake_error_re)

            upload_dir = os.environ["UPLOAD_DIR"].encode("utf-8")
            if output_match or error_match:
                mkdir_p(upload_dir)
            if output_match:
                shutil.copy2(output_match.group(1), upload_dir)
            if error_match:
                shutil.copy2(error_match.group(1), upload_dir)
    else:
        r = subprocess.call(args)
    assert r == 0


def run_in(path, args):
    with chdir(path):
        check_run(args)


@contextmanager
def chdir(path):
    d = os.getcwd()
    print('cd "%s"' % path, file=sys.stderr)
    os.chdir(path)
    try:
        yield
    finally:
        print('cd "%s"' % d, file=sys.stderr)
        os.chdir(d)


def patch(patch, srcdir):
    patch = os.path.realpath(patch)
    check_run(["patch", "-d", srcdir, "-p1", "-i", patch, "--fuzz=0", "-s"])


def import_clang_tidy(source_dir, build_clang_tidy_alpha, build_clang_tidy_external):
    clang_plugin_path = os.path.join(os.path.dirname(sys.argv[0]), "..", "clang-plugin")
    clang_tidy_path = os.path.join(source_dir, "clang-tools-extra/clang-tidy")
    sys.path.append(clang_plugin_path)
    from import_mozilla_checks import do_import

    import_options = {
        "alpha": build_clang_tidy_alpha,
        "external": build_clang_tidy_external,
    }
    do_import(clang_plugin_path, clang_tidy_path, import_options)


def build_package(package_build_dir, cmake_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    # If CMake has already been run, it may have been run with different
    # arguments, so we need to re-run it.  Make sure the cached copy of the
    # previous CMake run is cleared before running it again.
    if os.path.exists(package_build_dir + "/CMakeCache.txt"):
        os.remove(package_build_dir + "/CMakeCache.txt")
    if os.path.exists(package_build_dir + "/CMakeFiles"):
        shutil.rmtree(package_build_dir + "/CMakeFiles")

    run_in(package_build_dir, ["cmake"] + cmake_args)
    run_in(package_build_dir, ["ninja", "install", "-v"])


@contextmanager
def updated_env(env):
    old_env = os.environ.copy()
    os.environ.update(env)
    yield
    os.environ.clear()
    os.environ.update(old_env)


def build_tar_package(name, base, directory):
    name = os.path.realpath(name)
    print("tarring {} from {}/{}".format(name, base, directory), file=sys.stderr)
    assert name.endswith(".tar.zst")

    cctx = zstandard.ZstdCompressor()
    with open(name, "wb") as f, cctx.stream_writer(f) as z:
        with tarfile.open(mode="w|", fileobj=z) as tf:
            with chdir(base):
                tf.add(directory)


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != errno.EEXIST or not os.path.isdir(path):
            raise


def delete(path):
    if os.path.isdir(path):
        shutil.rmtree(path)
    else:
        try:
            os.unlink(path)
        except Exception:
            pass


def install_import_library(build_dir, clang_dir):
    shutil.copy2(
        os.path.join(build_dir, "lib", "clang.lib"), os.path.join(clang_dir, "lib")
    )


def is_darwin():
    return platform.system() == "Darwin"


def is_linux():
    return platform.system() == "Linux"


def is_windows():
    return platform.system() == "Windows"


def build_one_stage(
    cc,
    cxx,
    asm,
    ld,
    ar,
    ranlib,
    libtool,
    src_dir,
    stage_dir,
    package_name,
    osx_cross_compile,
    build_type,
    assertions,
    targets,
    is_final_stage=False,
    profile=None,
):
    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/" + package_name

    # cmake doesn't deal well with backslashes in paths.
    def slashify_path(path):
        return path.replace("\\", "/")

    def cmake_base_args(cc, cxx, asm, ld, ar, ranlib, libtool, inst_dir):
        machine_targets = targets if is_final_stage and targets else "X86"

        cmake_args = [
            "-GNinja",
            "-DCMAKE_C_COMPILER=%s" % slashify_path(cc[0]),
            "-DCMAKE_CXX_COMPILER=%s" % slashify_path(cxx[0]),
            "-DCMAKE_ASM_COMPILER=%s" % slashify_path(asm[0]),
            "-DCMAKE_LINKER=%s" % slashify_path(ld[0]),
            "-DCMAKE_AR=%s" % slashify_path(ar),
            "-DCMAKE_C_FLAGS=%s" % " ".join(cc[1:]),
            "-DCMAKE_CXX_FLAGS=%s" % " ".join(cxx[1:]),
            "-DCMAKE_ASM_FLAGS=%s" % " ".join(asm[1:]),
            "-DCMAKE_EXE_LINKER_FLAGS=%s" % " ".join(ld[1:]),
            "-DCMAKE_SHARED_LINKER_FLAGS=%s" % " ".join(ld[1:]),
            "-DCMAKE_BUILD_TYPE=%s" % build_type,
            "-DCMAKE_INSTALL_PREFIX=%s" % inst_dir,
            "-DLLVM_TARGETS_TO_BUILD=%s" % machine_targets,
            "-DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF",
            "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
            "-DLLVM_ENABLE_BINDINGS=OFF",
            "-DLLVM_ENABLE_CURL=OFF",
            "-DLLVM_INCLUDE_TESTS=OFF",
        ]
        if "TASK_ID" in os.environ:
            cmake_args += [
                "-DCLANG_REPOSITORY_STRING=taskcluster-%s" % os.environ["TASK_ID"],
            ]
        projects = ["clang"]
        if is_final_stage:
            projects.extend(("clang-tools-extra", "lld"))
        else:
            cmake_args.append("-DLLVM_TOOL_LLI_BUILD=OFF")

        cmake_args.append("-DLLVM_ENABLE_PROJECTS=%s" % ";".join(projects))

        # There is no libxml2 on Windows except if we build one ourselves.
        # libxml2 is only necessary for llvm-mt, but Windows can just use the
        # native MT tool.
        if not is_windows() and is_final_stage:
            cmake_args += ["-DLLVM_ENABLE_LIBXML2=FORCE_ON"]
        if is_linux() and not osx_cross_compile and is_final_stage:
            sysroot = os.path.join(os.environ.get("MOZ_FETCHES_DIR", ""), "sysroot")
            if os.path.exists(sysroot):
                cmake_args += ["-DLLVM_BINUTILS_INCDIR=/usr/include"]
                cmake_args += ["-DCMAKE_SYSROOT=%s" % sysroot]
                # Work around the LLVM build system not building the i386 compiler-rt
                # because it doesn't allow to use a sysroot for that during the cmake
                # checks.
                cmake_args += ["-DCAN_TARGET_i386=1"]
        if is_windows():
            cmake_args.insert(-1, "-DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON")
            cmake_args.insert(-1, "-DLLVM_USE_CRT_RELEASE=MT")
        else:
            # libllvm as a shared library is not supported on Windows
            cmake_args += ["-DLLVM_LINK_LLVM_DYLIB=ON"]
        if ranlib is not None:
            cmake_args += ["-DCMAKE_RANLIB=%s" % slashify_path(ranlib)]
        if libtool is not None:
            cmake_args += ["-DCMAKE_LIBTOOL=%s" % slashify_path(libtool)]
        if osx_cross_compile:
            arch = "arm64" if os.environ.get("OSX_ARCH") == "arm64" else "x86_64"
            target_cpu = (
                "aarch64" if os.environ.get("OSX_ARCH") == "arm64" else "x86_64"
            )
            cmake_args += [
                "-DCMAKE_SYSTEM_NAME=Darwin",
                "-DCMAKE_SYSTEM_VERSION=%s" % os.environ["MACOSX_DEPLOYMENT_TARGET"],
                "-DCMAKE_OSX_SYSROOT=%s" % slashify_path(os.getenv("CROSS_SYSROOT")),
                "-DCMAKE_FIND_ROOT_PATH=%s" % slashify_path(os.getenv("CROSS_SYSROOT")),
                "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER",
                "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY",
                "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY",
                "-DCMAKE_MACOSX_RPATH=ON",
                "-DCMAKE_OSX_ARCHITECTURES=%s" % arch,
                "-DDARWIN_osx_ARCHS=%s" % arch,
                "-DDARWIN_osx_SYSROOT=%s" % slashify_path(os.getenv("CROSS_SYSROOT")),
                "-DLLVM_DEFAULT_TARGET_TRIPLE=%s-apple-darwin" % target_cpu,
                "-DCMAKE_C_COMPILER_TARGET=%s-apple-darwin" % target_cpu,
                "-DCMAKE_CXX_COMPILER_TARGET=%s-apple-darwin" % target_cpu,
                "-DCMAKE_ASM_COMPILER_TARGET=%s-apple-darwin" % target_cpu,
            ]
            if os.environ.get("OSX_ARCH") == "arm64":
                cmake_args += [
                    "-DDARWIN_osx_BUILTIN_ARCHS=arm64",
                ]
            # Starting in LLVM 11 (which requires SDK 10.12) the build tries to
            # detect the SDK version by calling xcrun. Cross-compiles don't have
            # an xcrun, so we have to set the version explicitly.
            cmake_args += [
                "-DDARWIN_macosx_OVERRIDE_SDK_VERSION=%s"
                % os.environ["MACOSX_DEPLOYMENT_TARGET"],
            ]
        if profile == "gen":
            # Per https://releases.llvm.org/10.0.0/docs/HowToBuildWithPGO.html
            cmake_args += [
                "-DLLVM_BUILD_INSTRUMENTED=IR",
                "-DLLVM_BUILD_RUNTIME=No",
            ]
        elif profile:
            cmake_args += [
                "-DLLVM_PROFDATA_FILE=%s" % profile,
            ]
        return cmake_args

    cmake_args = []
    cmake_args += cmake_base_args(cc, cxx, asm, ld, ar, ranlib, libtool, inst_dir)
    cmake_args += [src_dir]
    build_package(build_dir, cmake_args)

    # For some reasons the import library clang.lib of clang.exe is not
    # installed, so we copy it by ourselves.
    if is_windows() and is_final_stage:
        install_import_library(build_dir, inst_dir)


# Return the absolute path of a build tool.  We first look to see if the
# variable is defined in the config file, and if so we make sure it's an
# absolute path to an existing tool, otherwise we look for a program in
# $PATH named "key".
#
# This expects the name of the key in the config file to match the name of
# the tool in the default toolchain on the system (for example, "ld" on Unix
# and "link" on Windows).
def get_tool(config, key):
    f = None
    if key in config:
        f = config[key].format(**os.environ)
        if os.path.isabs(f):
            if not os.path.exists(f):
                raise ValueError("%s must point to an existing path" % key)
            return f

    # Assume that we have the name of some program that should be on PATH.
    tool = which(f) if f else which(key)
    if not tool:
        raise ValueError("%s not found on PATH" % (f or key))
    return tool


# This function is intended to be called on the final build directory when
# building clang-tidy. Also clang-format binaries are included that can be used
# in conjunction with clang-tidy.
# As a separate binary we also ship clangd for the language server protocol that
# can be used as a plugin in `vscode`.
# Its job is to remove all of the files which won't be used for clang-tidy or
# clang-format to reduce the download size.  Currently when this function
# finishes its job, it will leave final_dir with a layout like this:
#
# clang/
#   bin/
#     clang-apply-replacements
#     clang-format
#     clang-tidy
#     clangd
#     run-clang-tidy
#   include/
#     * (nothing will be deleted here)
#   lib/
#     clang/
#       4.0.0/
#         include/
#           * (nothing will be deleted here)
#   share/
#     clang/
#       clang-format-diff.py
#       clang-tidy-diff.py
#       run-clang-tidy.py
def prune_final_dir_for_clang_tidy(final_dir, osx_cross_compile):
    # Make sure we only have what we expect.
    dirs = [
        "bin",
        "include",
        "lib",
        "lib32",
        "libexec",
        "msbuild-bin",
        "share",
        "tools",
    ]
    if is_linux():
        dirs.append("x86_64-unknown-linux-gnu")
    for f in glob.glob("%s/*" % final_dir):
        if os.path.basename(f) not in dirs:
            raise Exception("Found unknown file %s in the final directory" % f)
        if not os.path.isdir(f):
            raise Exception("Expected %s to be a directory" % f)

    kept_binaries = [
        "clang-apply-replacements",
        "clang-format",
        "clang-tidy",
        "clangd",
        "clang-query",
        "run-clang-tidy",
    ]
    re_clang_tidy = re.compile(r"^(" + "|".join(kept_binaries) + r")(\.exe)?$", re.I)
    for f in glob.glob("%s/bin/*" % final_dir):
        if re_clang_tidy.search(os.path.basename(f)) is None:
            delete(f)

    # Keep include/ intact.

    # Remove the target-specific files.
    if is_linux():
        if os.path.exists(os.path.join(final_dir, "x86_64-unknown-linux-gnu")):
            shutil.rmtree(os.path.join(final_dir, "x86_64-unknown-linux-gnu"))

    # In lib/, only keep lib/clang/N.M.O/include and the LLVM shared library.
    re_ver_num = re.compile(r"^\d+\.\d+\.\d+$", re.I)
    for f in glob.glob("%s/lib/*" % final_dir):
        name = os.path.basename(f)
        if name == "clang":
            continue
        if osx_cross_compile and name in ["libLLVM.dylib", "libclang-cpp.dylib"]:
            continue
        if is_linux() and (
            fnmatch.fnmatch(name, "libLLVM*.so")
            or fnmatch.fnmatch(name, "libclang-cpp.so*")
        ):
            continue
        delete(f)
    for f in glob.glob("%s/lib/clang/*" % final_dir):
        if re_ver_num.search(os.path.basename(f)) is None:
            delete(f)
    for f in glob.glob("%s/lib/clang/*/*" % final_dir):
        if os.path.basename(f) != "include":
            delete(f)

    # Completely remove libexec/, msbuild-bin and tools, if it exists.
    shutil.rmtree(os.path.join(final_dir, "libexec"))
    for d in ("msbuild-bin", "tools"):
        d = os.path.join(final_dir, d)
        if os.path.exists(d):
            shutil.rmtree(d)

    # In share/, only keep share/clang/*tidy*
    re_clang_tidy = re.compile(r"format|tidy", re.I)
    for f in glob.glob("%s/share/*" % final_dir):
        if os.path.basename(f) != "clang":
            delete(f)
    for f in glob.glob("%s/share/clang/*" % final_dir):
        if re_clang_tidy.search(os.path.basename(f)) is None:
            delete(f)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c",
        "--config",
        action="append",
        required=True,
        type=argparse.FileType("r"),
        help="Clang configuration file",
    )
    parser.add_argument(
        "--clean", required=False, action="store_true", help="Clean the build directory"
    )
    parser.add_argument(
        "--skip-tar",
        required=False,
        action="store_true",
        help="Skip tar packaging stage",
    )
    parser.add_argument(
        "--skip-patch",
        required=False,
        action="store_true",
        help="Do not patch source",
    )

    args = parser.parse_args()

    if not os.path.exists("llvm/README.txt"):
        raise Exception(
            "The script must be run from the root directory of the llvm-project tree"
        )
    source_dir = os.getcwd()
    build_dir = source_dir + "/build"

    if args.clean:
        shutil.rmtree(build_dir)
        os.sys.exit(0)

    llvm_source_dir = source_dir + "/llvm"

    exe_ext = ""
    if is_windows():
        exe_ext = ".exe"

    cc_name = "clang"
    cxx_name = "clang++"
    if is_windows():
        cc_name = "clang-cl"
        cxx_name = "clang-cl"

    config = {}
    # Merge all the configs we got from the command line.
    for c in args.config:
        this_config_dir = os.path.dirname(c.name)
        this_config = json.load(c)
        patches = this_config.get("patches")
        if patches:
            this_config["patches"] = [os.path.join(this_config_dir, p) for p in patches]
        for key, value in this_config.items():
            old_value = config.get(key)
            if old_value is None:
                config[key] = value
            elif value is None:
                if key in config:
                    del config[key]
            elif type(old_value) != type(value):
                raise Exception(
                    "{} is overriding `{}` with a value of the wrong type".format(
                        c.name, key
                    )
                )
            elif isinstance(old_value, list):
                for v in value:
                    if v not in old_value:
                        old_value.append(v)
            elif isinstance(old_value, dict):
                raise Exception("{} is setting `{}` to a dict?".format(c.name, key))
            else:
                config[key] = value

    stages = 2
    if "stages" in config:
        stages = int(config["stages"])
        if stages not in (1, 2, 3, 4):
            raise ValueError("We only know how to build 1, 2, 3, or 4 stages.")
    skip_stages = 0
    if "skip_stages" in config:
        # The assumption here is that the compiler given in `cc` and other configs
        # is the result of the last skip stage, built somewhere else.
        skip_stages = int(config["skip_stages"])
        if skip_stages >= stages:
            raise ValueError("Cannot skip more stages than are built.")
    pgo = False
    if "pgo" in config:
        pgo = config["pgo"]
        if pgo not in (True, False):
            raise ValueError("Only boolean values are accepted for pgo.")
    build_type = "Release"
    if "build_type" in config:
        build_type = config["build_type"]
        if build_type not in ("Release", "Debug", "RelWithDebInfo", "MinSizeRel"):
            raise ValueError(
                "We only know how to do Release, Debug, RelWithDebInfo or "
                "MinSizeRel builds"
            )
    targets = config.get("targets")
    build_clang_tidy = False
    if "build_clang_tidy" in config:
        build_clang_tidy = config["build_clang_tidy"]
        if build_clang_tidy not in (True, False):
            raise ValueError("Only boolean values are accepted for build_clang_tidy.")
    build_clang_tidy_alpha = False
    # check for build_clang_tidy_alpha only if build_clang_tidy is true
    if build_clang_tidy and "build_clang_tidy_alpha" in config:
        build_clang_tidy_alpha = config["build_clang_tidy_alpha"]
        if build_clang_tidy_alpha not in (True, False):
            raise ValueError(
                "Only boolean values are accepted for build_clang_tidy_alpha."
            )
    build_clang_tidy_external = False
    # check for build_clang_tidy_external only if build_clang_tidy is true
    if build_clang_tidy and "build_clang_tidy_external" in config:
        build_clang_tidy_external = config["build_clang_tidy_external"]
        if build_clang_tidy_external not in (True, False):
            raise ValueError(
                "Only boolean values are accepted for build_clang_tidy_external."
            )
    osx_cross_compile = False
    if "osx_cross_compile" in config:
        osx_cross_compile = config["osx_cross_compile"]
        if osx_cross_compile not in (True, False):
            raise ValueError("Only boolean values are accepted for osx_cross_compile.")
        if osx_cross_compile and not is_linux():
            raise ValueError("osx_cross_compile can only be used on Linux.")
    assertions = False
    if "assertions" in config:
        assertions = config["assertions"]
        if assertions not in (True, False):
            raise ValueError("Only boolean values are accepted for assertions.")

    if is_darwin() or osx_cross_compile:
        os.environ["MACOSX_DEPLOYMENT_TARGET"] = (
            "11.0" if os.environ.get("OSX_ARCH") == "arm64" else "10.12"
        )

    cc = get_tool(config, "cc")
    cxx = get_tool(config, "cxx")
    asm = get_tool(config, "ml" if is_windows() else "as")
    ld = get_tool(config, "link" if is_windows() else "ld")
    ar = get_tool(config, "lib" if is_windows() else "ar")
    ranlib = None if is_windows() else get_tool(config, "ranlib")
    libtool = None
    if "libtool" in config:
        libtool = get_tool(config, "libtool")

    if not os.path.exists(source_dir):
        os.makedirs(source_dir)

    if not args.skip_patch:
        for p in config.get("patches", []):
            patch(p, source_dir)

    package_name = "clang"
    if build_clang_tidy:
        package_name = "clang-tidy"
        if not args.skip_patch:
            import_clang_tidy(
                source_dir, build_clang_tidy_alpha, build_clang_tidy_external
            )

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    stage1_dir = build_dir + "/stage1"
    stage1_inst_dir = stage1_dir + "/" + package_name

    final_stage_dir = stage1_dir

    if is_darwin():
        extra_cflags = []
        extra_cxxflags = []
        extra_cflags2 = []
        extra_cxxflags2 = []
        extra_asmflags = []
        extra_ldflags = []
    elif is_linux():
        extra_cflags = []
        extra_cxxflags = []
        extra_cflags2 = ["-fPIC"]
        # Silence clang's warnings about arguments not being used in compilation.
        extra_cxxflags2 = [
            "-fPIC",
            "-Qunused-arguments",
        ]
        extra_asmflags = []
        # Avoid libLLVM internal function calls going through the PLT.
        extra_ldflags = ["-Wl,-Bsymbolic-functions"]
        # For whatever reason, LLVM's build system will set things up to turn
        # on -ffunction-sections and -fdata-sections, but won't turn on the
        # corresponding option to strip unused sections.  We do it explicitly
        # here.  LLVM's build system is also picky about turning on ICF, so
        # we do that explicitly here, too.
        extra_ldflags += ["-fuse-ld=gold", "-Wl,--gc-sections", "-Wl,--icf=safe"]
    elif is_windows():
        extra_cflags = []
        extra_cxxflags = []
        # clang-cl would like to figure out what it's supposed to be emulating
        # by looking at an MSVC install, but we don't really have that here.
        # Force things on based on WinMsvc.cmake.
        # Ideally, we'd just use WinMsvc.cmake as a toolchain file, but it only
        # really works for cross-compiles, which this is not.
        with open(os.path.join(llvm_source_dir, "cmake/platforms/WinMsvc.cmake")) as f:
            compat = [
                item
                for line in f
                for item in line.split()
                if "-fms-compatibility-version=" in item
            ][0]
        extra_cflags2 = [compat]
        extra_cxxflags2 = [compat]
        extra_asmflags = []
        extra_ldflags = []

    if osx_cross_compile:
        # undo the damage done in the is_linux() block above, and also simulate
        # the is_darwin() block above.
        extra_cflags = []
        extra_cxxflags = []
        extra_cxxflags2 = []

        extra_flags = [
            "-mlinker-version=137",
            "-B",
            "%s/bin" % os.getenv("CROSS_CCTOOLS_PATH"),
            "-isysroot",
            os.getenv("CROSS_SYSROOT"),
            # technically the sysroot flag there should be enough to deduce this,
            # but clang needs some help to figure this out.
            "-I%s/usr/include" % os.getenv("CROSS_SYSROOT"),
            "-iframework",
            "%s/System/Library/Frameworks" % os.getenv("CROSS_SYSROOT"),
        ]
        extra_cflags += extra_flags
        extra_cxxflags += extra_flags
        extra_cflags2 += extra_flags
        extra_cxxflags2 += extra_flags
        extra_asmflags += extra_flags
        extra_ldflags = [
            "-Wl,-syslibroot,%s" % os.getenv("CROSS_SYSROOT"),
            "-Wl,-dead_strip",
        ]

    upload_dir = os.getenv("UPLOAD_DIR")
    if assertions and upload_dir:
        extra_cflags2 += ["-fcrash-diagnostics-dir=%s" % upload_dir]
        extra_cxxflags2 += ["-fcrash-diagnostics-dir=%s" % upload_dir]

    if skip_stages < 1:
        build_one_stage(
            [cc] + extra_cflags,
            [cxx] + extra_cxxflags,
            [asm] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage1_dir,
            package_name,
            osx_cross_compile,
            build_type,
            assertions,
            targets,
            is_final_stage=(stages == 1),
        )

    if stages >= 2 and skip_stages < 2:
        stage2_dir = build_dir + "/stage2"
        stage2_inst_dir = stage2_dir + "/" + package_name
        final_stage_dir = stage2_dir
        if skip_stages < 1:
            cc = stage1_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
            cxx = stage1_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)
            asm = stage1_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
        build_one_stage(
            [cc] + extra_cflags2,
            [cxx] + extra_cxxflags2,
            [asm] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage2_dir,
            package_name,
            osx_cross_compile,
            build_type,
            assertions,
            targets,
            is_final_stage=(stages == 2),
            profile="gen" if pgo else None,
        )

    if stages >= 3 and skip_stages < 3:
        stage3_dir = build_dir + "/stage3"
        stage3_inst_dir = stage3_dir + "/" + package_name
        final_stage_dir = stage3_dir
        if skip_stages < 2:
            cc = stage2_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
            cxx = stage2_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)
            asm = stage2_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
        build_one_stage(
            [cc] + extra_cflags2,
            [cxx] + extra_cxxflags2,
            [asm] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage3_dir,
            package_name,
            osx_cross_compile,
            build_type,
            assertions,
            targets,
            (stages == 3),
        )
        if pgo:
            llvm_profdata = stage2_inst_dir + "/bin/llvm-profdata%s" % exe_ext
            merge_cmd = [llvm_profdata, "merge", "-o", "merged.profdata"]
            profraw_files = glob.glob(
                os.path.join(stage2_dir, "build", "profiles", "*.profraw")
            )
            run_in(stage3_dir, merge_cmd + profraw_files)
            if stages == 3:
                mkdir_p(upload_dir)
                shutil.copy2(os.path.join(stage3_dir, "merged.profdata"), upload_dir)
                return

    if stages >= 4 and skip_stages < 4:
        stage4_dir = build_dir + "/stage4"
        final_stage_dir = stage4_dir
        profile = None
        if pgo:
            if skip_stages == 3:
                profile_dir = os.environ.get("MOZ_FETCHES_DIR", "")
            else:
                profile_dir = stage3_dir
            profile = os.path.join(profile_dir, "merged.profdata")
        if skip_stages < 3:
            cc = stage3_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
            cxx = stage3_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)
            asm = stage3_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)
        build_one_stage(
            [cc] + extra_cflags2,
            [cxx] + extra_cxxflags2,
            [asm] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage4_dir,
            package_name,
            osx_cross_compile,
            build_type,
            assertions,
            targets,
            (stages == 4),
            profile=profile,
        )

    if build_clang_tidy:
        prune_final_dir_for_clang_tidy(
            os.path.join(final_stage_dir, package_name), osx_cross_compile
        )

    if not args.skip_tar:
        build_tar_package("%s.tar.zst" % package_name, final_stage_dir, package_name)


if __name__ == "__main__":
    main()
