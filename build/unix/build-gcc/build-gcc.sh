#!/bin/bash

set -e
set -x

make_flags='-j12'

if [ -z "$root_dir" -o ! -d "$root_dir" ]; then
  root_dir=$(mktemp -d)
fi

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

mkdir $root_dir/gpg
GPG="gpg --homedir $root_dir/gpg"

> $root_dir/downloads

download() {
  wget -c -P $TMPDIR $1/$2
  (cd $TMPDIR; sha256sum $2) >> $root_dir/downloads
}

download_and_check() {
  download $1 ${2%.*}
  wget -c -P $TMPDIR $1/$2
  $GPG --verify $TMPDIR/$2 $TMPDIR/${2%.*}
}

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

apply_patch() {
  pushd $root_dir/gcc-$gcc_version
  patch -p1 < $1
  popd
}

build_binutils() {
  mkdir $root_dir/binutils-objdir
  pushd $root_dir/binutils-objdir
  # gold is disabled because we don't use it on automation, and also we ran into
  # some issues with it using this script in build-clang.py.
  ../binutils-$binutils_version/configure --prefix /tools/gcc/ --disable-gold --enable-plugins --disable-nls
  make $make_flags
  make install $make_flags DESTDIR=$root_dir
  popd
}

build_gcc() {
  mkdir $root_dir/gcc-objdir
  pushd $root_dir/gcc-objdir
  ../gcc-$gcc_version/configure --prefix=/tools/gcc --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro
  make $make_flags
  make $make_flags install DESTDIR=$root_dir

  cd $root_dir/tools
  tar caf $root_dir/gcc.tar.xz gcc/
  popd
}
