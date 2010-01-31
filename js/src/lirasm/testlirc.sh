#!/bin/bash

set -eu

LIRASM=$1

TESTS_DIR=`dirname "$0"`/tests

for infile in "$TESTS_DIR"/*.in
do
    outfile=`echo $infile | sed 's/\.in/\.out/'`
    if [ ! -e "$outfile" ]
    then
        echo "$0: error: no out file $outfile"
        exit 1
    fi

    # Treat "random.in" and "random-opt.in" specially.
    if [ `basename $infile` = "random.in" ]
    then
        infile="--random 1000000"
    elif [ `basename $infile` = "random-opt.in" ]
    then
        infile="--random 1000000 --optimize"
    fi

    if $LIRASM --execute $infile | tr -d '\r' > testoutput.txt && cmp -s testoutput.txt $outfile
    then
        echo "TEST-PASS | lirasm | lirasm --execute $infile"
    else
        echo "TEST-UNEXPECTED-FAIL | lirasm | lirasm --execute $infile"
        echo "expected output"
        cat $outfile
        echo "actual output"
        cat testoutput.txt
    fi
done

rm testoutput.txt
