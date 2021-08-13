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
from distutils.dir_util import copy_tree

from shutil import which

import zstandard


def symlink(source, link_name):
    os_symlink = getattr(os, "symlink", None)
    if callable(os_symlink):
        os_symlink(source, link_name)
    else:
        if os.path.isdir(source):
            # Fall back to copying the directory :(
            copy_tree(source, link_name)


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
        if r != 0:
            cmake_output_re = re.compile(b'See also "(.*/CMakeOutput.log)"')
            cmake_error_re = re.compile(b'See also "(.*/CMakeError.log)"')

            def find_first_match(re):
                for l in lines:
                    match = re.search(l)
                    if match:
                        return match

            output_match = find_first_match(cmake_output_re)
            error_match = find_first_match(cmake_error_re)

            def dump_file(log):
                with open(log, "r", errors="replace") as f:
                    print("\nContents of", log, "follow\n", file=sys.stderr)
                    for line in f:
                        print(line, file=sys.stderr)

            if output_match:
                dump_file(output_match.group(1))
            if error_match:
                dump_file(error_match.group(1))
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


def install_asan_symbols(build_dir, clang_dir):
    lib_path_pattern = os.path.join("lib", "clang", "*.*.*", "lib", "windows")
    src_path = glob.glob(
        os.path.join(build_dir, lib_path_pattern, "clang_rt.asan_dynamic-*.pdb")
    )
    dst_path = glob.glob(os.path.join(clang_dir, lib_path_pattern))

    if len(src_path) != 1:
        raise Exception("Source path pattern did not resolve uniquely")

    if len(src_path) != 1:
        raise Exception("Destination path pattern did not resolve uniquely")

    shutil.copy2(src_path[0], dst_path[0])


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
    build_libcxx,
    osx_cross_compile,
    build_type,
    assertions,
    libcxx_include_dir,
    build_wasm,
    compiler_rt_source_dir=None,
    runtimes_source_link=None,
    compiler_rt_source_link=None,
    is_final_stage=False,
    android_targets=None,
    extra_targets=None,
    pgo_phase=None,
):
    if is_final_stage and (android_targets or extra_targets):
        # Linking compiler-rt under "runtimes" activates LLVM_RUNTIME_TARGETS
        # and related arguments.
        symlink(compiler_rt_source_dir, runtimes_source_link)
        try:
            os.unlink(compiler_rt_source_link)
        except Exception:
            pass

    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/" + package_name

    # cmake doesn't deal well with backslashes in paths.
    def slashify_path(path):
        return path.replace("\\", "/")

    def cmake_base_args(cc, cxx, asm, ld, ar, ranlib, libtool, inst_dir):
        machine_targets = "X86;ARM;AArch64" if is_final_stage else "X86"
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
            "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
            "-DLLVM_TOOL_LIBCXX_BUILD=%s" % ("ON" if build_libcxx else "OFF"),
            "-DLLVM_ENABLE_BINDINGS=OFF",
        ]
        if "TASK_ID" in os.environ:
            cmake_args += [
                "-DCLANG_REPOSITORY_STRING=taskcluster-%s" % os.environ["TASK_ID"],
            ]
        if not is_final_stage:
            cmake_args += ["-DLLVM_ENABLE_PROJECTS=clang;compiler-rt"]
        if build_wasm:
            cmake_args += ["-DLLVM_EXPERIMENTAL_TARGETS_TO_BUILD=WebAssembly"]
        if is_linux() and not osx_cross_compile:
            cmake_args += ["-DLLVM_BINUTILS_INCDIR=/usr/include"]
            cmake_args += ["-DLLVM_ENABLE_LIBXML2=FORCE_ON"]
            sysroot = os.path.join(os.environ.get("MOZ_FETCHES_DIR", ""), "sysroot")
            if os.path.exists(sysroot):
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
                "-DLIBCXXABI_LIBCXX_INCLUDES=%s" % libcxx_include_dir,
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
        if pgo_phase == "gen":
            # Per https://releases.llvm.org/10.0.0/docs/HowToBuildWithPGO.html
            cmake_args += [
                "-DLLVM_BUILD_INSTRUMENTED=IR",
                "-DLLVM_BUILD_RUNTIME=No",
            ]
        if pgo_phase == "use":
            cmake_args += [
                "-DLLVM_PROFDATA_FILE=%s/merged.profdata" % stage_dir,
            ]
        return cmake_args

    cmake_args = []

    runtime_targets = []
    if is_final_stage:
        if android_targets:
            runtime_targets = list(sorted(android_targets.keys()))
        if extra_targets:
            runtime_targets.extend(sorted(extra_targets))

    if runtime_targets:
        cmake_args += [
            "-DLLVM_BUILTIN_TARGETS=%s" % ";".join(runtime_targets),
            "-DLLVM_RUNTIME_TARGETS=%s" % ";".join(runtime_targets),
        ]

        for target in runtime_targets:
            cmake_args += [
                "-DRUNTIMES_%s_COMPILER_RT_BUILD_PROFILE=ON" % target,
                "-DRUNTIMES_%s_COMPILER_RT_BUILD_SANITIZERS=ON" % target,
                "-DRUNTIMES_%s_COMPILER_RT_BUILD_XRAY=OFF" % target,
                "-DRUNTIMES_%s_SANITIZER_ALLOW_CXXABI=OFF" % target,
                "-DRUNTIMES_%s_COMPILER_RT_BUILD_LIBFUZZER=OFF" % target,
                "-DRUNTIMES_%s_COMPILER_RT_INCLUDE_TESTS=OFF" % target,
                "-DRUNTIMES_%s_LLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF" % target,
                "-DRUNTIMES_%s_LLVM_INCLUDE_TESTS=OFF" % target,
            ]

    # The above code flipped switches to build various runtime libraries on
    # Android; we now have to provide all the necessary compiler switches to
    # make that work.
    if is_final_stage and android_targets:
        cmake_args += [
            "-DLLVM_LIBDIR_SUFFIX=64",
        ]

        android_link_flags = "-fuse-ld=lld"

        for target, cfg in android_targets.items():
            sysroot_dir = cfg["ndk_sysroot"].format(**os.environ)
            android_gcc_dir = cfg["ndk_toolchain"].format(**os.environ)
            android_include_dirs = cfg["ndk_includes"]
            api_level = cfg["api_level"]

            android_flags = [
                "-isystem %s" % d.format(**os.environ) for d in android_include_dirs
            ]
            android_flags += ["--gcc-toolchain=%s" % android_gcc_dir]
            android_flags += ["-D__ANDROID_API__=%s" % api_level]

            # Our flags go last to override any --gcc-toolchain that may have
            # been set earlier.
            rt_c_flags = " ".join(cc[1:] + android_flags)
            rt_cxx_flags = " ".join(cxx[1:] + android_flags)
            rt_asm_flags = " ".join(asm[1:] + android_flags)

            for kind in ("BUILTINS", "RUNTIMES"):
                for var, arg in (
                    ("ANDROID", "1"),
                    ("CMAKE_ASM_FLAGS", rt_asm_flags),
                    ("CMAKE_CXX_FLAGS", rt_cxx_flags),
                    ("CMAKE_C_FLAGS", rt_c_flags),
                    ("CMAKE_EXE_LINKER_FLAGS", android_link_flags),
                    ("CMAKE_SHARED_LINKER_FLAGS", android_link_flags),
                    ("CMAKE_SYSROOT", sysroot_dir),
                    ("ANDROID_NATIVE_API_LEVEL", api_level),
                ):
                    cmake_args += ["-D%s_%s_%s=%s" % (kind, target, var, arg)]

    cmake_args += cmake_base_args(cc, cxx, asm, ld, ar, ranlib, libtool, inst_dir)
    cmake_args += [src_dir]
    build_package(build_dir, cmake_args)

    # For some reasons the import library clang.lib of clang.exe is not
    # installed, so we copy it by ourselves.
    if is_windows():
        # The compiler-rt cmake scripts don't allow to build it for multiple
        # targets at once on Windows, so manually build the 32-bits compiler-rt
        # during the final stage.
        build_32_bit = False
        if is_final_stage:
            # Only build the 32-bits compiler-rt when we originally built for
            # 64-bits, which we detect through the contents of the LIB
            # environment variable, which we also adjust for a 32-bits build
            # at the same time.
            old_lib = os.environ["LIB"]
            new_lib = []
            for l in old_lib.split(os.pathsep):
                if l.endswith("x64"):
                    l = l[:-3] + "x86"
                    build_32_bit = True
                elif l.endswith("amd64"):
                    l = l[:-5]
                    build_32_bit = True
                new_lib.append(l)
        if build_32_bit:
            os.environ["LIB"] = os.pathsep.join(new_lib)
            compiler_rt_build_dir = stage_dir + "/compiler-rt"
            compiler_rt_inst_dir = inst_dir + "/lib/clang/"
            subdirs = os.listdir(compiler_rt_inst_dir)
            assert len(subdirs) == 1
            compiler_rt_inst_dir += subdirs[0]
            cmake_args = cmake_base_args(
                [os.path.join(inst_dir, "bin", "clang-cl.exe"), "-m32"] + cc[1:],
                [os.path.join(inst_dir, "bin", "clang-cl.exe"), "-m32"] + cxx[1:],
                [os.path.join(inst_dir, "bin", "clang-cl.exe"), "-m32"] + asm[1:],
                ld,
                ar,
                ranlib,
                libtool,
                compiler_rt_inst_dir,
            )
            cmake_args += [
                "-DLLVM_CONFIG_PATH=%s"
                % slashify_path(os.path.join(inst_dir, "bin", "llvm-config")),
                os.path.join(src_dir, "projects", "compiler-rt"),
            ]
            build_package(compiler_rt_build_dir, cmake_args)
            os.environ["LIB"] = old_lib
        if is_final_stage:
            install_import_library(build_dir, inst_dir)
            install_asan_symbols(build_dir, inst_dir)


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


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "-c",
        "--config",
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
    extra_source_dir = source_dir + "/clang-tools-extra"
    clang_source_dir = source_dir + "/clang"
    lld_source_dir = source_dir + "/lld"
    compiler_rt_source_dir = source_dir + "/compiler-rt"
    libcxx_source_dir = source_dir + "/libcxx"
    libcxxabi_source_dir = source_dir + "/libcxxabi"

    exe_ext = ""
    if is_windows():
        exe_ext = ".exe"

    cc_name = "clang"
    cxx_name = "clang++"
    if is_windows():
        cc_name = "clang-cl"
        cxx_name = "clang-cl"

    config_dir = os.path.dirname(args.config.name)
    config = json.load(args.config)

    stages = 3
    if "stages" in config:
        stages = int(config["stages"])
        if stages not in (1, 2, 3, 4):
            raise ValueError("We only know how to build 1, 2, 3, or 4 stages.")
    pgo = False
    if "pgo" in config:
        pgo = config["pgo"]
        if pgo not in (True, False):
            raise ValueError("Only boolean values are accepted for pgo.")
        if pgo and stages != 4:
            raise ValueError("PGO is only supported in 4-stage builds.")
    build_type = "Release"
    if "build_type" in config:
        build_type = config["build_type"]
        if build_type not in ("Release", "Debug", "RelWithDebInfo", "MinSizeRel"):
            raise ValueError(
                "We only know how to do Release, Debug, RelWithDebInfo or "
                "MinSizeRel builds"
            )
    build_libcxx = False
    if "build_libcxx" in config:
        build_libcxx = config["build_libcxx"]
        if build_libcxx not in (True, False):
            raise ValueError("Only boolean values are accepted for build_libcxx.")
    build_wasm = False
    if "build_wasm" in config:
        build_wasm = config["build_wasm"]
        if build_wasm not in (True, False):
            raise ValueError("Only boolean values are accepted for build_wasm.")
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
    ndk_dir = None
    android_targets = None
    if "android_targets" in config:
        android_targets = config["android_targets"]
        for attr in ("ndk_toolchain", "ndk_sysroot", "ndk_includes", "api_level"):
            for target, cfg in android_targets.items():
                if attr not in cfg:
                    raise ValueError(
                        "must specify '%s' as a key for android target: %s"
                        % (attr, target)
                    )
    extra_targets = None
    if "extra_targets" in config:
        extra_targets = config["extra_targets"]
        if not isinstance(extra_targets, list):
            raise ValueError("extra_targets must be a list")
        if not all(isinstance(t, str) for t in extra_targets):
            raise ValueError("members of extra_targets should be strings")

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
            patch(os.path.join(config_dir, p), source_dir)

    compiler_rt_source_link = llvm_source_dir + "/projects/compiler-rt"

    symlinks = [
        (clang_source_dir, llvm_source_dir + "/tools/clang"),
        (extra_source_dir, llvm_source_dir + "/tools/clang/tools/extra"),
        (lld_source_dir, llvm_source_dir + "/tools/lld"),
        (compiler_rt_source_dir, compiler_rt_source_link),
        (libcxx_source_dir, llvm_source_dir + "/projects/libcxx"),
        (libcxxabi_source_dir, llvm_source_dir + "/projects/libcxxabi"),
    ]
    for l in symlinks:
        # On Windows, we have to re-copy the whole directory every time.
        if not is_windows() and os.path.islink(l[1]):
            continue
        delete(l[1])
        if os.path.exists(l[0]):
            symlink(l[0], l[1])

    package_name = "clang"
    if build_clang_tidy:
        package_name = "clang-tidy"
        if not args.skip_patch:
            import_clang_tidy(
                source_dir, build_clang_tidy_alpha, build_clang_tidy_external
            )

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    libcxx_include_dir = os.path.join(llvm_source_dir, "projects", "libcxx", "include")

    stage1_dir = build_dir + "/stage1"
    stage1_inst_dir = stage1_dir + "/" + package_name

    final_stage_dir = stage1_dir
    final_inst_dir = stage1_inst_dir

    if is_darwin():
        extra_cflags = []
        extra_cxxflags = ["-stdlib=libc++"]
        extra_cflags2 = []
        extra_cxxflags2 = ["-stdlib=libc++"]
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
        # Force things on.
        extra_cflags2 = []
        extra_cxxflags2 = [
            "-fms-compatibility-version=19.15.26726",
            "-Xclang",
            "-std=c++14",
        ]
        extra_asmflags = []
        extra_ldflags = []

    if osx_cross_compile:
        # undo the damage done in the is_linux() block above, and also simulate
        # the is_darwin() block above.
        extra_cflags = []
        extra_cxxflags = ["-stdlib=libc++"]
        extra_cxxflags2 = ["-stdlib=libc++"]

        extra_flags = [
            "-target",
            "x86_64-apple-darwin",
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
        build_libcxx,
        osx_cross_compile,
        build_type,
        assertions,
        libcxx_include_dir,
        build_wasm,
        is_final_stage=(stages == 1),
    )

    runtimes_source_link = llvm_source_dir + "/runtimes/compiler-rt"

    if stages >= 2:
        stage2_dir = build_dir + "/stage2"
        stage2_inst_dir = stage2_dir + "/" + package_name
        final_stage_dir = stage2_dir
        final_inst_dir = stage2_inst_dir
        pgo_phase = "gen" if pgo else None
        build_one_stage(
            [stage1_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_cflags2,
            [stage1_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)] + extra_cxxflags2,
            [stage1_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage2_dir,
            package_name,
            build_libcxx,
            osx_cross_compile,
            build_type,
            assertions,
            libcxx_include_dir,
            build_wasm,
            compiler_rt_source_dir,
            runtimes_source_link,
            compiler_rt_source_link,
            is_final_stage=(stages == 2),
            android_targets=android_targets,
            extra_targets=extra_targets,
            pgo_phase=pgo_phase,
        )

    if stages >= 3:
        stage3_dir = build_dir + "/stage3"
        stage3_inst_dir = stage3_dir + "/" + package_name
        final_stage_dir = stage3_dir
        final_inst_dir = stage3_inst_dir
        build_one_stage(
            [stage2_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_cflags2,
            [stage2_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)] + extra_cxxflags2,
            [stage2_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage3_dir,
            package_name,
            build_libcxx,
            osx_cross_compile,
            build_type,
            assertions,
            libcxx_include_dir,
            build_wasm,
            compiler_rt_source_dir,
            runtimes_source_link,
            compiler_rt_source_link,
            (stages == 3),
            extra_targets=extra_targets,
        )

    if stages >= 4:
        stage4_dir = build_dir + "/stage4"
        stage4_inst_dir = stage4_dir + "/" + package_name
        final_stage_dir = stage4_dir
        final_inst_dir = stage4_inst_dir
        pgo_phase = None
        if pgo:
            pgo_phase = "use"
            llvm_profdata = stage3_inst_dir + "/bin/llvm-profdata%s" % exe_ext
            merge_cmd = [llvm_profdata, "merge", "-o", "merged.profdata"]
            profraw_files = glob.glob(
                os.path.join(stage2_dir, "build", "profiles", "*.profraw")
            )
            if not os.path.exists(stage4_dir):
                os.mkdir(stage4_dir)
            run_in(stage4_dir, merge_cmd + profraw_files)
        build_one_stage(
            [stage3_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_cflags2,
            [stage3_inst_dir + "/bin/%s%s" % (cxx_name, exe_ext)] + extra_cxxflags2,
            [stage3_inst_dir + "/bin/%s%s" % (cc_name, exe_ext)] + extra_asmflags,
            [ld] + extra_ldflags,
            ar,
            ranlib,
            libtool,
            llvm_source_dir,
            stage4_dir,
            package_name,
            build_libcxx,
            osx_cross_compile,
            build_type,
            assertions,
            libcxx_include_dir,
            build_wasm,
            compiler_rt_source_dir,
            runtimes_source_link,
            compiler_rt_source_link,
            (stages == 4),
            extra_targets=extra_targets,
            pgo_phase=pgo_phase,
        )

    if build_clang_tidy:
        prune_final_dir_for_clang_tidy(
            os.path.join(final_stage_dir, package_name), osx_cross_compile
        )

    # Copy the wasm32 builtins to the final_inst_dir if the archive is present.
    if "wasi-compiler-rt" in config:
        compiler_rt = config["wasi-compiler-rt"].format(**os.environ)
        if os.path.isdir(compiler_rt):
            for libdir in glob.glob(
                os.path.join(final_inst_dir, "lib", "clang", "*", "lib")
            ):
                srcdir = os.path.join(compiler_rt, "lib", "wasi")
                print("Copying from wasi-compiler-rt srcdir %s" % srcdir)
                # Copy the contents of the "lib/wasi" subdirectory to the
                # appropriate location in final_inst_dir.
                destdir = os.path.join(libdir, "wasi")
                mkdir_p(destdir)
                copy_tree(srcdir, destdir)

    if not args.skip_tar:
        build_tar_package("%s.tar.zst" % package_name, final_stage_dir, package_name)
