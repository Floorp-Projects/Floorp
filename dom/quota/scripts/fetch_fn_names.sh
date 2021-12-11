#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

# This script assumes to have rust-code-analysis-cli in the path.
HG_URL=$1
TEMPDIR=/tmp/fetch_fn_names_$BASHPID
TEMPSRC=$TEMPDIR/src
mkdir $TEMPDIR
echo "" > $TEMPDIR/empty.json
HG_URL=`echo $HG_URL | sed 's/annotate/raw-file/g'`
wget -q -O "$TEMPSRC" $HG_URL
rust-code-analysis-cli -m -O json -o "$TEMPDIR" -p "$TEMPSRC"
CONTENT=`cat $TEMPDIR/*.json`
rm -rf $TEMPDIR
echo $CONTENT
