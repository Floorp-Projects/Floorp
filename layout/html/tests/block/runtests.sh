#!/bin/sh

# create test file to use first; since we don't know where the tree
# is, and we need full pathnames in the file, we create it on the fly.

testsfile=/tmp/$$-tests.txt

sed -e "s@file:///s\(:\||\)@file://$MOZ_TEST_BASE@" < file_list.txt > $testsfile

if test "$1"x = "baselinex"; then
  rm -r -f baseline
  mkdir baseline
  echo
  echo $MOZ_TEST_VIEWER -o baseline/ -f $testsfile
  $MOZ_TEST_VIEWER -o baseline/ -f $testsfile
elif test "$1"x = "verifyx"; then
  rm -r -f verify
  mkdir verify
  echo
  echo $MOZ_TEST_VIEWER -o baseline/ -f $testsfile
  $MOZ_TEST_VIEWER -o verify/ -rd baseline/ -f $testsfile
elif test "$1"x = "cleanx"; then
  rm -r -f verify baseline  
else
  echo "Usage: $0 baseline|verify|clean"
fi
rm -f $testsfile
