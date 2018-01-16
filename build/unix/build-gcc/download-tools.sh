#!/bin/bash

set -e
set -x

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
  wget -c --progress=dot:mega -P $TMPDIR $1/$2
  (cd $TMPDIR; sha256sum $2) >> $root_dir/downloads
}

download_and_check() {
  download $1 ${2%.*}
  wget -c --progress=dot:mega -P $TMPDIR $1/$2
  $GPG --verify $TMPDIR/$2 $TMPDIR/${2%.*}
}
