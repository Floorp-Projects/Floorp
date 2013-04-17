#!/bin/bash

# Script used to update the Graphite2 library in the mozilla source tree

# This script lives in gfx/graphite2, along with the library source,
# but must be run from the top level of the mozilla-central tree.

# It expects to find a checkout of the graphite2 tree in a directory "graphitedev"
# alongside the current mozilla tree that is to be updated.
# Expect error messages from the copy commands if this is not found!

# copy the source and headers
cp -R ../graphitedev/src/* gfx/graphite2/src
cp ../graphitedev/include/graphite2/* gfx/graphite2/include/graphite2

# record the upstream changeset that was used
CHANGESET=$(cd ../graphitedev/ && hg log | head -n 1 | cut -d : -f 1,3 | sed -e 's/:/ /')
echo "This directory contains the Graphite2 library from http://hg.palaso.org/graphitedev\n" > gfx/graphite2/README.mozilla
echo "Current version derived from upstream" $CHANGESET >> gfx/graphite2/README.mozilla
echo "\nSee" $0 "for update procedure.\n" >> gfx/graphite2/README.mozilla

# fix up includes because of bug 721839 (cstdio) and bug 803066 (Windows.h)
find gfx/graphite2/ -name "*.cpp" -exec perl -p -i -e "s/<cstdio>/<stdio.h>/;s/Windows.h/windows.h/;" {} \;
find gfx/graphite2/ -name "*.h" -exec perl -p -i -e "s/<cstdio>/<stdio.h>/;s/Windows.h/windows.h/;" {} \;

# summarize what's been touched
echo Updated to $CHANGESET.
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
