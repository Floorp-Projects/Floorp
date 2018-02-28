#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
test -z "$libs" && libs=.libs
stat=0

if which nm 2>/dev/null >/dev/null; then
	:
else
	echo "check-defs.sh: 'nm' not found; skipping test"
	exit 77
fi

defs="harfbuzz.def"
if ! test -f "$defs"; then
	echo "check-defs.sh: '$defs' not found; skipping test"
	exit 77
fi

tested=false
for def in $defs; do
	lib=`echo "$def" | sed 's/[.]def$//;s@.*/@@'`
	for suffix in so dylib; do
		so=$libs/lib${lib}.$suffix
		if ! test -f "$so"; then continue; fi

		# On mac, C symbols are prefixed with _
		if test $suffix = dylib; then prefix="_"; fi

		EXPORTED_SYMBOLS="`nm "$so" | grep ' [BCDGINRSTVW] .' | grep -v " $prefix"'\(_fini\>\|_init\>\|_fdata\>\|_ftext\>\|_fbss\>\|__bss_start\>\|__bss_start__\>\|__bss_end__\>\|_edata\>\|_end\>\|_bss_end__\>\|__end__\>\|__gcov_flush\>\|llvm_\)' | cut -d' ' -f3`"

		if test -f "$so"; then

			echo "Checking that $so has the same symbol list as $def"
			{
				echo EXPORTS
				echo "$EXPORTED_SYMBOLS" | sed -e "s/^${prefix}hb/hb/g"
				# cheat: copy the last line from the def file!
				tail -n1 "$def"
			} | diff "$def" - >&2 || stat=1

			tested=true
		fi
	done
done
if ! $tested; then
	echo "check-defs.sh: libharfbuzz shared library not found; skipping test"
	exit 77
fi

exit $stat
