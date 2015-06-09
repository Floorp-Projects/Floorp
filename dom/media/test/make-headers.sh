# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# Script to generate ^header^ files for all media files we use.
# This is to ensure that our media files are not cached by necko,
# so that our detection as to whether the server supports byte range
# requests is not interferred with by Necko's cache. See bug 977398
# for details. Necko will fix this in bug 977314. 

FILES=(`ls *.ogg *.ogv *.webm *.mp3 *.opus *.mp4 *.m4s *.wav`)

rm -f *.ogg^headers^ *.ogv^headers^ *.webm^headers^ *.mp3^headers^ *.opus^headers^ *.mp4^headers^ *.m4s^headers^ *.wav^headers^

for i in "${FILES[@]}"
do
  echo "Cache-Control: no-store" >> $i^headers^
done
