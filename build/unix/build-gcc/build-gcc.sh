#!/bin/bash

set -e
set -x

make_flags="-j$(nproc)"

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

  if [ -d $MOZ_FETCHES_DIR/sysroot ]; then
    EXTRA_CONFIGURE_FLAGS="--with-build-sysroot=$MOZ_FETCHES_DIR/sysroot"
    export CFLAGS_FOR_BUILD="--sysroot=$MOZ_FETCHES_DIR/sysroot"
  fi
  ../gcc-source/configure --prefix=${prefix-/tools/gcc} --build=x86_64-unknown-linux-gnu --target="${target}" --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro --with-sysroot=/ $EXTRA_CONFIGURE_FLAGS
  make $make_flags
  make $make_flags install-strip DESTDIR=$root_dir

  cd $root_dir/tools
  ln -s gcc gcc/bin/cc

  tar caf $root_dir/gcc.tar.zst gcc/
  popd
}
