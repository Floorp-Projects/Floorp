#!/usr/bin/python

# The directories end up in the debug info, so the easy way of getting
# a reproducible build is to run it in a know absolute directory.
# We use a directory in /builds/slave because the mozilla infrastructure
# cleans it up automatically.
base_dir = "/builds/slave/moz-toolchain"

source_dir = base_dir + "/src"
build_dir  = base_dir + "/build"
aux_inst_dir = build_dir + '/aux_inst'
old_make = aux_inst_dir + '/bin/make'

##############################################

import urllib
import os
import os.path
import shutil
import tarfile
import subprocess

def download_uri(uri):
    fname = uri.split('/')[-1]
    if (os.path.exists(fname)):
        return fname
    urllib.urlretrieve(uri, fname)
    return fname

def extract(tar, path):
    t = tarfile.open(tar)
    t.extractall(path)

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

def build_package(package_source_dir, package_build_dir, configure_args,
                   make = old_make):
    os.mkdir(package_build_dir)
    run_in(package_build_dir,
           ["%s/configure" % package_source_dir] + configure_args)
    run_in(package_build_dir, [make, "-j8"])
    run_in(package_build_dir, [make, "install"])

def build_aux_tools(base_dir):
    make_build_dir = base_dir + '/make_build'
    build_package(make_source_dir, make_build_dir,
                  ["--prefix=%s" % aux_inst_dir], "make")

    run_in(unifdef_source_dir, ["make"])
    run_in(unifdef_source_dir, ["make", "prefix=%s" % aux_inst_dir, "install"])

    tar_build_dir = base_dir + '/tar_build'
    build_package(tar_source_dir, tar_build_dir,
                  ["--prefix=%s" % aux_inst_dir])

def with_env(env, f):
    old_env = os.environ.copy()
    os.environ.update(env)
    f()
    os.environ.clear()
    os.environ.update(old_env)

def build_glibc(env, stage_dir, inst_dir):
    def f():
        build_glibc_aux(stage_dir, inst_dir)
    with_env(env, f)

def build_glibc_aux(stage_dir, inst_dir):
    glibc_build_dir = stage_dir + '/glibc'
    build_package(glibc_source_dir, glibc_build_dir,
                  ["--disable-profile",
                   "--enable-add-ons=nptl",
                   "--without-selinux",
                   "--enable-kernel=%s" % linux_version,
                   "--libdir=%s/lib64" % inst_dir,
                   "--prefix=%s" % inst_dir])

def build_linux_headers_aux(inst_dir):
    run_in(linux_source_dir, [old_make, "headers_check"])
    run_in(linux_source_dir, [old_make, "INSTALL_HDR_PATH=dest",
                               "headers_install"])
    shutil.move(linux_source_dir + "/dest", inst_dir)

def build_linux_headers(inst_dir):
    def f():
        build_linux_headers_aux(inst_dir)
    with_env({"PATH" : aux_inst_dir + "/bin:%s" % os.environ["PATH"]}, f)

def build_gcc(stage_dir, is_stage_one):
    gcc_build_dir = stage_dir + '/gcc'
    tool_inst_dir = stage_dir + '/inst'
    lib_inst_dir = stage_dir + '/libinst'
    gcc_configure_args = ["--prefix=%s" % tool_inst_dir,
                          "--enable-__cxa_atexit",
                          "--with-gmp=%s" % lib_inst_dir,
                          "--with-mpfr=%s" % lib_inst_dir,
                          "--with-mpc=%s" % lib_inst_dir,
                          "--enable-languages=c,c++",
                          "--disable-multilib",
                          "--disable-bootstrap"]
    if is_stage_one:
        # We build the stage1 gcc without shared libraries. Otherwise its
        # libgcc.so would depend on the system libc.so, which causes problems
        # when it tries to use that libgcc.so and the libc we are about to
        # build.
        gcc_configure_args.append("--disable-shared")

    build_package(gcc_source_dir, gcc_build_dir, gcc_configure_args)

    if is_stage_one:
        # The glibc build system uses -lgcc_eh, but at least in this setup
        # libgcc.a has all it needs.
        d = tool_inst_dir + "/lib/gcc/x86_64-unknown-linux-gnu/4.5.2/"
        os.symlink(d + "libgcc.a", d + "libgcc_eh.a")

def build_one_stage(env, stage_dir, is_stage_one):
    def f():
        build_one_stage_aux(stage_dir, is_stage_one)
    with_env(env, f)

def build_one_stage_aux(stage_dir, is_stage_one):
    os.mkdir(stage_dir)

    lib_inst_dir = stage_dir + '/libinst'

    gmp_build_dir = stage_dir + '/gmp'
    build_package(gmp_source_dir, gmp_build_dir,
                  ["--prefix=%s" % lib_inst_dir, "--disable-shared"])
    mpfr_build_dir = stage_dir + '/mpfr'
    build_package(mpfr_source_dir, mpfr_build_dir,
                  ["--prefix=%s" % lib_inst_dir, "--disable-shared",
                   "--with-gmp=%s" % lib_inst_dir])
    mpc_build_dir = stage_dir + '/mpc'
    build_package(mpc_source_dir, mpc_build_dir,
                  ["--prefix=%s" % lib_inst_dir, "--disable-shared",
                   "--with-gmp=%s" % lib_inst_dir,
                   "--with-mpfr=%s" % lib_inst_dir])

    tool_inst_dir = stage_dir + '/inst'
    build_linux_headers(tool_inst_dir)

    binutils_build_dir = stage_dir + '/binutils'
    build_package(binutils_source_dir, binutils_build_dir,
                  ["--prefix=%s" % tool_inst_dir])

    # During stage one we have to build gcc first, this glibc doesn't even
    # build with gcc 4.6. During stage two, we have to build glibc first.
    # The problem is that libstdc++ is built with xgcc and if glibc has
    # not been built yet xgcc will use the system one.
    if is_stage_one:
        build_gcc(stage_dir, is_stage_one)
        build_glibc({"CC"  : tool_inst_dir + "/bin/gcc",
                     "CXX" : tool_inst_dir + "/bin/g++"},
                    stage_dir, tool_inst_dir)
    else:
        build_glibc({}, stage_dir, tool_inst_dir)
        build_gcc(stage_dir, is_stage_one)

def build_tar_package(tar, name, base, directory):
    name = os.path.realpath(name)
    run_in(base, [tar, "-cf", name, "--mtime=2012-01-01", "--owner=root",
                  directory])

##############################################

def build_source_dir(prefix, version):
    return source_dir + '/' + prefix + version

binutils_version = "2.21.1"
glibc_version = "2.5.1"
linux_version = "2.6.18"
tar_version = "1.26"
make_version = "3.81"
gcc_version = "4.5.2"
mpfr_version = "2.4.2"
gmp_version = "5.0.1"
mpc_version = "0.8.1"
unifdef_version = "2.6"

binutils_source_uri = "http://ftp.gnu.org/gnu/binutils/binutils-%sa.tar.bz2" % \
    binutils_version
glibc_source_uri = "http://ftp.gnu.org/gnu/glibc/glibc-%s.tar.bz2" % \
    glibc_version
linux_source_uri = "http://www.kernel.org/pub/linux/kernel/v2.6/linux-%s.tar.bz2" % \
    linux_version
tar_source_uri = "http://ftp.gnu.org/gnu/tar/tar-%s.tar.bz2" % \
    tar_version
make_source_uri = "http://ftp.gnu.org/gnu/make/make-%s.tar.bz2" % \
    make_version
unifdef_source_uri = "http://dotat.at/prog/unifdef/unifdef-%s.tar.gz" % \
    unifdef_version
gcc_source_uri = "http://ftp.gnu.org/gnu/gcc/gcc-%s/gcc-%s.tar.bz2" % \
    (gcc_version, gcc_version)
mpfr_source_uri = "http://www.mpfr.org/mpfr-%s/mpfr-%s.tar.bz2" % \
    (mpfr_version, mpfr_version)
gmp_source_uri = "http://ftp.gnu.org/gnu/gmp/gmp-%s.tar.bz2" % gmp_version
mpc_source_uri = "http://www.multiprecision.org/mpc/download/mpc-%s.tar.gz" % \
    mpc_version

binutils_source_tar = download_uri(binutils_source_uri)
glibc_source_tar = download_uri(glibc_source_uri)
linux_source_tar = download_uri(linux_source_uri)
tar_source_tar = download_uri(tar_source_uri)
make_source_tar = download_uri(make_source_uri)
unifdef_source_tar = download_uri(unifdef_source_uri)
mpc_source_tar = download_uri(mpc_source_uri)
mpfr_source_tar = download_uri(mpfr_source_uri)
gmp_source_tar = download_uri(gmp_source_uri)
gcc_source_tar = download_uri(gcc_source_uri)

binutils_source_dir  = build_source_dir('binutils-', binutils_version)
glibc_source_dir  = build_source_dir('glibc-', glibc_version)
linux_source_dir  = build_source_dir('linux-', linux_version)
tar_source_dir  = build_source_dir('tar-', tar_version)
make_source_dir  = build_source_dir('make-', make_version)
unifdef_source_dir  = build_source_dir('unifdef-', unifdef_version)
mpc_source_dir  = build_source_dir('mpc-', mpc_version)
mpfr_source_dir = build_source_dir('mpfr-', mpfr_version)
gmp_source_dir  = build_source_dir('gmp-', gmp_version)
gcc_source_dir  = build_source_dir('gcc-', gcc_version)

if not os.path.exists(source_dir):
    os.makedirs(source_dir)
    extract(binutils_source_tar, source_dir)
    patch('binutils-deterministic.patch', 1, binutils_source_dir)
    extract(glibc_source_tar, source_dir)
    extract(linux_source_tar, source_dir)
    patch('glibc-deterministic.patch', 1, glibc_source_dir)
    run_in(glibc_source_dir, ["autoconf"])
    extract(tar_source_tar, source_dir)
    extract(make_source_tar, source_dir)
    extract(unifdef_source_tar, source_dir)
    extract(mpc_source_tar, source_dir)
    extract(mpfr_source_tar, source_dir)
    extract(gmp_source_tar, source_dir)
    extract(gcc_source_tar, source_dir)
    patch('plugin_finish_decl.diff', 0, gcc_source_dir)
    patch('pr49911.diff', 1, gcc_source_dir)
    patch('r159628-r163231-r171807.patch', 1, gcc_source_dir)
    patch('gcc-fixinc.patch', 1, gcc_source_dir)
    patch('gcc-include.patch', 1, gcc_source_dir)

if os.path.exists(build_dir):
    shutil.rmtree(build_dir)
os.makedirs(build_dir)

build_aux_tools(build_dir)

stage1_dir = build_dir + '/stage1'
build_one_stage({"CC": "gcc", "CXX" : "g++"}, stage1_dir, True)

stage1_tool_inst_dir = stage1_dir + '/inst'
stage2_dir = build_dir + '/stage2'
build_one_stage({"CC"     : stage1_tool_inst_dir + "/bin/gcc -fgnu89-inline",
                 "CXX"    : stage1_tool_inst_dir + "/bin/g++",
                 "AR"     : stage1_tool_inst_dir + "/bin/ar",
                 "RANLIB" : "true" },
                stage2_dir, False)

build_tar_package(aux_inst_dir + "/bin/tar",
                  "toolchain.tar", stage2_dir, "inst")
