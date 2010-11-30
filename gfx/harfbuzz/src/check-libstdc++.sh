#!/bin/sh

LC_ALL=C
export LC_ALL

if which ldd 2>/dev/null >/dev/null; then
	:
else
	echo "check-libstdc++.sh: 'ldd' not found; skipping test"
	exit 0
fi

test -z "$srcdir" && srcdir=.
stat=0

so=.libs/libharfbuzz.so
if test -f "$so"; then
	echo "Checking that we are not linking to libstdc++"
	if ldd $so | grep 'libstdc[+][+]'; then
		echo "Ouch, linked to libstdc++"
		stat=1
	fi
else
	echo "check-libstdc++.sh: libharfbuzz.so not found; skipping test"
fi

exit $stat
