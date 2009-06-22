#!/bin/bash

set -eu

TESTS_DIR=`dirname "$0"`/tests

for infile in "$TESTS_DIR"/*.in
do
    outfile=`echo $infile | sed 's/\.in/\.out/'`
    if [ ! -e "$outfile" ]
    then
        echo "$0: error: no out file $outfile"
        exit 1
    fi

    if ./lirasm --execute $infile > testoutput.txt && cmp -s testoutput.txt $outfile
    then
        echo "$0: output correct for $infile"
    else
        echo "$0: incorrect output for $infile"
        echo "$0: === actual output ==="
        cat testoutput.txt
        echo "$0: === expected output ==="
	cat $outfile
    fi
done
rm testoutput.txt
