#!/bin/bash

set -e

set -x

gcc_version=4.9.4
binutils_version=2.25.1
this_path=$(readlink -f $(dirname $0))
make_flags='-j12'

no_build=
if [ "$1" = "--no-build" ]; then
    no_build=1
    shift
fi

root_dir="$1"
if [ -z "$root_dir" -o ! -d "$root_dir" ]; then
  root_dir=$(mktemp -d)
fi
cd $root_dir

if test -z $TMPDIR; then
  TMPDIR=/tmp/
fi

mkdir gpg
GPG="gpg --homedir $root_dir/gpg"
# GPG key used to sign GCC
$GPG --import $this_path/13975A70E63C361C73AE69EF6EEB81F8981C74C7.key
# GPG key used to sign binutils
$GPG --import $this_path/EAF1C276A747E9ED86210CBAC3126D3B4AE55E93.key
# GPG key used to sign GMP
$GPG --import $this_path/343C2FF0FBEE5EC2EDBEF399F3599FF828C67298.key
# GPG key used to sign MPFR
$GPG --import $this_path/07F3DBBECC1A39605078094D980C197698C3739D.key
# GPG key used to sign MPC
$GPG --import $this_path/AD17A21EF8AED8F1CC02DBD9F7D5C9BF765C61E3.key

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

download_and_check ftp://ftp.gnu.org/gnu/binutils binutils-$binutils_version.tar.bz2.sig
tar xjf $TMPDIR/binutils-$binutils_version.tar.bz2
mkdir binutils-objdir
cd binutils-objdir
# gold is disabled because we don't use it on automation, and also we ran into
# some issues with it using this script in build-clang.py.
../binutils-$binutils_version/configure --prefix /tools/gcc/ --disable-gold --enable-plugins --disable-nls
make $make_flags
make install $make_flags DESTDIR=$root_dir
cd ..

case "$gcc_version" in
*-*)
  download ftp://gcc.gnu.org/pub/gcc/snapshots/$gcc_version/gcc-$gcc_version.tar.bz2
  ;;
*)
  download_and_check ftp://ftp.gnu.org/gnu/gcc/gcc-$gcc_version gcc-$gcc_version.tar.bz2.sig
  ;;
esac
tar xjf $TMPDIR/gcc-$gcc_version.tar.bz2
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
diff -u <(sort -k 2 $root_dir/downloads) $this_path/checksums

patch -p1 < "${this_path}/PR64905.patch"

if [ -n "$no_build" ]; then
    exit 0
fi

cd ..
mkdir gcc-objdir
cd gcc-objdir
../gcc-$gcc_version/configure --prefix=/tools/gcc --enable-languages=c,c++  --disable-nls --disable-gnu-unique-object --enable-__cxa_atexit --with-arch-32=pentiumpro
make $make_flags
make $make_flags install DESTDIR=$root_dir

cd $root_dir/tools
tar caf $root_dir/gcc.tar.xz gcc/
