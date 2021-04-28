#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0

echo Checking public headers for missing cairo_public decorators

cd "$srcdir"
FILES=$all_cairo_headers
if test "x$FILES" = x; then
	FILES=`find . -name 'cairo*.h' ! -name '*-private.h' ! -name 'cairoint.h'`
fi

grep -B 1 '^cairo_.*[ 	]\+(' /dev/null $FILES |
awk '
/^--$/ { context=""; public=0; next; }
/:cairo_.*[ 	]+\(/ { if (!public) {print context; print; print "--";} next; }
/-cairo_public.*[ 	]/ {public=1;}
{ context=$0; }
' |
sed 's/[.]h-/.h:/' |
grep . >&2 && stat=1

exit $stat
