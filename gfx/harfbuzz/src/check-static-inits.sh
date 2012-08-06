#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
stat=0


if which objdump 2>/dev/null >/dev/null; then
	:
else
	echo "check-static-inits.sh: 'objdump' not found; skipping test"
	exit 77
fi

echo "Checking that no object file has static initializers"
for obj in .libs/*.o; do
	if objdump -t "$obj" | grep '[.]ctors'; then
		echo "Ouch, $obj has static initializers"
		stat=1
	fi
done

echo "Checking that no object file has lazy static C++ constructors/destructors"
for obj in .libs/*.o; do
	if objdump -t "$obj" | grep '__c'; then
		echo "Ouch, $obj has lazy static C++ constructors/destructors"
		stat=1
	fi
done

exit $stat
