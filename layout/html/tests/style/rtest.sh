#!/bin/sh

# Table regression tests.
#
# How to run:
# 1. Before you make changes, run:
#    rtest.sh baseline
# 2. Make your changes and rebuild
# 3. rtest.sh verify >outfilename
#
# This unfortunately doesn't work if you build in a separate objdir.
# If you do, you'll set the MOZILLA_FIVE_HOME variable below to point
# to your objdir.
# To fix this we'd need to make a Makefile.in for this directory
# and patch in the objdir from the build system.
#

dirs="base"

# Set a bunch of environment vars needed by runtests.sh or viewer:
MOZ_SRC=`pwd | sed "s_/mozilla/layout/html/tests/style__"`
export MOZ_SRC

# Insert objdir between mozilla and dist if necessary:
MOZILLA_FIVE_HOME=$MOZ_SRC/mozilla/dist/bin
export MOZILLA_FIVE_HOME

LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$MOZILLA_FIVE_HOME
export LD_LIBRARY_PATH

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
