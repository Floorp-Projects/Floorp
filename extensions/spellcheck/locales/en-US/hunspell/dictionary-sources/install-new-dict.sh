#! /usr/bin/env sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script copies the new dictionary created by make-new-dict in
# place.

set -e

WKDIR="`pwd`"
export SCOWL="$WKDIR/scowl/"
SUPPORT_DIR="$WKDIR/support_files/"
SPELLER="$SCOWL/speller"

if [ -e "$SUPPORT_DIR/orig-bk" ]; then
    echo "$0: directory '$SUPPORT_DIR/orig-bk' exists." 1>&2
    exit 0
fi

mv orig "$SUPPORT_DIR/orig-bk"
mkdir orig
cp $SPELLER/en_US-custom.dic $SPELLER/en_US-custom.aff $SPELLER/README_en_US-custom.txt orig

mkdir "$SUPPORT_DIR/mozilla-bk"
mv ../en-US.dic ../en-US.aff ../README_en_US.txt "$SUPPORT_DIR/mozilla-bk"

# Convert the affix file to ISO-8859-1
cp en_US-mozilla.aff utf8/en-US-utf8.aff
sed -i "" -e '/^ICONV/d' -e 's/^SET UTF-8$/SET ISO8859-1/' en_US-mozilla.aff

# Convert the dictionary to ISO-8859-1
mv en_US-mozilla.dic utf8/en-US-utf8.dic
iconv -f utf-8 -t iso-8859-1 < utf8/en-US-utf8.dic > en_US-mozilla.dic

cp en_US-mozilla.aff ../en-US.aff
cp en_US-mozilla.dic ../en-US.dic
mv README_en_US-mozilla.txt ../README_en_US.txt

echo "New dictionary copied into place. Please commit the changes."
