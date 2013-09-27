#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
test -z "$MAKE" && MAKE=make
stat=0

if which nm 2>/dev/null >/dev/null; then
	:
else
	echo "check-symbols.sh: 'nm' not found; skipping test"
	exit 77
fi

defs="harfbuzz.def"
$MAKE $defs > /dev/null
tested=false
for def in $defs; do
	lib=`echo "$def" | sed 's/[.]def$//;s@.*/@@'`
	so=.libs/lib${lib}.so

	EXPORTED_SYMBOLS="`nm "$so" | grep ' [BCDGINRSTVW] ' | grep -v ' _fini\>\| _init\>\| _fdata\>\| _ftext\>\| __bss_start\>\| __bss_start__\>\| __bss_end__\>\| _edata\>\| _end\>\| _bss_end__\>\| __end__\>' | cut -d' ' -f3`"

	if test -f "$so"; then

		echo "Checking that $so has the same symbol list as $def"
		{
			echo EXPORTS
			echo "$EXPORTED_SYMBOLS"
			# cheat: copy the last line from the def file!
			tail -n1 "$def"
		} | diff "$def" - >&2 || stat=1

		echo "Checking that we are not exposing internal symbols"
		if echo "$EXPORTED_SYMBOLS" | grep -v 'hb_'; then
			echo "Ouch, internal symbols exposed"
			stat=1
		fi

		tested=true
	fi
done
if ! $tested; then
	echo "check-exported-symbols.sh: libharfbuzz shared library not found; skipping test"
	exit 77
fi

exit $stat
