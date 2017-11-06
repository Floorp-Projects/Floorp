# Usage: sh update.sh <upstream_src_directory>
set -e

cp -p $1/LICENSE .
cp -p $1/Cargo.toml .
for dir in cubeb-api cubeb-backend cubeb-core; do
    test -d $dir/src || mkdir -p $dir/src
    cp -pr $1/$dir/Cargo.toml $dir/
    cp -pr $1/$dir/src/* $dir/src/
done

test -d cubeb-api/libcubeb-sys || mkdir -p cubeb-api/libcubeb-sys
cp -p $1/cubeb-api/libcubeb-sys/Cargo.toml cubeb-api/libcubeb-sys/
cp -p $1/cubeb-api/libcubeb-sys/lib.rs cubeb-api/libcubeb-sys/

if [ -d $1/.git ]; then
  rev=$(cd $1 && git rev-parse --verify HEAD)
  date=$(cd $1 && git show -s --format=%ci HEAD)
  dirty=$(cd $1 && git diff-index --name-only HEAD)
fi

if [ -n "$rev" ]; then
  version=$rev
  if [ -n "$dirty" ]; then
    version=$version-dirty
    echo "WARNING: updating from a dirty git repository."
  fi
  sed -i.bak -e "/The git commit ID used was/ s/[0-9a-f]\{40\}\(-dirty\)\{0,1\} .\{1,100\}/$version ($date)/" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

echo "Applying remote-cubeb-build.patch on top of $rev"
patch -p3 < remove-cubeb-build.patch
