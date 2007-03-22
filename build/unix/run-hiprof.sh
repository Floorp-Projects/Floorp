#!/bin/sh

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
