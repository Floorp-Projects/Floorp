#!/bin/sh

dirs="core bugs"

case $1 in
  baseline|verify|clean)
    ;;
  *)
    echo "Usage: $0 baseline|verify|clean"
    exit 1
    ;;
esac

for i in $dirs; do
  cd $i
  echo $cmd in $i
  ../runtests.sh $1
  cd ..
done
