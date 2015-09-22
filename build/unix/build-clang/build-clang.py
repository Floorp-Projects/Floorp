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

centOS6 = False


def check_run(args):
    r = subprocess.call(args)
    assert r == 0


def run_in(path, args):
    d = os.getcwd()
    os.chdir(path)
    check_run(args)
    os.chdir(d)


def patch(patch, srcdir):
    patch = os.path.realpath(patch)
    check_run(['patch', '-d', srcdir, '-p1', '-i', patch, '--fuzz=0',
               '-s'])


def build_package(package_source_dir, package_build_dir, configure_args,
                  make_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    run_in(package_build_dir,
           ["%s/configure" % package_source_dir] + configure_args)
    run_in(package_build_dir, ["make", "-j4"] + make_args)
    run_in(package_build_dir, ["make", "install"])


@contextmanager
def updated_env(env):
    old_env = os.environ.copy()
    os.environ.update(env)
    yield
    os.environ.clear()
    os.environ.update(old_env)


def build_tar_package(tar, name, base, directory):
    name = os.path.realpath(name)
    run_in(base, [tar, "-cJf", name, directory])


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


def build_one_stage(env, stage_dir, llvm_source_dir):
    with updated_env(env):
        build_one_stage_aux(stage_dir, llvm_source_dir)


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


def build_one_stage_aux(stage_dir, llvm_source_dir):
    os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    targets = ["x86", "x86_64"]
    # The Darwin equivalents of binutils appear to have intermittent problems
    # with objects in compiler-rt that are compiled for arm.  Since the arm
    # support is only necessary for iOS (which we don't support), only enable
    # arm support on Linux.
    if not is_darwin():
        targets.append("arm")

    global centOS6
    if centOS6:
        python_path = "/usr/bin/python2.7"
    else:
        python_path = "/usr/local/bin/python2.7"

    configure_opts = ["--enable-optimized",
                      "--enable-targets=" + ",".join(targets),
                      "--disable-assertions",
                      "--disable-libedit",
                      "--with-python=%s" % python_path,
                      "--prefix=%s" % inst_dir,
                      "--disable-compiler-version-checks"]
    build_package(llvm_source_dir, build_dir, configure_opts, [])

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

    global centOS6
    if centOS6:
        gcc_dir = "/home/worker/workspace/build/src/gcc"
    else:
        gcc_dir = "/tools/gcc-4.7.3-0moz1"

    if is_darwin():
        os.environ['MACOSX_DEPLOYMENT_TARGET'] = '10.7'

    parser = argparse.ArgumentParser()
    parser.add_argument('-c', '--config', required=True,
                        type=argparse.FileType('r'),
                        help="Clang configuration file")

    args = parser.parse_args()
    config = json.load(args.config)
    llvm_revision = config["llvm_revision"]
    llvm_repo = config["llvm_repo"]
    clang_repo = config["clang_repo"]
    compiler_repo = config["compiler_repo"]
    libcxx_repo = config["libcxx_repo"]

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

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    if is_darwin():
        extra_cflags = ""
        extra_cxxflags = "-stdlib=libc++"
        extra_cflags2 = ""
        extra_cxxflags2 = "-stdlib=libc++"
        cc = "/usr/bin/clang"
        cxx = "/usr/bin/clang++"
    else:
        extra_cflags = ""
        extra_cxxflags = ""
        extra_cflags2 = "-static-libgcc --gcc-toolchain=%s" % gcc_dir
        extra_cxxflags2 = "-static-libgcc -static-libstdc++ --gcc-toolchain=%s" % gcc_dir
        cc = gcc_dir + "/bin/gcc"
        cxx = gcc_dir + "/bin/g++"

    if os.environ.has_key('LD_LIBRARY_PATH'):
        os.environ['LD_LIBRARY_PATH'] = '%s/lib64/:%s' % (gcc_dir, os.environ['LD_LIBRARY_PATH']);
    else:
        os.environ['LD_LIBRARY_PATH'] = '%s/lib64/' % gcc_dir

    build_one_stage(
        {"CC": cc + " %s" % extra_cflags,
         "CXX": cxx + " %s" % extra_cxxflags},
        stage1_dir, llvm_source_dir)

    stage2_dir = build_dir + '/stage2'
    build_one_stage(
        {"CC": stage1_inst_dir + "/bin/clang %s" % extra_cflags2,
         "CXX": stage1_inst_dir + "/bin/clang++ %s" % extra_cxxflags2},
        stage2_dir, llvm_source_dir)

    if not is_darwin():
        stage2_inst_dir = stage2_dir + '/clang'
        build_and_use_libgcc(
            {"CC": cc + " %s" % extra_cflags,
             "CXX": cxx + " %s" % extra_cxxflags},
            stage2_inst_dir)

    build_tar_package("tar", "clang.tar.xz", stage2_dir, "clang")
