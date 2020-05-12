#!/bin/sh

LC_ALL=C
export LC_ALL

test -z "$srcdir" && srcdir=.
test -z "$builddir" && builddir=.
stat=0

if which objdump 2>/dev/null >/dev/null; then
	:
else
	echo "check-static-inits.sh: 'objdump' not found; skipping test"
	exit 77
fi

OBJS=$(find $builddir/ -name '*.o')
if test "x`echo $OBJS`" = "x$OBJS" 2>/dev/null >/dev/null; then
	echo "check-static-inits.sh: object files not found; skipping test"
	exit 77
fi

tested=false

echo "Checking that no object file has static initializers"
for obj in $OBJS; do
	if objdump -t "$obj" | grep '[.][cd]tors' | grep -v '\<00*\>'; then
		echo "Ouch, $obj has static initializers/finalizers"
		stat=1
	fi
	tested=true
done

echo "Checking that no object file has lazy static C++ constructors/destructors or other such stuff"
for obj in $OBJS; do
	if objdump -t "$obj" | grep -q '__cxa_' && ! objdump -t "$obj" | grep -q __ubsan_handle; then
		objdump -t "$obj" | grep '__cxa_'
		echo "Ouch, $obj has lazy static C++ constructors/destructors or other such stuff"
		stat=1
	fi
	tested=true
done

if ! $tested; then
	echo "check-static-inits.sh: no objects found; skipping test"
	exit 77
fi

exit $stat
