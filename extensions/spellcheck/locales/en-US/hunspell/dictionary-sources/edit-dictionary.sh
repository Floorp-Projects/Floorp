#! /usr/bin/env sh

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -e

if [ -z "$EDITOR" ]; then
  echo 'Need to set the $EDITOR environment variable to your favorite editor.'
  exit 1
fi

# Copy the current en-US dictionary and strip the first line that contains
# the count.
tail -n +2 ../en-US.dic > en-US.stripped

# Convert the file to UTF-8
iconv -f iso-8859-1 -t utf-8 en-US.stripped > en-US.utf8
rm en-US.stripped

# Open the hunspell dictionary and let the user edit it
echo "Now the dictionary is going to be opened for you to edit. Quit the editor to finish editing."
echo "Press Enter to begin."
read foo
$EDITOR en-US.utf8

# Add back the line count and sort the lines
wc -l < en-US.utf8 | tr -d '[:blank:]' > en-US.dic
LC_ALL=C sort en-US.utf8 >> en-US.dic
rm -f en-US.utf8

# Convert back to ISO-8859-1
iconv -f utf-8 -t iso-8859-1 en-US.dic > ../en-US.dic

# Keep a copy of the UTF-8 file in /utf8
mv en-US.dic utf8/en-US-utf8.dic
