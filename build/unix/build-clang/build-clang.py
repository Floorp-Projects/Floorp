#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

llvm_revision = "159219"
moz_version = "moz0"

##############################################

import os
import os.path
import shutil
import tarfile
import subprocess
import platform

def check_run(args):
    r = subprocess.call(args)
    assert r == 0

def run_in(path, args):
    d = os.getcwd()
    os.chdir(path)
    check_run(args)
    os.chdir(d)

def patch(patch, plevel, srcdir):
    patch = os.path.realpath(patch)
    check_run(['patch', '-d', srcdir, '-p%s' % plevel, '-i', patch, '--fuzz=0',
               '-s'])

def build_package(package_source_dir, package_build_dir, configure_args):
    if not os.path.exists(package_build_dir):
        os.mkdir(package_build_dir)
    run_in(package_build_dir,
           ["%s/configure" % package_source_dir] + configure_args)
    run_in(package_build_dir, ["make", "-j8"])
    run_in(package_build_dir, ["make", "install"])

def with_env(env, f):
    old_env = os.environ.copy()
    os.environ.update(env)
    f()
    os.environ.clear()
    os.environ.update(old_env)

def build_tar_package(tar, name, base, directory):
    name = os.path.realpath(name)
    run_in(base, [tar, "-cjf", name, directory])

def svn_co(url, directory, revision):
    check_run(["svn", "co", "-r", revision, url, directory])

# The directories end up in the debug info, so the easy way of getting
# a reproducible build is to run it in a know absolute directory.
# We use a directory in /builds/slave because the mozilla infrastructure
# cleans it up automatically.
base_dir = "/builds/slave/moz-toolchain"

source_dir = base_dir + "/src"
build_dir  = base_dir + "/build"

llvm_source_dir = source_dir + "/llvm"
clang_source_dir = source_dir + "/clang"
compiler_rt_source_dir = source_dir + "/compiler-rt"

def build_one_stage(env, stage_dir, is_stage_one):
    def f():
        build_one_stage_aux(stage_dir, is_stage_one)
    with_env(env, f)

def build_one_stage_aux(stage_dir, is_stage_one):
    os.mkdir(stage_dir)

    build_dir = stage_dir + "/build"
    inst_dir = stage_dir + "/clang"

    configure_opts = ["--enable-optimized",
                      "--prefix=%s" % inst_dir,
                      "--with-gcc-toolchain=/tools/gcc-4.5-0moz3"]
    if is_stage_one:
        configure_opts.append("--with-optimize-option=-O0")

    build_package(llvm_source_dir, build_dir, configure_opts)

isDarwin = platform.system() == "Darwin"

if not os.path.exists(source_dir):
    os.makedirs(source_dir)
    svn_co("http://llvm.org/svn/llvm-project/llvm/trunk",
           llvm_source_dir, llvm_revision)
    svn_co("http://llvm.org/svn/llvm-project/cfe/trunk",
           clang_source_dir, llvm_revision)
    svn_co("http://llvm.org/svn/llvm-project/compiler-rt/trunk",
           compiler_rt_source_dir, llvm_revision)
    os.symlink("../../clang", llvm_source_dir + "/tools/clang")
    os.symlink("../../compiler-rt", llvm_source_dir + "/projects/compiler-rt")
    if not isDarwin:
        patch("old-ld-hack.patch", 1, llvm_source_dir)
        patch("compiler-rt-gnu89-inline.patch", 0, compiler_rt_source_dir)

if os.path.exists(build_dir):
    shutil.rmtree(build_dir)
os.makedirs(build_dir)

stage1_dir = build_dir + '/stage1'
stage1_inst_dir = stage1_dir + '/clang'

if isDarwin:
    extra_cflags = ""
    extra_cxxflags = ""
    cc = "/usr/bin/clang"
    cxx = "/usr/bin/clang++"
else:
    extra_cflags = "-static-libgcc"
    extra_cxxflags = "-static-libgcc -static-libstdc++"
    cc = "/tools/gcc-4.5-0moz3/bin/gcc %s" % extra_cflags
    cxx = "/tools/gcc-4.5-0moz3/bin/g++ %s" % extra_cxxflags

build_one_stage({"CC"  : cc,
                 "CXX" : cxx },
                stage1_dir, True)

if not isDarwin:
    extra_cflags += " -fgnu89-inline"

stage2_dir = build_dir + '/stage2'
build_one_stage({"CC"  : stage1_inst_dir + "/bin/clang %s" % extra_cflags,
                 "CXX" : stage1_inst_dir + "/bin/clang++ %s" % extra_cxxflags},
                stage2_dir, False)

build_tar_package("tar", "clang.tar.bz2", stage2_dir, "clang")
