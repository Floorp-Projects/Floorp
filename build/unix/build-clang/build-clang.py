#!/usr/bin/python
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

import os
import os.path
import shutil
import subprocess
import platform
import sys
import json
import collections
import argparse


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
    run_in(package_build_dir, ["make", "-j8"] + make_args)
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


def build_one_stage(env, stage_dir, llvm_source_dir):
    def f():
        build_one_stage_aux(stage_dir, llvm_source_dir)
    with_env(env, f)


def build_tooltool_manifest(llvm_revision):
    basedir = os.path.split(os.path.realpath(sys.argv[0]))[0]
    tooltool = basedir + '/tooltool.py'
    setup = basedir + '/setup.sh'
    manifest = 'clang.manifest'
    check_run(['python', tooltool, '-m', manifest, 'add',
               setup, 'clang.tar.bz2'])
    data = json.load(file(manifest), object_pairs_hook=collections.OrderedDict)
    data = [{'clang_version': 'r%s' % llvm_revision}] + data
    out = file(manifest, 'w')
    json.dump(data, out, indent=0)
    out.write('\n')

    assert data[2]['filename'] == 'clang.tar.bz2'
    os.rename('clang.tar.bz2', data[2]['digest'])


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

    configure_opts = ["--enable-optimized",
                      "--enable-targets=" + ",".join(targets),
                      "--disable-assertions",
                      "--prefix=%s" % inst_dir,
                      "--with-gcc-toolchain=/tools/gcc-4.7.3-0moz1"]
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

    if not os.path.exists(source_dir):
        os.makedirs(source_dir)
        svn_co(llvm_repo, llvm_source_dir, llvm_revision)
        svn_co(clang_repo, clang_source_dir, llvm_revision)
        svn_co(compiler_repo, compiler_rt_source_dir, llvm_revision)
        os.symlink("../../clang", llvm_source_dir + "/tools/clang")
        os.symlink("../../compiler-rt",
                   llvm_source_dir + "/projects/compiler-rt")
        for p in config.get("patches", {}).get(get_platform(), []):
            patch(p, source_dir)

    if os.path.exists(build_dir):
        shutil.rmtree(build_dir)
    os.makedirs(build_dir)

    stage1_dir = build_dir + '/stage1'
    stage1_inst_dir = stage1_dir + '/clang'

    if is_darwin():
        extra_cflags = ""
        extra_cxxflags = ""
        cc = "/usr/bin/clang"
        cxx = "/usr/bin/clang++"
    else:
        extra_cflags = "-static-libgcc"
        extra_cxxflags = "-static-libgcc -static-libstdc++"
        cc = "/usr/bin/gcc"
        cxx = "/usr/bin/g++"

    build_one_stage({"CC": cc, "CXX": cxx}, stage1_dir, llvm_source_dir)

    stage2_dir = build_dir + '/stage2'
    build_one_stage(
        {"CC": stage1_inst_dir + "/bin/clang %s" % extra_cflags,
         "CXX": stage1_inst_dir + "/bin/clang++ %s" % extra_cxxflags},
        stage2_dir, llvm_source_dir)

    build_tar_package("tar", "clang.tar.bz2", stage2_dir, "clang")
    build_tooltool_manifest(llvm_revision)
