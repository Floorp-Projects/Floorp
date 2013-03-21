#!/bin/sh

set -e

JOBS="$1"

N=$($SIXGILL/bin/xdbkeys src_body.xdb | wc -l)
EACH=$(( $N / $JOBS ))
START=1
for j in $(seq $JOBS); do
    END=$(( $START + $EACH ))
    env PATH=$PATH:$SIXGILL/bin XDB=$SIXGILL/bin/xdb.so $JS $ANALYZE gcFunctions.txt gcTypes.txt $START $END tmp.$j > rootingHazards.$j &
    START=$END
done

wait

for j in $(seq $JOBS); do
    cat rootingHazards.$j
done
