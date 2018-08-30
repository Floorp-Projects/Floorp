#!/bin/bash

binutils_version=2.28.1
make_flags="-j$(nproc)"

root_dir="$1"

cd $root_dir

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
