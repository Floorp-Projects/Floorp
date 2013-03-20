#!/bin/bash

MODE=compare
if [[ "$1" == "--seq" ]]; then
    MODE=seq
    shift
elif [[ "$1" == "--par" ]]; then
    MODE=par
    shift
fi

if [[ -z "$1" ]] || [[ "$1" == "--help" ]]; then
    echo "Usage: run.sh [--seq | --par] path-to-shell paths-to-test"
    echo ""
    echo "Runs the given benchmark(s) using the given shell and "
    echo "prints the results.  If -seq or -par is supplied, "
    echo "only runs sequentially or in parallel.  Otherwise, runs both"
    echo "and compares the performance."
fi

D="$(dirname $0)"
S="$1"
shift
for T in "$@"; do
    echo "$S" -e "'"'var libdir="'$D'/"; var MODE="'$MODE'";'"'" "$T"
    "$S" -e 'var libdir="'$D'/"; var MODE="'$MODE'";' "$T"
done
