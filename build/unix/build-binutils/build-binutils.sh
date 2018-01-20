#!/bin/bash

binutils_version=2.25.1
make_flags='-j12'

root_dir="$1"
if [ -z "$root_dir" -o ! -d "$root_dir" ]; then
  root_dir=$(mktemp -d)
fi
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

# Download the source of the specified version of binutils
wget -c --progress=dot:mega -P $TMPDIR ftp://ftp.gnu.org/gnu/binutils/binutils-${binutils_version}.tar.bz2 || exit 1
tar xjf $TMPDIR/binutils-${binutils_version}.tar.bz2

# Build binutils
mkdir binutils-objdir
cd binutils-objdir

../binutils-$binutils_version/configure --prefix /tools/binutils/ --enable-gold --enable-plugins --disable-nls || exit 1
make $make_flags || exit 1
make install $make_flags DESTDIR=$root_dir || exit 1

cd ..

# Make a package of the built binutils
cd $root_dir/tools
tar caf $root_dir/binutils.tar.xz binutils/
