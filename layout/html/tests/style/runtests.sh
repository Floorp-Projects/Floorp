#!/bin/sh

# create test file to use first; since we don't know where the tree
# is, and we need full pathnames in the file, we create it on the fly.

viewer=$MOZILLA_FIVE_HOME/viewer
echo viewer: $viewer
testsfile=/tmp/$$-tests.txt

if test "$1"x = "baselinex"; then
  rm -r -f baseline
  mkdir baseline
  for i in 1; do
    if test -f file_list"$i".txt; then 
      sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list"$i".txt > $testsfile
      $viewer -d 1 -o baseline/ -f $testsfile
    fi
  done
  if test -f file_list_printing.txt; then 
    sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list_printing.txt > $testsfile
    $viewer -Prt 1 -d 5 -o baseline/ -f $testsfile
  fi
elif test "$1"x = "verifyx"; then
  rm -r -f verify
  mkdir verify
  for i in 1 2 3 4 5 6; do
    if test -f file_list"$i".txt; then 
      sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list"$i".txt > $testsfile
      $viewer -d 1 -o verify/ -rd baseline/ -f $testsfile
    fi
  done
  if test -f file_list_printing.txt; then 
    sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list"$i".txt > $testsfile
    $viewer -Prt 1 -d 5 -o verify/ -rd baseline/ -f $testsfile
  fi
elif test "$1"x = "cleanx"; then
  rm -r -f verify baseline  
else
  echo "Usage: $0 baseline|verify|clean"
fi
rm -f $testsfile
