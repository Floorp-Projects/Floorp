#!/bin/sh
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.


LD_LIBRARY_PATH=.
export LD_LIBRARY_PATH

PROG=mozilla-bin
PLIBS="-L."

TOOL=hiprof

for l in ./*.so components/*.so; do
    PLIBS="$PLIBS -incobj $l"
done

$ECHO atom $PROG -tool $TOOL -env threads -toolargs="-calltime -systime" -all $PLIBS

cd components && (
    for f in lib*.so; do
        mv ../$f.$PROG.$TOOL.threads .
    done
)
