#!/bin/sh

LD_LIBRARY_PATH=.
export LD_LIBRARY_PATH

PROG=mozilla-bin
PLIBS=""

for l in *.so components/*.so; do
    PLIBS="$PLIBS -incobj $l"
done

$ECHO /bin/gprof -L. -Lcomponents -all $PLIBS $PROG $PROG.hiout
