#! /usr/bin/env sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script creates a new dictionary by expanding the original,
# Mozilla's, and the upstream dictionary to remove affix flags and
# then doing the wordlist equivalent of diff3 to create a new
# dictionary.
#
# The files 2-mozilla-add and 2-mozilla-rem contain words added and
# removed, respectively in the Mozilla dictionary. The final
# dictionary will be in hunspell-en_US-mozilla.zip.

set -e

export LANG=C
export LC_ALL=C
export LC_CTYPE=C
export LC_COLLATE=C

WKDIR="`pwd`"

export SCOWL="$WKDIR/scowl/"

ORIG="$WKDIR/orig/"
SUPPORT_DIR="$WKDIR/support_files/"
SPELLER="$SCOWL/speller"

expand() {
  grep -v '^[0-9]\+$' | $SPELLER/munch-list expand $1 | sort -u
}

mkdir -p $SUPPORT_DIR
cd $SPELLER
MK_LIST="../mk-list -v1 --accents=both en_US 60"
cat <<EOF > params.txt
With Input Command: $MK_LIST
EOF
# Note: the output of make-hunspell-dict is UTF-8
$MK_LIST | ./make-hunspell-dict -one en_US-custom params.txt > ./make-hunspell-dict.log
cd $WKDIR

# Note: Input and output of "expand" is always ISO-8859-1.
#       All expanded word list files are thus in ISO-8859-1.
expand $SPELLER/en.aff < $SPELLER/en.dic.supp > $SUPPORT_DIR/0-special.txt

# Input is UTF-8, expand expects ISO-8859-1 so use iconv
iconv -f utf-8 -t iso-8859-1 $ORIG/en_US-custom.dic | expand $ORIG/en_US-custom.aff > $SUPPORT_DIR/1-base.txt

# The existing Mozilla dictionary is already in ISO-8859-1
expand ../en-US.aff < ../en-US.dic > $SUPPORT_DIR/2-mozilla.txt

# Input is UTF-8, expand expects ISO-8859-1 so use iconv
iconv -f utf-8 -t iso-8859-1 $SPELLER/en_US-custom.dic | expand $SPELLER/en_US-custom.aff > $SUPPORT_DIR/3-upstream.txt

# Suppress common lines and lines only in the 2nd file, leaving words that are
# only available in the 1st file (SCOWL), i.e. were removed by Mozilla.
comm -23 $SUPPORT_DIR/1-base.txt $SUPPORT_DIR/2-mozilla.txt > $SUPPORT_DIR/2-mozilla-removed.txt

# Suppress common lines and lines only in the 1st file, leaving words that are
# only available in the 2nd file (current Mozilla dictionary), i.e. were added
# by Mozilla.
comm -13 $SUPPORT_DIR/1-base.txt $SUPPORT_DIR/2-mozilla.txt > $SUPPORT_DIR/2-mozilla-added.txt

# Suppress common lines and lines only in the 2nd file, leaving words that are
# only available in the 1st file (words from the new upstream SCOWL dictionary).
# The result is upstream, minus the words removed, plus the words added.
comm -23 $SUPPORT_DIR/3-upstream.txt $SUPPORT_DIR/2-mozilla-removed.txt | cat - $SUPPORT_DIR/2-mozilla-added.txt | sort -u > $SUPPORT_DIR/4-patched.txt

# Note: the output of make-hunspell-dict is UTF-8
cat $SUPPORT_DIR/4-patched.txt | comm -23 - $SUPPORT_DIR/0-special.txt | $SPELLER/make-hunspell-dict -one en_US-mozilla /dev/null

# Exclude specific words from suggestions
while IFS= read -r line
do
  # If the string already contains an affix, just add !, otherwise add /!
  if [[ "$line" == *"/"* ]]; then
    sed -i "" "s|^$line$|$line!|" en_US-mozilla.dic
  else
    sed -i "" "s|^$line$|$line/!|" en_US-mozilla.dic
  fi
done < "mozilla-exclusions.txt"

# Sanity check should yield identical results
#comm -23 $SUPPORT_DIR/1-base.txt $SUPPORT_DIR/3-upstream.txt > $SUPPORT_DIR/3-upstream-remover.txt
#comm -13 $SUPPORT_DIR/1-base.txt $SUPPORT_DIR/3-upstream.txt > $SUPPORT_DIR/3-upstream-added.txt
#comm -23 $SUPPORT_DIR/2-mozilla.txt $SUPPORT_DIR/3-upstream-removed.txt | cat - $SUPPORT_DIR/3-upstream-added.txt | sort -u > $SUPPORT_DIR/4-patched-v2.txt

expand ../en-US.aff < mozilla-specific.txt > 5-mozilla-specific.txt

# Update Mozilla removed and added wordlists based on the new upstream
# dictionary, save them as UTF-8 and not ISO-8951-1
comm -12 $SUPPORT_DIR/3-upstream.txt $SUPPORT_DIR/2-mozilla-removed.txt > $SUPPORT_DIR/5-mozilla-removed.txt
iconv -f iso-8859-1 -t utf-8 $SUPPORT_DIR/5-mozilla-removed.txt > 5-mozilla-removed.txt
comm -13 $SUPPORT_DIR/3-upstream.txt $SUPPORT_DIR/2-mozilla-added.txt > $SUPPORT_DIR/5-mozilla-added.txt
iconv -f iso-8859-1 -t utf-8 $SUPPORT_DIR/5-mozilla-added.txt > 5-mozilla-added.txt

# Clean up some files
rm hunspell-en_US-mozilla.zip
rm nosug
