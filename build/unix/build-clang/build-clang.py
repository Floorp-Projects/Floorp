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
from contextlib import contextmanager
import sys

DEBUG = os.getenv("DEBUG")

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
    run_in(base, [tar,
                  "-c",
                  "-%s" % ("J" if ".xz" in name else "j"),
                  "-f",
                  name, directory])


def copy_dir_contents(src, dest):
    for f in glob.glob("%s/*" % src):
        try:
            destname = "%s/%s" % (dest, os.path.basename(f))
            shutil.copytree(f, destname)
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


def build_and_use_libgcc(env, clang_dir):
    with updated_env(env):
        tempdir = tempfile.mkdtemp()
        gcc_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                               "..", "build-gcc")
        run_in(gcc_dir, ["./build-gcc.sh", tempdir, "libgcc"])
        run_in(tempdir, ["tar", "-xf", "gcc.tar.xz"])
        libgcc_dir = glob.glob(os.path.join(tempdir,
                                            "gcc", "lib", "gcc",
                                            "x86_64-unknown-linux-gnu",
                                            "[0-9]*"))[0]
        clang_lib_dir = os.path.join(clang_dir, "lib", "gcc",
                                     "x86_64-unknown-linux-gnu",
                                     os.path.basename(libgcc_dir))
        mkdir_p(clang_lib_dir)
        copy_dir_contents(libgcc_dir, clang_lib_dir)
        libgcc_dir = os.path.join(tempdir, "gcc", "lib64")
        clang_lib_dir = os.path.join(clang_dir, "lib")
        copy_dir_contents(libgcc_dir, clang_lib_dir)
        include_dir = os.path.join(tempdir, "gcc", "include")
        clang_include_dir = os.path.join(clang_dir, "include")
        copy_dir_contents(include_dir, clang_include_dir)
        shutil.rmtree(tempdir)


def svn_co(url, directory, revision):
    check_run(["svn", "co", "-r", revision, url, directory])


def svn_update(directory, revision):
    run_in(directory, ["svn", "update", "-r", revision])


def build_one_stage(env, src_dir, stage_dir, build_libcxx, build_type, assertions,
                    python_path):
    with updated_env(env):
        build_one_stage_aux(src_dir, stage_dir, build_libcxx, build_type,
                            assertions, python_path)


def get_platform():
    p = platform.system()
    if p == "Darwin":
        return "macosx64"
    elif p == "Linux":
        if platform.processor() == "x86_64":
            return "linux64"
        else:
            return "linux32"
    else:
        raise NotImplementedError("Not supported platform")


def is_darwin():
    return platform.system() == "Darwin"


def build_one_stage_aux(src_dir, stage_dir, build_libcxx,
                        build_type, assertions, python_path):
    if not os.path.exists(stage_dir):
        os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    run_cmake = True
    if os.path.exists(build_dir):
        run_cmake = False

    cmake_args = ["-GNinja",
                  "-DCMAKE_BUILD_TYPE=%s" % build_type,
                  "-DLLVM_TARGETS_TO_BUILD=X86;ARM",
                  "-DLLVM_ENABLE_ASSERTIONS=%s" % ("ON" if assertions else "OFF"),
                  "-DPYTHON_EXECUTABLE=%s" % python_path,
                  "-DCMAKE_INSTALL_PREFIX=%s" % inst_dir,
                  "-DLLVM_TOOL_LIBCXX_BUILD=%s" % ("ON" if build_libcxx else "OFF"),
                  "-DLIBCXX_LIBCPPABI_VERSION=\"\"",
                  src_dir];
    build_package(build_dir, run_cmake, cmake_args)

if __name__ == "__main__":
    # The directories end up in the debug info, so the easy way of getting
    # a reproducible build is to run it in a know absolute directory.
    # We use a directory in /builds/slave because the mozilla infrastructure
    # cleans it up automatically.
    base_dir = "/builds/slave/moz-toolchain"

    source_dir = base_dir + "/src"
    build_dir = base_dir + "/build"

    llvm_source_dir = source_dir + "/llvm"
    clang_source_dir = source_dir + "/clang"
    compiler_rt_source_dir = source_dir + "/compiler-rt"
    libcxx_source_dir = source_dir + "/libcxx"

    if is_darwin():
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.7'

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
    if not is_darwin() and gcc_dir is None:
        raise ValueError("Config file needs to set gcc_dir")
    cc = None
    if "cc" in config:
        cc = config["cc"]
        if not os.path.exists(cc):
            raise ValueError("cc must point to an existing path")
    else:
        raise ValueError("Config file needs to set cc")
    cxx = None
    if "cxx" in config:
        cxx = config["cxx"]
        if not os.path.exists(cxx):
            raise ValueError("cxx must point to an existing path")
    else:
        raise ValueError("Config file needs to set cxx")

    if not os.path.exists(source_dir):
        os.makedirs(source_dir)
        svn_co(llvm_repo, llvm_source_dir, llvm_revision)
        svn_co(clang_repo, clang_source_dir, llvm_revision)
        svn_co(compiler_repo, compiler_rt_source_dir, llvm_revision)
        svn_co(libcxx_repo, libcxx_source_dir, llvm_revision)
        os.symlink("../../clang", llvm_source_dir + "/tools/clang")
        os.symlink("../../compiler-rt",
                   llvm_source_dir + "/projects/compiler-rt")
        os.symlink("../../libcxx",
                   llvm_source_dir + "/projects/libcxx")
        for p in config.get("patches", {}).get(get_platform(), []):
            patch(p, source_dir)
    else:
        svn_update(llvm_source_dir, llvm_revision)
        svn_update(clang_source_dir, llvm_revision)
        svn_update(compiler_rt_source_dir, llvm_revision)
        svn_update(libcxx_source_dir, llvm_revision)

    if not os.path.exists(build_dir):
        os.makedirs(build_dir)

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    final_stage_dir = stage1_dir

    if is_darwin():
        extra_cflags = ""
        extra_cxxflags = "-stdlib=libc++"
        extra_cflags2 = ""
        extra_cxxflags2 = "-stdlib=libc++"
    else:
        extra_cflags = "-static-libgcc"
        extra_cxxflags = "-static-libgcc -static-libstdc++"
        extra_cflags2 = "-fPIC --gcc-toolchain=%s" % gcc_dir
        extra_cxxflags2 = "-fPIC --gcc-toolchain=%s" % gcc_dir

        if os.environ.has_key('LD_LIBRARY_PATH'):
            os.environ['LD_LIBRARY_PATH'] = '%s/lib64/:%s' % (gcc_dir, os.environ['LD_LIBRARY_PATH']);
        else:
            os.environ['LD_LIBRARY_PATH'] = '%s/lib64/' % gcc_dir

    build_one_stage(
        {"CC": cc + " %s" % extra_cflags,
         "CXX": cxx + " %s" % extra_cxxflags},
        llvm_source_dir, stage1_dir, build_libcxx,
        build_type, assertions, python_path)

    if stages > 1:
        stage2_dir = build_dir + '/stage2'
        stage2_inst_dir = stage2_dir + '/clang'
        final_stage_dir = stage2_dir
        build_one_stage(
            {"CC": stage1_inst_dir + "/bin/clang %s" % extra_cflags2,
             "CXX": stage1_inst_dir + "/bin/clang++ %s" % extra_cxxflags2},
            llvm_source_dir, stage2_dir, build_libcxx,
            build_type, assertions, python_path)

        if stages > 2:
            stage3_dir = build_dir + '/stage3'
            final_stage_dir = stage3_dir
            build_one_stage(
                {"CC": stage2_inst_dir + "/bin/clang %s" % extra_cflags2,
                 "CXX": stage2_inst_dir + "/bin/clang++ %s" % extra_cxxflags2},
                llvm_source_dir, stage3_dir, build_libcxx,
                build_type, assertions, python_path)

    if not is_darwin():
        final_stage_inst_dir = final_stage_dir + '/clang'
        build_and_use_libgcc(
            {"CC": cc + " %s" % extra_cflags,
             "CXX": cxx + " %s" % extra_cxxflags},
            final_stage_inst_dir)

    if is_darwin():
        build_tar_package("tar", "clang.tar.bz2", final_stage_dir, "clang")
    else:
        build_tar_package("tar", "clang.tar.xz", final_stage_dir, "clang")
