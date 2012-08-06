#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0


if which nm 2>/dev/null >/dev/null; then
	:
else
	echo "check-internal-symbols.sh: 'nm' not found; skipping test"
	exit 77
fi

if which c++filt 2>/dev/null >/dev/null; then
	cplusplusfilt=c++filt
else
	cplusplusfilt=cat
fi

tested=false
for suffix in so; do
	so=.libs/libharfbuzz.$suffix
	if test -f "$so"; then
		echo "Checking that we are not exposing internal symbols"
		if nm $so | grep ' [TW] ' | $cplusplusfilt | grep -v ' T _fini\>\| T _init\>\| T hb_'; then
			echo "Ouch, internal symbols exposed"
			stat=1
		fi
		tested=true
	fi
done
if ! $tested; then
	echo "check-internal-symbols.sh: libharfbuzz shared library not found; skipping test"
	exit 77
fi

exit $stat
