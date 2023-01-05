#! /usr/bin/env sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

WKDIR="`pwd`"
SPELLER="$WKDIR/scowl/speller"

munch() {
  $SPELLER/munch-list munch $1 | sort -u
}

expand() {
  grep -v '^[0-9]\+$' | $SPELLER/munch-list expand $1 | sort -u
}

if [ ! -d "$SPELLER" ]; then
  echo "The 'scowl' folder is missing. Check the documentation at"
  echo "https://firefox-source-docs.mozilla.org/extensions/spellcheck/index.html"
  exit 1
fi

if [ -z "$EDITOR" ]; then
  echo 'Need to set the $EDITOR environment variable to your favorite editor.'
  exit 1
fi

# Open the editor and allow the user to type or paste words
echo "Editor is going to open, you can add the list of words. Quit the editor to finish editing."
echo "Press Enter to begin."
read foo
$EDITOR temp-list.txt

if [ ! -f temp-list.txt ]; then
  echo "The content of the editor hasn't been saved."
  exit 1
fi
# Remove empty lines
sed -i "" "/^$/d" temp-list.txt

# Copy the current en-US dictionary and strip the first line that contains
# the count.
tail -n +2 ../en-US.dic > en-US.stripped

# Convert the file to UTF-8
iconv -f iso-8859-1 -t utf-8 en-US.stripped > en-US.utf8
rm en-US.stripped

# Save to a temporary file words excluded from suggestions, and numerals,
# since the munched result is different for both.
grep '!$' < utf8/en-US-utf8.dic > en-US-nosug.txt
grep '^[0-9][a-z/]' < utf8/en-US-utf8.dic > en-US-numerals.txt

# Expand the dictionary to a word list
expand ../en-US.aff < en-US.utf8 > en-US-wordlist.txt
rm en-US.utf8

# Add the new words
cat temp-list.txt >> en-US-wordlist.txt
rm temp-list.txt

# Remove numerals from the expanded wordlist
grep -v '^[0-9]' < en-US-wordlist.txt > en-US-wordlist-nonum.txt
rm en-US-wordlist.txt

# Run the wordlist through the munch script, to compress the dictionary where
# possible (using affix rules).
munch ../en-US.aff < en-US-wordlist-nonum.txt > en-US-munched.dic
rm en-US-wordlist-nonum.txt

# Remove words that should not be suggested
while IFS='/' read -ra line
do
  sed -E -i "" "\:^$line($|/.*):d" en-US-munched.dic
done < "en-US-nosug.txt"

# Add back suggestion exclusions and numerals from the original .dic file
cat en-US-nosug.txt >> en-US-munched.dic
cat en-US-numerals.txt >> en-US-munched.dic
rm en-US-nosug.txt
rm en-US-numerals.txt

# Add back the line count and sort the lines
wc -l < en-US-munched.dic | tr -d '[:blank:]' > en-US.dic
LC_ALL=C sort en-US-munched.dic >> en-US.dic
rm -f en-US-munched.dic

# Convert back to ISO-8859-1
iconv -f utf-8 -t iso-8859-1 en-US.dic > ../en-US.dic

# Keep a copy of the UTF-8 file in /utf8
mv en-US.dic utf8/en-US-utf8.dic
