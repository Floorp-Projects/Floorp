#!/bin/bash

set -e
set -x

make_flags="-j$(nproc)"

prepare_mingw() {
  export prefix=/tools/mingw32
  export install_dir=$root_dir$prefix
  mkdir -p $install_dir
  export PATH=$PATH:$install_dir/bin/

  cd $root_dir

  git clone -n git://git.code.sf.net/p/mingw-w64/mingw-w64
  pushd mingw-w64
  git checkout $mingw_version # Asserts the integrity of the checkout (Right?)
  popd
}

apply_patch() {
  if [ $# -ge 2 ]; then
    pushd $root_dir/$1
    shift
  else
    pushd $root_dir/gcc-source
  fi
  patch -p1 < $1
  popd
}

build_binutils() {
  # if binutils_configure_flags is not set at all, give it the default value
  if [ -z "${binutils_configure_flags+xxx}" ];
  then
    # gold is disabled because we don't use it on automation, and also we ran into
    # some issues with it using this script in build-clang.py.
    #
    # --enable-targets builds extra target support in ld.
    # Enabling aarch64 support brings in arm support, so we don't need to specify that too.
    #
    # It is important to have the binutils --target and the gcc --target match,
    # so binutils will install binaries in a place that gcc will look for them.
    binutils_configure_flags="--enable-targets=aarch64-unknown-linux-gnu --build=x86_64-unknown-linux-gnu --target=x86_64-unknown-linux-gnu --disable-gold --enable-plugins --disable-nls --with-sysroot=/"
  fi

  mkdir $root_dir/binutils-objdir
  pushd $root_dir/binutils-objdir
  ../binutils-source/configure --prefix=${prefix-/tools/gcc}/ $binutils_configure_flags
  make $make_flags
  make install $make_flags DESTDIR=$root_dir
  export PATH=$root_dir/${prefix-/tools/gcc}/bin:$PATH
  popd
}

build_gcc() {
  # Be explicit about --build and --target so header and library install
  # directories are consistent.
  local target="${1:-x86_64-unknown-linux-gnu}"

  mkdir $root_dir/gcc-objdir
  pushd $root_dir/gcc-objdir
  ../gcc-source/configure --prefix=${prefix-/tools/gcc} --build=x86_64-unknown-linux-gnu --target="${target}" --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro --with-sysroot=/
  make $make_flags
  make $make_flags install DESTDIR=$root_dir

  cd $root_dir/tools
  ln -s gcc gcc/bin/cc

  tar caf $root_dir/gcc.tar.xz gcc/
  popd
}

build_gcc_and_mingw() {
  mkdir gcc-objdir
  pushd gcc-objdir
  ../gcc-source/configure --prefix=$install_dir --target=i686-w64-mingw32 --with-gnu-ld --with-gnu-as --disable-multilib --enable-threads=posix
  make $make_flags all-gcc
  make $make_flags install-gcc
  popd

  mkdir mingw-w64-headers32
  pushd mingw-w64-headers32
  ../mingw-w64/mingw-w64-headers/configure --host=i686-w64-mingw32 --prefix=$install_dir/i686-w64-mingw32/ --enable-sdk=all --enable-secure-api --enable-idl
  make $make_flags install
  popd

  mkdir mingw-w64-crt32
  pushd mingw-w64-crt32
  ../mingw-w64/mingw-w64-crt/configure --host=i686-w64-mingw32 --prefix=$install_dir/i686-w64-mingw32/
  make
  make install
  popd

  mkdir mingw-w64-pthread
  pushd mingw-w64-pthread
  ../mingw-w64/mingw-w64-libraries/winpthreads/configure --host=i686-w64-mingw32 --prefix=$install_dir/i686-w64-mingw32/
  make
  make install
  popd

  pushd gcc-objdir
  make
  make install
  popd

  mkdir widl32
  pushd widl32
  ../mingw-w64/mingw-w64-tools/widl/configure --prefix=$install_dir --target=i686-w64-mingw32
  make
  make install
  popd

  pushd $(dirname $install_dir)
  tar caf $root_dir/mingw32.tar.xz $(basename $install_dir)/
  popd
}
