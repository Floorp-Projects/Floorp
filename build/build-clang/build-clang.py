#!/usr/bin/python2.7
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import os.path
import shutil
import subprocess
import platform
import json
import argparse
import glob
import errno
import re
from contextlib import contextmanager
import sys
import which
from distutils.dir_util import copy_tree


def symlink(source, link_name):
    os_symlink = getattr(os, "symlink", None)
    if callable(os_symlink):
        os_symlink(source, link_name)
    else:
        if os.path.isdir(source):
            # Fall back to copying the directory :(
            copy_tree(source, link_name)


def check_run(args):
    print >> sys.stderr, ' '.join(args)
    r = subprocess.call(args)
    assert r == 0


def run_in(path, args):
    d = os.getcwd()
    print >> sys.stderr, 'cd "%s"' % path
    os.chdir(path)
    check_run(args)
    print >> sys.stderr, 'cd "%s"' % d
    os.chdir(d)


def patch(patch, srcdir):
    patch = os.path.realpath(patch)
    check_run(['patch', '-d', srcdir, '-p1', '-i', patch, '--fuzz=0',
               '-s'])


def import_clang_tidy(source_dir):
    clang_plugin_path = os.path.join(os.path.dirname(sys.argv[0]),
                                     '..', 'clang-plugin')
    clang_tidy_path = os.path.join(source_dir,
                                   'tools/clang/tools/extra/clang-tidy')
    sys.path.append(clang_plugin_path)
    from import_mozilla_checks import do_import
    do_import(clang_plugin_path, clang_tidy_path)


def build_package(package_build_dir, cmake_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    run_in(package_build_dir, ["cmake"] + cmake_args)
    run_in(package_build_dir, ["ninja", "install"])


@contextmanager
def updated_env(env):
    old_env = os.environ.copy()
    os.environ.update(env)
    yield
    os.environ.clear()
    os.environ.update(old_env)


def build_tar_package(tar, name, base, directory):
    name = os.path.realpath(name)
    # On Windows, we have to convert this into an msys path so that tar can
    # understand it.
    if is_windows():
        name = name.replace('\\', '/')

        def f(match):
            return '/' + match.group(1).lower()
        name = re.sub(r'^([A-Za-z]):', f, name)
    run_in(base, [tar,
                  "-c",
                  "-%s" % ("J" if ".xz" in name else "j"),
                  "-f",
                  name, directory])


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


def install_libgcc(gcc_dir, clang_dir):
    gcc_bin_dir = os.path.join(gcc_dir, 'bin')

    # Copy over gcc toolchain bits that clang looks for, to ensure that
    # clang is using a consistent version of ld, since the system ld may
    # be incompatible with the output clang produces.  But copy it to a
    # target-specific directory so a cross-compiler to Mac doesn't pick
    # up the (Linux-specific) ld with disastrous results.
    x64_bin_dir = os.path.join(clang_dir, 'x86_64-unknown-linux-gnu', 'bin')
    mkdir_p(x64_bin_dir)
    shutil.copy2(os.path.join(gcc_bin_dir, 'ld'), x64_bin_dir)

    out = subprocess.check_output([os.path.join(gcc_bin_dir, "gcc"),
                                   '-print-libgcc-file-name'])

    libgcc_dir = os.path.dirname(out.rstrip())
    clang_lib_dir = os.path.join(clang_dir, "lib", "gcc",
                                 "x86_64-unknown-linux-gnu",
                                 os.path.basename(libgcc_dir))
    mkdir_p(clang_lib_dir)
    copy_tree(libgcc_dir, clang_lib_dir)
    libgcc_dir = os.path.join(gcc_dir, "lib64")
    clang_lib_dir = os.path.join(clang_dir, "lib")
    copy_tree(libgcc_dir, clang_lib_dir)
    include_dir = os.path.join(gcc_dir, "include")
    clang_include_dir = os.path.join(clang_dir, "include")
    copy_tree(include_dir, clang_include_dir)


def install_import_library(build_dir, clang_dir):
    shutil.copy2(os.path.join(build_dir, "lib", "clang.lib"),
                 os.path.join(clang_dir, "lib"))


def svn_co(source_dir, url, directory, revision):
    run_in(source_dir, ["svn", "co", "-q", "-r", revision, url, directory])


def svn_update(directory, revision):
    run_in(directory, ["svn", "revert", "-q", "-R", "."])
    run_in(directory, ["svn", "update", "-q", "-r", revision])


def is_darwin():
    return platform.system() == "Darwin"


def is_linux():
    return platform.system() == "Linux"


def is_windows():
    return platform.system() == "Windows"


def build_one_stage(cc, cxx, asm, ld, ar, ranlib, libtool,
                    src_dir, stage_dir, build_libcxx,
                    osx_cross_compile, build_type, assertions,
                    python_path, gcc_dir, libcxx_include_dir):
    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    # If CMake has already been run, it may have been run with different
    # arguments, so we need to re-run it.  Make sure the cached copy of the
    # previous CMake run is cleared before running it again.
    if os.path.exists(build_dir + "/CMakeCache.txt"):
        os.remove(build_dir + "/CMakeCache.txt")
    if os.path.exists(build_dir + "/CMakeFiles"):
        shutil.rmtree(build_dir + "/CMakeFiles")

    # cmake doesn't deal well with backslashes in paths.
    def slashify_path(path):
        return path.replace('\\', '/')

    cmake_args = ["-GNinja",
                  "-DCMAKE_C_COMPILER=%s" % slashify_path(cc[0]),
                  "-DCMAKE_CXX_COMPILER=%s" % slashify_path(cxx[0]),
                  "-DCMAKE_ASM_COMPILER=%s" % slashify_path(asm[0]),
                  "-DCMAKE_LINKER=%s" % slashify_path(ld[0]),
                  "-DCMAKE_AR=%s" % slashify_path(ar),
                  "-DCMAKE_C_FLAGS=%s" % ' '.join(cc[1:]),
                  "-DCMAKE_CXX_FLAGS=%s" % ' '.join(cxx[1:]),
                  "-DCMAKE_ASM_FLAGS=%s" % ' '.join(asm[1:]),
                  "-DCMAKE_EXE_LINKER_FLAGS=%s" % ' '.join(ld[1:]),
                  "-DCMAKE_SHARED_LINKER_FLAGS=%s" % ' '.join(ld[1:]),
                  "-DCMAKE_BUILD_TYPE=%s" % build_type,
                  "-DLLVM_TARGETS_TO_BUILD=X86;ARM;AArch64",
                  "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
                  "-DPYTHON_EXECUTABLE=%s" % slashify_path(python_path),
                  "-DCMAKE_INSTALL_PREFIX=%s" % inst_dir,
                  "-DLLVM_TOOL_LIBCXX_BUILD=%s" % ("ON" if build_libcxx else "OFF"),
                  "-DLIBCXX_LIBCPPABI_VERSION=\"\"",
                  src_dir]
    if is_windows():
        cmake_args.insert(-1, "-DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON")
        cmake_args.insert(-1, "-DLLVM_USE_CRT_RELEASE=MT")
    if ranlib is not None:
        cmake_args += ["-DCMAKE_RANLIB=%s" % slashify_path(ranlib)]
    if libtool is not None:
        cmake_args += ["-DCMAKE_LIBTOOL=%s" % slashify_path(libtool)]
    if osx_cross_compile:
        cmake_args += ["-DCMAKE_SYSTEM_NAME=Darwin",
                       "-DCMAKE_SYSTEM_VERSION=10.10",
                       "-DLLVM_ENABLE_THREADS=OFF",
                       "-DLIBCXXABI_LIBCXX_INCLUDES=%s" % libcxx_include_dir,
                       "-DCMAKE_OSX_SYSROOT=%s" % slashify_path(os.getenv("CROSS_SYSROOT")),
                       "-DCMAKE_FIND_ROOT_PATH=%s" % slashify_path(os.getenv("CROSS_CCTOOLS_PATH")), # noqa
                       "-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER",
                       "-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY",
                       "-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY",
                       "-DCMAKE_MACOSX_RPATH=ON",
                       "-DCMAKE_OSX_ARCHITECTURES=x86_64",
                       "-DDARWIN_osx_ARCHS=x86_64",
                       "-DDARWIN_osx_SYSROOT=%s" % slashify_path(os.getenv("CROSS_SYSROOT")),
                       "-DLLVM_DEFAULT_TARGET_TRIPLE=x86_64-apple-darwin11"]
    build_package(build_dir, cmake_args)

    if is_linux():
        install_libgcc(gcc_dir, inst_dir)
    # For some reasons the import library clang.lib of clang.exe is not
    # installed, so we copy it by ourselves.
    if is_windows():
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
        f = config[key]
        if os.path.isabs(f):
            if not os.path.exists(f):
                raise ValueError("%s must point to an existing path" % key)
            return f

    # Assume that we have the name of some program that should be on PATH.
    try:
        return which.which(f) if f else which.which(key)
    except which.WhichError:
        raise ValueError("%s not found on PATH" % f)


# This function is intended to be called on the final build directory when
# building clang-tidy. Also clang-format binaries are included that can be used
# in conjunction with clang-tidy.
# Its job is to remove all of the files which won't be used for clang-tidy or
# clang-format to reduce the download size.  Currently when this function
# finishes its job, it will leave final_dir with a layout like this:
#
# clang/
#   bin/
#     clang-apply-replacements
#     clang-format
#     clang-tidy
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
def prune_final_dir_for_clang_tidy(final_dir):
    # Make sure we only have what we expect.
    dirs = ("bin", "include", "lib", "libexec", "msbuild-bin", "share", "tools",
            "x86_64-unknown-linux-gnu")
    for f in glob.glob("%s/*" % final_dir):
        if os.path.basename(f) not in dirs:
            raise Exception("Found unknown file %s in the final directory" % f)
        if not os.path.isdir(f):
            raise Exception("Expected %s to be a directory" % f)

    # In bin/, only keep clang-tidy and clang-apply-replacements. The last one
    # is used to auto-fix some of the issues detected by clang-tidy.
    re_clang_tidy = re.compile(
        r"^clang-(apply-replacements|format|tidy)(\.exe)?$", re.I)
    for f in glob.glob("%s/bin/*" % final_dir):
        if re_clang_tidy.search(os.path.basename(f)) is None:
            delete(f)

    # Keep include/ intact.

    # In lib/, only keep lib/clang/N.M.O/include.
    re_ver_num = re.compile(r"^\d+\.\d+\.\d+$", re.I)
    for f in glob.glob("%s/lib/*" % final_dir):
        if os.path.basename(f) != "clang":
            delete(f)
    for f in glob.glob("%s/lib/clang/*" % final_dir):
        if re_ver_num.search(os.path.basename(f)) is None:
            delete(f)
    for f in glob.glob("%s/lib/clang/*/*" % final_dir):
        if os.path.basename(f) != "include":
            delete(f)

    # Completely remove libexec/, msbuilld-bin and tools, if it exists.
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
    parser.add_argument('-c', '--config', required=True,
                        type=argparse.FileType('r'),
                        help="Clang configuration file")
    parser.add_argument('-b', '--base-dir', required=False,
                        help="Base directory for code and build artifacts")
    parser.add_argument('--clean', required=False,
                        action='store_true',
                        help="Clean the build directory")
    parser.add_argument('--skip-tar', required=False,
                        action='store_true',
                        help="Skip tar packaging stage")

    args = parser.parse_args()

    # The directories end up in the debug info, so the easy way of getting
    # a reproducible build is to run it in a know absolute directory.
    # We use a directory that is registered as a volume in the Docker image.

    if args.base_dir:
        base_dir = args.base_dir
    elif os.environ.get('MOZ_AUTOMATION') and not is_windows():
        base_dir = "/builds/worker/workspace/moz-toolchain"
    else:
        # Handles both the Windows automation case and the local build case
        # TODO: Because Windows taskcluster builds are run with distinct
        # user IDs for each job, we can't store things in some globally
        # accessible directory: one job will run, checkout LLVM to that
        # directory, and then if another job runs, the new user won't be
        # able to access the previously-checked out code--or be able to
        # delete it.  So on Windows, we build in the task-specific home
        # directory; we will eventually add -fdebug-prefix-map options
        # to the LLVM build to bring back reproducibility.
        base_dir = os.path.join(os.getcwd(), 'build-clang')

    source_dir = base_dir + "/src"
    build_dir = base_dir + "/build"

    if not os.path.exists(base_dir):
        os.makedirs(base_dir)
    elif os.listdir(base_dir) and not os.path.exists(os.path.join(base_dir, '.build-clang')):
        raise ValueError("Base directory %s exists and is not a build-clang directory. "
                         "Supply a non-existent or empty directory with --base-dir" % base_dir)
    open(os.path.join(base_dir, '.build-clang'), 'a').close()

    if args.clean:
        shutil.rmtree(build_dir)
        os.sys.exit(0)

    llvm_source_dir = source_dir + "/llvm"
    clang_source_dir = source_dir + "/clang"
    extra_source_dir = source_dir + "/extra"
    lld_source_dir = source_dir + "/lld"
    compiler_rt_source_dir = source_dir + "/compiler-rt"
    libcxx_source_dir = source_dir + "/libcxx"
    libcxxabi_source_dir = source_dir + "/libcxxabi"

    if is_darwin():
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.7'

    exe_ext = ""
    if is_windows():
        exe_ext = ".exe"

    cc_name = "clang"
    cxx_name = "clang++"
    if is_windows():
        cc_name = "clang-cl"
        cxx_name = "clang-cl"

    config = json.load(args.config)

    llvm_revision = config["llvm_revision"]
    llvm_repo = config["llvm_repo"]
    clang_repo = config["clang_repo"]
    extra_repo = config.get("extra_repo")
    lld_repo = config.get("lld_repo")
    compiler_repo = config["compiler_repo"]
    libcxx_repo = config["libcxx_repo"]
    libcxxabi_repo = config.get("libcxxabi_repo")
    stages = 3
    if "stages" in config:
        stages = int(config["stages"])
        if stages not in (1, 2, 3):
            raise ValueError("We only know how to build 1, 2, or 3 stages")
    build_type = "Release"
    if "build_type" in config:
        build_type = config["build_type"]
        if build_type not in ("Release", "Debug", "RelWithDebInfo", "MinSizeRel"):
            raise ValueError("We only know how to do Release, Debug, RelWithDebInfo or "
                             "MinSizeRel builds")
    build_libcxx = False
    if "build_libcxx" in config:
        build_libcxx = config["build_libcxx"]
        if build_libcxx not in (True, False):
            raise ValueError("Only boolean values are accepted for build_libcxx.")
    build_clang_tidy = False
    if "build_clang_tidy" in config:
        build_clang_tidy = config["build_clang_tidy"]
        if build_clang_tidy not in (True, False):
            raise ValueError("Only boolean values are accepted for build_clang_tidy.")
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
    python_path = None
    if "python_path" not in config:
        raise ValueError("Config file needs to set python_path")
    python_path = config["python_path"]
    gcc_dir = None
    if "gcc_dir" in config:
        gcc_dir = config["gcc_dir"]
        if not os.path.exists(gcc_dir):
            raise ValueError("gcc_dir must point to an existing path")
    if is_linux() and gcc_dir is None:
        raise ValueError("Config file needs to set gcc_dir")
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

    def checkout_or_update(repo, checkout_dir):
        if os.path.exists(checkout_dir):
            svn_update(checkout_dir, llvm_revision)
        else:
            svn_co(source_dir, repo, checkout_dir, llvm_revision)

    checkout_or_update(llvm_repo, llvm_source_dir)
    checkout_or_update(clang_repo, clang_source_dir)
    checkout_or_update(compiler_repo, compiler_rt_source_dir)
    checkout_or_update(libcxx_repo, libcxx_source_dir)
    if lld_repo:
        checkout_or_update(lld_repo, lld_source_dir)
    if libcxxabi_repo:
        checkout_or_update(libcxxabi_repo, libcxxabi_source_dir)
    if extra_repo:
        checkout_or_update(extra_repo, extra_source_dir)
    for p in config.get("patches", []):
        patch(p, source_dir)

    symlinks = [(clang_source_dir,
                 llvm_source_dir + "/tools/clang"),
                (extra_source_dir,
                 llvm_source_dir + "/tools/clang/tools/extra"),
                (lld_source_dir,
                 llvm_source_dir + "/tools/lld"),
                (compiler_rt_source_dir,
                 llvm_source_dir + "/projects/compiler-rt"),
                (libcxx_source_dir,
                 llvm_source_dir + "/projects/libcxx"),
                (libcxxabi_source_dir,
                 llvm_source_dir + "/projects/libcxxabi")]
    for l in symlinks:
        # On Windows, we have to re-copy the whole directory every time.
        if not is_windows() and os.path.islink(l[1]):
            continue
        delete(l[1])
        if os.path.exists(l[0]):
            symlink(l[0], l[1])

    if build_clang_tidy:
        import_clang_tidy(llvm_source_dir)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    libcxx_include_dir = os.path.join(llvm_source_dir, "projects",
                                      "libcxx", "include")

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    final_stage_dir = stage1_dir

    if is_darwin():
        extra_cflags = []
        extra_cxxflags = ["-stdlib=libc++"]
        extra_cflags2 = []
        extra_cxxflags2 = ["-stdlib=libc++"]
        extra_asmflags = []
        extra_ldflags = []
    elif is_linux():
        extra_cflags = ["-static-libgcc"]
        extra_cxxflags = ["-static-libgcc", "-static-libstdc++"]
        # When building stage2 and stage3, we want the newly-built clang to pick
        # up whatever headers were installed from the gcc we used to build stage1,
        # always, rather than the system headers.  Providing -gcc-toolchain
        # encourages clang to do that.
        extra_cflags2 = ["-fPIC",
                         '-gcc-toolchain', stage1_inst_dir]
        # Silence clang's warnings about arguments not being used in compilation.
        extra_cxxflags2 = ["-fPIC", '-Qunused-arguments', "-static-libstdc++",
                           '-gcc-toolchain', stage1_inst_dir]
        extra_asmflags = []
        extra_ldflags = []

        if 'LD_LIBRARY_PATH' in os.environ:
            os.environ['LD_LIBRARY_PATH'] = ('%s/lib64/:%s' %
                                             (gcc_dir, os.environ['LD_LIBRARY_PATH']))
        else:
            os.environ['LD_LIBRARY_PATH'] = '%s/lib64/' % gcc_dir
    elif is_windows():
        extra_cflags = []
        extra_cxxflags = []
        # clang-cl would like to figure out what it's supposed to be emulating
        # by looking at an MSVC install, but we don't really have that here.
        # Force things on.
        extra_cflags2 = []
        extra_cxxflags2 = ['-fms-compatibility-version=19.13.26128', '-Xclang', '-std=c++14']
        extra_asmflags = []
        extra_ldflags = []

    if osx_cross_compile:
        # undo the damage done in the is_linux() block above, and also simulate
        # the is_darwin() block above.
        extra_cflags = []
        extra_cxxflags = ["-stdlib=libc++"]
        extra_cxxflags2 = ["-stdlib=libc++"]

        extra_flags = ["-target", "x86_64-apple-darwin11", "-mlinker-version=137",
                       "-B", "%s/bin" % os.getenv("CROSS_CCTOOLS_PATH"),
                       "-isysroot", os.getenv("CROSS_SYSROOT"),
                       # technically the sysroot flag there should be enough to deduce this,
                       # but clang needs some help to figure this out.
                       "-I%s/usr/include" % os.getenv("CROSS_SYSROOT"),
                       "-iframework", "%s/System/Library/Frameworks" % os.getenv("CROSS_SYSROOT")]
        extra_cflags += extra_flags
        extra_cxxflags += extra_flags
        extra_cflags2 += extra_flags
        extra_cxxflags2 += extra_flags
        extra_asmflags += extra_flags
        extra_ldflags = ["-Wl,-syslibroot,%s" % os.getenv("CROSS_SYSROOT"),
                         "-Wl,-dead_strip"]

    build_one_stage(
        [cc] + extra_cflags,
        [cxx] + extra_cxxflags,
        [asm] + extra_asmflags,
        [ld] + extra_ldflags,
        ar, ranlib, libtool,
        llvm_source_dir, stage1_dir, build_libcxx, osx_cross_compile,
        build_type, assertions, python_path, gcc_dir, libcxx_include_dir)

    if stages > 1:
        stage2_dir = build_dir + '/stage2'
        stage2_inst_dir = stage2_dir + '/clang'
        final_stage_dir = stage2_dir
        build_one_stage(
            [stage1_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_cflags2,
            [stage1_inst_dir + "/bin/%s%s" %
                (cxx_name, exe_ext)] + extra_cxxflags2,
            [stage1_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_asmflags,
            [ld] + extra_ldflags,
            ar, ranlib, libtool,
            llvm_source_dir, stage2_dir, build_libcxx, osx_cross_compile,
            build_type, assertions, python_path, gcc_dir, libcxx_include_dir)

    if stages > 2:
        stage3_dir = build_dir + '/stage3'
        final_stage_dir = stage3_dir
        build_one_stage(
            [stage2_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_cflags2,
            [stage2_inst_dir + "/bin/%s%s" %
                (cxx_name, exe_ext)] + extra_cxxflags2,
            [stage2_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_asmflags,
            [ld] + extra_ldflags,
            ar, ranlib, libtool,
            llvm_source_dir, stage3_dir, build_libcxx, osx_cross_compile,
            build_type, assertions, python_path, gcc_dir, libcxx_include_dir)

    package_name = "clang"
    if build_clang_tidy:
        prune_final_dir_for_clang_tidy(os.path.join(final_stage_dir, "clang"))
        package_name = "clang-tidy"

    if not args.skip_tar:
        ext = "bz2" if is_darwin() or is_windows() else "xz"
        build_tar_package("tar", "%s.tar.%s" % (package_name, ext), final_stage_dir, "clang")
