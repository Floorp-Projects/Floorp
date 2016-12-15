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
import tempfile
import glob
import errno
import re
from contextlib import contextmanager
import sys
import which

DEBUG = os.getenv("DEBUG")


def symlink(source, link_name):
    os_symlink = getattr(os, "symlink", None)
    if callable(os_symlink):
        os_symlink(source, link_name)
    else:
        if os.path.isdir(source):
            # Fall back to copying the directory :(
            copy_dir_contents(source, link_name)


def check_run(args):
    global DEBUG
    if DEBUG:
        print >> sys.stderr, ' '.join(args)
    r = subprocess.call(args)
    assert r == 0


def run_in(path, args):
    d = os.getcwd()
    global DEBUG
    if DEBUG:
        print >> sys.stderr, 'cd "%s"' % path
    os.chdir(path)
    check_run(args)
    if DEBUG:
        print >> sys.stderr, 'cd "%s"' % d
    os.chdir(d)


def patch(patch, srcdir):
    patch = os.path.realpath(patch)
    check_run(['patch', '-d', srcdir, '-p1', '-i', patch, '--fuzz=0',
               '-s'])


def build_package(package_build_dir, run_cmake, cmake_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    if run_cmake:
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


def copy_dir_contents(src, dest):
    for f in glob.glob("%s/*" % src):
        try:
            destname = "%s/%s" % (dest, os.path.basename(f))
            if os.path.isdir(f):
                shutil.copytree(f, destname)
            else:
                shutil.copy2(f, destname)
        except OSError as e:
            if e.errno == errno.ENOTDIR:
                shutil.copy2(f, destname)
            elif e.errno == errno.EEXIST:
                if os.path.isdir(f):
                    copy_dir_contents(f, destname)
                else:
                    os.remove(destname)
                    shutil.copy2(f, destname)
            else:
                raise Exception('Directory not copied. Error: %s' % e)


def mkdir_p(path):
    try:
        os.makedirs(path)
    except OSError as e:
        if e.errno != errno.EEXIST or not os.path.isdir(path):
            raise


def install_libgcc(gcc_dir, clang_dir):
    out = subprocess.check_output([os.path.join(gcc_dir, "bin", "gcc"),
                                   '-print-libgcc-file-name'])

    libgcc_dir = os.path.dirname(out.rstrip())
    clang_lib_dir = os.path.join(clang_dir, "lib", "gcc",
                                 "x86_64-unknown-linux-gnu",
                                 os.path.basename(libgcc_dir))
    mkdir_p(clang_lib_dir)
    copy_dir_contents(libgcc_dir, clang_lib_dir)
    libgcc_dir = os.path.join(gcc_dir, "lib64")
    clang_lib_dir = os.path.join(clang_dir, "lib")
    copy_dir_contents(libgcc_dir, clang_lib_dir)
    include_dir = os.path.join(gcc_dir, "include")
    clang_include_dir = os.path.join(clang_dir, "include")
    copy_dir_contents(include_dir, clang_include_dir)


def install_import_library(build_dir, clang_dir):
    shutil.copy2(os.path.join(build_dir, "lib", "clang.lib"),
                 os.path.join(clang_dir, "lib"))


def svn_co(source_dir, url, directory, revision):
    run_in(source_dir, ["svn", "co", "-q", "-r", revision, url, directory])


def svn_update(directory, revision):
    run_in(directory, ["svn", "update", "-q", "-r", revision])


def get_platform():
    p = platform.system()
    if p == "Darwin":
        return "macosx64"
    elif p == "Linux":
        if platform.architecture() == "AMD64":
            return "linux64"
        else:
            return "linux32"
    elif p == "Windows":
        if platform.architecture() == "AMD64":
            return "win64"
        else:
            return "win32"
    else:
        raise NotImplementedError("Not supported platform")


def is_darwin():
    return platform.system() == "Darwin"


def is_linux():
    return platform.system() == "Linux"


def is_windows():
    return platform.system() == "Windows"


def build_one_stage(cc, cxx, src_dir, stage_dir, build_libcxx,
                    build_type, assertions, python_path, gcc_dir):
    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    run_cmake = True
    if os.path.exists(build_dir + "/build.ninja"):
        run_cmake = False

    # cmake doesn't deal well with backslashes in paths.
    def slashify_path(path):
        return path.replace('\\', '/')

    cmake_args = ["-GNinja",
                  "-DCMAKE_C_COMPILER=%s" % slashify_path(cc[0]),
                  "-DCMAKE_CXX_COMPILER=%s" % slashify_path(cxx[0]),
                  "-DCMAKE_ASM_COMPILER=%s" % slashify_path(cc[0]),
                  "-DCMAKE_C_FLAGS=%s" % ' '.join(cc[1:]),
                  "-DCMAKE_CXX_FLAGS=%s" % ' '.join(cxx[1:]),
                  "-DCMAKE_BUILD_TYPE=%s" % build_type,
                  "-DLLVM_TARGETS_TO_BUILD=X86;ARM",
                  "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
                  "-DPYTHON_EXECUTABLE=%s" % slashify_path(python_path),
                  "-DCMAKE_INSTALL_PREFIX=%s" % inst_dir,
                  "-DLLVM_TOOL_LIBCXX_BUILD=%s" % ("ON" if build_libcxx else "OFF"),
                  "-DLIBCXX_LIBCPPABI_VERSION=\"\"",
                  src_dir];
    if is_windows():
        cmake_args.insert(-1, "-DLLVM_EXPORT_SYMBOLS_FOR_PLUGINS=ON")
    build_package(build_dir, run_cmake, cmake_args)

    if is_linux():
        install_libgcc(gcc_dir, inst_dir)
    # For some reasons the import library clang.lib of clang.exe is not
    # installed, so we copy it by ourselves.
    if is_windows():
        install_import_library(build_dir, inst_dir)

def get_compiler(config, key):
    if key not in config:
        raise ValueError("Config file needs to set %s" % key)

    f = config[key]
    if os.path.isabs(f):
        if not os.path.exists(f):
            raise ValueError("%s must point to an existing path" % key)
        return f

    # Assume that we have the name of some program that should be on PATH.
    try:
        return which.which(f)
    except which.WhichError:
        raise ValueError("%s not found on PATH" % f)

if __name__ == "__main__":
    # The directories end up in the debug info, so the easy way of getting
    # a reproducible build is to run it in a know absolute directory.
    # We use a directory in /builds/slave because the mozilla infrastructure
    # cleans it up automatically.
    base_dir = "/builds/slave/moz-toolchain"
    if is_windows():
        # TODO: Because Windows taskcluster builds are run with distinct
        # user IDs for each job, we can't store things in some globally
        # accessible directory: one job will run, checkout LLVM to that
        # directory, and then if another job runs, the new user won't be
        # able to access the previously-checked out code--or be able to
        # delete it.  So on Windows, we build in the task-specific home
        # directory; we will eventually add -fdebug-prefix-map options
        # to the LLVM build to bring back reproducibility.
        base_dir = os.path.join(os.getcwd(), 'llvm-sources')

    source_dir = base_dir + "/src"
    build_dir = base_dir + "/build"

    llvm_source_dir = source_dir + "/llvm"
    clang_source_dir = source_dir + "/clang"
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

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', required=True,
                        type=argparse.FileType('r'),
                        help="Clang configuration file")
    parser.add_argument('--clean', required=False,
                        action='store_true',
                        help="Clean the build directory")

    args = parser.parse_args()
    config = json.load(args.config)

    if args.clean:
        shutil.rmtree(build_dir)
        os.sys.exit(0)

    llvm_revision = config["llvm_revision"]
    llvm_repo = config["llvm_repo"]
    clang_repo = config["clang_repo"]
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
            raise ValueError("We only know how to do Release, Debug, RelWithDebInfo or MinSizeRel builds")
    build_libcxx = False
    if "build_libcxx" in config:
        build_libcxx = config["build_libcxx"]
        if build_libcxx not in (True, False):
            raise ValueError("Only boolean values are accepted for build_libcxx.")
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
    cc = get_compiler(config, "cc")
    cxx = get_compiler(config, "cxx")

    if not os.path.exists(source_dir):
        os.makedirs(source_dir)
        svn_co(source_dir, llvm_repo, llvm_source_dir, llvm_revision)
        svn_co(source_dir, clang_repo, clang_source_dir, llvm_revision)
        svn_co(source_dir, compiler_repo, compiler_rt_source_dir, llvm_revision)
        svn_co(source_dir, libcxx_repo, libcxx_source_dir, llvm_revision)
        if libcxxabi_repo:
            svn_co(source_dir, libcxxabi_repo, libcxxabi_source_dir, llvm_revision)
        for p in config.get("patches", {}).get(get_platform(), []):
            patch(p, source_dir)
    else:
        svn_update(llvm_source_dir, llvm_revision)
        svn_update(clang_source_dir, llvm_revision)
        svn_update(compiler_rt_source_dir, llvm_revision)
        svn_update(libcxx_source_dir, llvm_revision)
        if libcxxabi_repo:
            svn_update(libcxxabi_source_dir, llvm_revision)

    symlinks = [(source_dir + "/clang",
                 llvm_source_dir + "/tools/clang"),
                (source_dir + "/compiler-rt",
                 llvm_source_dir + "/projects/compiler-rt"),
                (source_dir + "/libcxx",
                 llvm_source_dir + "/projects/libcxx"),
                (source_dir + "/libcxxabi",
                 llvm_source_dir + "/projects/libcxxabi")]
    for l in symlinks:
        # On Windows, we have to re-copy the whole directory every time.
        if not is_windows() and os.path.islink(l[1]):
            continue
        if os.path.isdir(l[1]):
            shutil.rmtree(l[1])
        elif os.path.exists(l[1]):
            os.unlink(l[1])
        if os.path.exists(l[0]):
            symlink(l[0], l[1])

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    final_stage_dir = stage1_dir

    if is_darwin():
        extra_cflags = []
        extra_cxxflags = ["-stdlib=libc++"]
        extra_cflags2 = []
        extra_cxxflags2 = ["-stdlib=libc++"]
    elif is_linux():
        extra_cflags = ["-static-libgcc"]
        extra_cxxflags = ["-static-libgcc", "-static-libstdc++"]
        extra_cflags2 = ["-fPIC"]
        extra_cxxflags2 = ["-fPIC", "-static-libstdc++"]

        if os.environ.has_key('LD_LIBRARY_PATH'):
            os.environ['LD_LIBRARY_PATH'] = '%s/lib64/:%s' % (gcc_dir, os.environ['LD_LIBRARY_PATH']);
        else:
            os.environ['LD_LIBRARY_PATH'] = '%s/lib64/' % gcc_dir
    elif is_windows():
        extra_cflags = []
        extra_cxxflags = []
        # clang-cl would like to figure out what it's supposed to be emulating
        # by looking at an MSVC install, but we don't really have that here.
        # Force things on.
        extra_cflags2 = []
        extra_cxxflags2 = ['-fms-compatibility-version=19.00.23918', '-Xclang', '-std=c++14']

    build_one_stage(
        [cc] + extra_cflags,
        [cxx] + extra_cxxflags,
        llvm_source_dir, stage1_dir, build_libcxx,
        build_type, assertions, python_path, gcc_dir)

    if stages > 1:
        stage2_dir = build_dir + '/stage2'
        stage2_inst_dir = stage2_dir + '/clang'
        final_stage_dir = stage2_dir
        build_one_stage(
            [stage1_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_cflags2,
            [stage1_inst_dir + "/bin/%s%s" %
                (cxx_name, exe_ext)] + extra_cxxflags2,
            llvm_source_dir, stage2_dir, build_libcxx,
            build_type, assertions, python_path, gcc_dir)

    if stages > 2:
        stage3_dir = build_dir + '/stage3'
        final_stage_dir = stage3_dir
        build_one_stage(
            [stage2_inst_dir + "/bin/%s%s" %
                (cc_name, exe_ext)] + extra_cflags2,
            [stage2_inst_dir + "/bin/%s%s" %
                (cxx_name, exe_ext)] + extra_cxxflags2,
            llvm_source_dir, stage3_dir, build_libcxx,
            build_type, assertions, python_path, gcc_dir)

    if is_darwin() or is_windows():
        build_tar_package("tar", "clang.tar.bz2", final_stage_dir, "clang")
    else:
        build_tar_package("tar", "clang.tar.xz", final_stage_dir, "clang")
