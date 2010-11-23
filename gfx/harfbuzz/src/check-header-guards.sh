#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

cd "$srcdir"

for x in hb-*.h hb-*.hh ; do
	tag=`echo "$x" | tr 'a-z.-' 'A-Z_'`
	lines=`grep "\<$tag\>" "$x" | wc -l`
	if test "x$lines" != x3; then
		echo "Ouch, header file $x does not have correct preprocessor guards"
		stat=1
	fi
done

exit $stat
