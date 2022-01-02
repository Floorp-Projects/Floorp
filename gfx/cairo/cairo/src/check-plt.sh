#!/bin/sh

LC_ALL=C
export LC_ALL

if which readelf 2>/dev/null >/dev/null; then
	:
else
	echo "'readelf' not found; skipping test"
	exit 0
fi

test -z "$srcdir" && srcdir=.
test -z "$MAKE" && MAKE=make
stat=0

$MAKE check-has-hidden-symbols.i > /dev/null || exit 1
if tail -1 check-has-hidden-symbols.i | grep CAIRO_HAS_HIDDEN_SYMBOLS >/dev/null; then
	echo "Compiler doesn't support symbol visibility; skipping test"
	exit 0
fi

for so in .libs/lib*.so; do
	echo Checking "$so" for local PLT entries
	readelf -W -r "$so" | grep 'JU\?MP_SLO' | grep 'cairo' >&2 && stat=1
done

exit $stat
