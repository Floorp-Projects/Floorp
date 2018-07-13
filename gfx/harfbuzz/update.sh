#!/bin/sh

# Script to update the mozilla in-tree copy of the HarfBuzz library.
# Run this within the /gfx/harfbuzz directory of the source tree.

MY_TEMP_DIR=`mktemp -d -t harfbuzz_update.XXXXXX` || exit 1

VERSION=1.8.3

git clone https://github.com/harfbuzz/harfbuzz ${MY_TEMP_DIR}/harfbuzz
git -C ${MY_TEMP_DIR}/harfbuzz checkout ${VERSION}

COMMIT=$(git -C ${MY_TEMP_DIR}/harfbuzz rev-parse HEAD)
perl -p -i -e "s/(\d+\.)(\d+\.)(\d+)/${VERSION}/" README-mozilla;
perl -p -i -e "s/\[commit [0-9a-f]{40}\]/[commit ${COMMIT}]/" README-mozilla;

FILES="AUTHORS autogen.sh configure.ac COPYING git.mk harfbuzz.doap Makefile.am NEWS README src THANKS TODO"

for f in $FILES; do
	rm -rf $f
	mv ${MY_TEMP_DIR}/harfbuzz/$f $f
done
rm -rf src/hb-ucdn
rm -rf ${MY_TEMP_DIR}

hg revert -r . src/moz.build
hg addremove

echo "###"
echo "### Updated HarfBuzz to $COMMIT."
echo "### Remember to verify and commit the changes to source control!"
echo "###"
