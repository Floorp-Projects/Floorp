# Usage: sh update.sh <upstream_src_directory>
set -e

if [ ! -d "$1" ]; then
  echo "Usage: ./update.sh /path/to/kiss_fft" > 2
  exit 1
fi

FILES="CHANGELOG \
       COPYING \
       README \
       README.simd \
       _kiss_fft_guts.h \
       kiss_fft.c \
       kiss_fft.h \
       tools/kiss_fftr.c \
       tools/kiss_fftr.h"

for file in $FILES; do
  cp "$1/$file" .
done

if [ -d "$1/.hg" ]; then
  rev=$(cd "$1" && hg log --template='{node}' -r `hg identify -i`)
fi

if [ -n "$rev" ]; then
  version=$rev
  sed -i.bak -e "/The hg revision ID used was/ s/[0-9a-f]\{40\}\./$version./" README_MOZILLA
  rm README_MOZILLA.bak
else
  echo "Remember to update README_MOZILLA with the version details."
fi

