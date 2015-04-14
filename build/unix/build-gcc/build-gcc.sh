#!/bin/bash

gcc_version=4.7.3
binutils_version=2.23.1
this_path=$(readlink -f $(dirname $0))
gcc_bt_patch=${this_path}/gcc-bt.patch
gcc_pr55650_patch=${this_path}/gcc48-pr55650.patch
make_flags='-j12'

root_dir=$(mktemp -d)
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

wget -c -P $TMPDIR ftp://ftp.gnu.org/gnu/binutils/binutils-$binutils_version.tar.bz2 || exit 1
tar xjf $TMPDIR/binutils-$binutils_version.tar.bz2
mkdir binutils-objdir
cd binutils-objdir
../binutils-$binutils_version/configure --prefix /tools/gcc/ --enable-gold  --enable-plugins --disable-nls || exit 1
make $make_flags || exit 1
make install $make_flags DESTDIR=$root_dir || exit 1
cd ..

case "$gcc_version" in
*-*)
  wget -c -P $TMPDIR ftp://gcc.gnu.org/pub/gcc/snapshots/$gcc_version/gcc-$gcc_version.tar.bz2 || exit 1
  ;;
*)
  wget -c -P $TMPDIR ftp://ftp.gnu.org/gnu/gcc/gcc-$gcc_version/gcc-$gcc_version.tar.bz2 || exit 1
  ;;
esac
tar xjf $TMPDIR/gcc-$gcc_version.tar.bz2
cd gcc-$gcc_version

./contrib/download_prerequisites

# gcc 4.7 doesn't dump a stack on ICE so hack that in
patch -p1 < $gcc_bt_patch || exit 1

patch -p0 < $gcc_pr55650_patch || exit 1

cd ..
mkdir gcc-objdir
cd gcc-objdir
../gcc-$gcc_version/configure --prefix=/tools/gcc --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro || exit 1
make $make_flags || exit 1
make $make_flags install DESTDIR=$root_dir || exit 1

cd $root_dir/tools
tar caf $root_dir/gcc.tar.xz gcc/
