#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

test "x$HBHEADERS" = x && HBHEADERS=`find . -maxdepth 1 -name 'hb*.h'`
test "x$HBSOURCES" = x && HBSOURCES=`find . -maxdepth 1 -name 'hb-*.cc' -or -name 'hb-*.hh'`


cd "$srcdir"


echo 'Checking that public header files #include "hb-common.h" or "hb.h" first (or none)'

for x in $HBHEADERS; do
	grep '#.*\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"hb-common[.]h"' |
grep -v '"hb[.]h"' |
grep -v 'hb-common[.]h:' |
grep -v 'hb[.]h:' |
grep . >&2 && stat=1


echo 'Checking that source files #include "hb-*private.hh" first (or none)'

for x in $HBSOURCES; do
	grep '#.*\<include\>' "$x" /dev/null | head -n 1
done |
grep -v '"hb-.*private[.]hh"' |
grep -v 'hb-private[.]hh:' |
grep . >&2 && stat=1


echo 'Checking that there is no #include <hb.*.h>'
grep '#.*\<include\>.*<.*hb' $HBHEADERS $HBSOURCES >&2 && stat=1


exit $stat
