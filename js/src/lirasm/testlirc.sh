#!/bin/bash

set -eu

exitcode=0

LIRASM=$1

TESTS_DIR=`dirname "$0"`/tests

function runtest {
    local infile=$1
    local options=${2-}

    # Catch a request for the random tests.
    if [[ $infile == --random* ]]
    then
        local outfile=$TESTS_DIR/random.out
    else
        local outfile=`echo $infile | sed 's/\.in/\.out/'`
    fi

    if [[ ! -e "$outfile" ]]
    then
        echo "$0: error: no out file $outfile"
        exit 1
    fi

    # sed used to strip extra leading zeros from exponential values 'e+00' (see bug 602786)
    if $LIRASM $options --execute $infile | tr -d '\r' | sed -e 's/e+00*/e+0/g' > testoutput.txt && cmp -s testoutput.txt $outfile
    then
        echo "TEST-PASS | lirasm | lirasm $options --execute $infile"
    else
        echo "TEST-UNEXPECTED-FAIL | lirasm | lirasm $options --execute $infile"
        echo "expected output"
        cat $outfile
        echo "actual output"
        cat testoutput.txt
        exitcode=1
    fi
}

# Tests common to all supported back-ends.
for infile in "$TESTS_DIR"/*.in
do
    runtest $infile
done

# ---- Platform-specific tests and configurations. ----

# Tests for hardware floating-point.
# These tests use LIR instructions which are normally removed by the soft-float
# filter, so soft-float targets do not need to support them.
#
# There is no conditional check for hardfloat support as every platform appears
# to support it. If the default for a particular platform does not support
# hardfloat, exclude the hardfloat tests (based on something like "uname -m").
for infile in "$TESTS_DIR"/hardfloat/*.in
do
    runtest $infile
done

# 64-bit platforms
if [[ $($LIRASM --word-size) == 64 ]]
then
    for infile in "$TESTS_DIR"/64-bit/*.in
    do
        runtest $infile
    done
fi

# 32-bit platforms
if [[ $($LIRASM --word-size) == 32 ]]
then
    for infile in "$TESTS_DIR"/32-bit/*.in
    do
        runtest $infile
    done
fi

# little endian
if [[ $($LIRASM --endianness 2>/dev/null) != "big-endian" ]]
then
    for infile in "$TESTS_DIR"/littleendian/*.in
    do
        runtest $infile
    done
fi

# big endian
if [[ $($LIRASM --endianness 2>/dev/null) == "big-endian" ]]
then
    for infile in "$TESTS_DIR"/bigendian/*.in
    do
        runtest $infile
    done
fi

# ARM
if [[ $(uname -m) == arm* ]]
then
    for infile in "$TESTS_DIR"/*.in
    do
        # Run standard tests, but set code generation for older architectures.
        # It may also be beneficial to test ARMv6 and ARMv7 with --novfp, but such
        # a platform seems so unlikely that it probably isn't worthwhile. It's also
        # unlikely that it's worth testing ARMv5 with VFP.
        runtest $infile "--arch 6"
        runtest $infile "--arch 5 --novfp"
    done

    for infile in "$TESTS_DIR"/hardfloat/*.in
    do
        # Run tests that require hardware floating-point.
        runtest $infile "--arch 6"
    done

    # Run specific soft-float tests, but only for ARMv5 without VFP.
    # NOTE: It looks like MIPS ought to be able to run these tests, but I can't
    # test this and _not_ running them seems like the safest option.
    for infile in "$TESTS_DIR"/softfloat/*.in
    do
        runtest $infile "--arch 5 --novfp"
    done

    # Run reduced random tests for these targets. (The default ARMv7 target
    # still runs the long tests.)
    runtest "--random 10000 --arch 6"
    runtest "--random 10000 --arch 5 --novfp"
    runtest "--random 10000 --optimize --arch 6"
    runtest "--random 10000 --optimize --arch 5 --novfp"
fi

# ---- Randomized tests, they are run last because they are slow ----

runtest "--random 1000000"
runtest "--random 1000000 --optimize"

rm testoutput.txt

exit $exitcode
