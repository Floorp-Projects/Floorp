#!/bin/bash

set -e
set -x

make_flags="-j$(nproc)"

. $data_dir/download-tools.sh

prepare() {
  pushd $root_dir
  download_and_check ftp://ftp.gnu.org/gnu/binutils binutils-$binutils_version.tar.$binutils_ext.sig
  tar xaf $TMPDIR/binutils-$binutils_version.tar.$binutils_ext

  case "$gcc_version" in
  *-*)
    download ftp://gcc.gnu.org/pub/gcc/snapshots/$gcc_version/gcc-$gcc_version.tar.$gcc_ext
    ;;
  *)
    download_and_check ftp://ftp.gnu.org/gnu/gcc/gcc-$gcc_version gcc-$gcc_version.tar.$gcc_ext.sig
    ;;
  esac
  tar xaf $TMPDIR/gcc-$gcc_version.tar.$gcc_ext
  cd gcc-$gcc_version

  (
    # Divert commands that download_prerequisites use
    ln() { :; }
    tar() { :; }
    sed() { :; }
    wget() {
      echo $1
    }

    . ./contrib/download_prerequisites
  ) | while read url; do
    file=$(basename $url)
    case "$file" in
    gmp-*.tar.*)
      # If download_prerequisites wants 4.3.2, use 5.1.3 instead.
      file=${file/4.3.2/5.1.3}
      download_and_check https://gmplib.org/download/gmp $file.sig
      ;;
    mpfr-*.tar.*)
      # If download_prerequisites wants 2.4.2, use 3.1.5 instead.
      file=${file/2.4.2/3.1.5}
      download_and_check http://www.mpfr.org/${file%.tar.*} $file.asc
      ;;
    mpc-*.tar.*)
      # If download_prerequisites wants 0.8.1, use 0.8.2 instead.
      file=${file/0.8.1/0.8.2}
      download_and_check http://www.multiprecision.org/mpc/download $file.asc
      ;;
    *)
      download $(dirname $url) $file
      ;;
    esac
    tar xaf $TMPDIR/$file
    ln -sf ${file%.tar.*} ${file%-*}
  done

  # Check all the downloads we did are in the checksums list, and that the
  # checksums match.
  diff -u <(sort -k 2 $root_dir/downloads) $root_dir/checksums

  popd
}

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
    pushd $root_dir/gcc-$gcc_version
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
    binutils_configure_flags="--disable-gold --enable-plugins --disable-nls"
  fi

  mkdir $root_dir/binutils-objdir
  pushd $root_dir/binutils-objdir
  ../binutils-$binutils_version/configure --prefix=${prefix-/tools/gcc}/ $binutils_configure_flags
  make $make_flags
  make install $make_flags DESTDIR=$root_dir
  export PATH=$root_dir/${prefix-/tools/gcc}/bin:$PATH
  popd
}

build_gcc() {
  mkdir $root_dir/gcc-objdir
  pushd $root_dir/gcc-objdir
  ../gcc-$gcc_version/configure --prefix=${prefix-/tools/gcc} --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro --disable-initfini-array
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
  ../gcc-$gcc_version/configure --prefix=$install_dir --target=i686-w64-mingw32 --with-gnu-ld --with-gnu-as --disable-multilib --enable-threads=posix
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
