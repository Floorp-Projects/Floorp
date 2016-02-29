#!/bin/bash

# Script used to update the Graphite2 library in the mozilla source tree

# This script lives in gfx/graphite2, along with the library source,
# but must be run from the top level of the mozilla-central tree.

# Run as
#
#    ./gfx/graphite2/moz-gr-update.sh RELEASE
#
# where RELEASE is the graphite2 release to be used, e.g. "1.3.4".

RELEASE=$1

if [ "x$RELEASE" == "x" ]
then
    echo "Must provide the version number to be used."
    exit 1
fi

TARBALL="https://github.com/silnrsi/graphite/releases/download/$RELEASE/graphite-minimal-$RELEASE.tgz"

foo=`basename $0`
TMPFILE=`mktemp -t ${foo}` || exit 1

curl -L "$TARBALL" -o "$TMPFILE"
tar -x -z -C gfx/graphite2/ --strip-components 1 -f "$TMPFILE" || exit 1
rm "$TMPFILE"

echo "This directory contains the Graphite2 library release $RELEASE from" > gfx/graphite2/README.mozilla
echo "$TARBALL" >> gfx/graphite2/README.mozilla
echo ""
echo "See" $0 "for update procedure." >> gfx/graphite2/README.mozilla

# fix up includes because of bug 721839 (cstdio) and bug 803066 (Windows.h)
#find gfx/graphite2/ -name "*.cpp" -exec perl -p -i -e "s/<cstdio>/<stdio.h>/;s/Windows.h/windows.h/;" {} \;
#find gfx/graphite2/ -name "*.h" -exec perl -p -i -e "s/<cstdio>/<stdio.h>/;s/Windows.h/windows.h/;" {} \;

# summarize what's been touched
echo Updated to $RELEASE.
echo Here is what changed in the gfx/graphite2 directory:
echo

hg stat gfx/graphite2

echo
echo If gfx/graphite2/src/files.mk has changed, please make corresponding
echo changes to gfx/graphite2/src/moz.build
echo

echo
echo Now use hg commands to create a patch for the mozilla tree.
echo
