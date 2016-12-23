#!/bin/bash

gcc_version=4.9.4
binutils_version=2.25.1
this_path=$(readlink -f $(dirname $0))
make_flags='-j12'

root_dir="$1"
if [ -z "$root_dir" -o ! -d "$root_dir" ]; then
  root_dir=$(mktemp -d)
fi
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

wget -c -P $TMPDIR ftp://ftp.gnu.org/gnu/binutils/binutils-$binutils_version.tar.bz2 || exit 1
tar xjf $TMPDIR/binutils-$binutils_version.tar.bz2
mkdir binutils-objdir
cd binutils-objdir
# gold is disabled because we don't use it on automation, and also we ran into
# some issues with it using this script in build-clang.py.
../binutils-$binutils_version/configure --prefix /tools/gcc/ --disable-gold --enable-plugins --disable-nls || exit 1
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

patch -p1 < "${this_path}/PR64905.patch" || exit 1

cd ..
mkdir gcc-objdir
cd gcc-objdir
../gcc-$gcc_version/configure --prefix=/tools/gcc --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro || exit 1
make $make_flags || exit 1
make $make_flags install DESTDIR=$root_dir || exit 1

cd $root_dir/tools
tar caf $root_dir/gcc.tar.xz gcc/
