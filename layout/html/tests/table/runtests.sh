#!/bin/sh

# create test file to use first; since we don't know where the tree
# is, and we need full pathnames in the file, we create it on the fly.

viewer=$MOZILLA_FIVE_HOME/viewer
echo viewer: $viewer
testsfile=/tmp/$$-tests.txt

sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list.txt > $testsfile
sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list2.txt >> $testsfile
sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list3.txt >> $testsfile
sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list4.txt >> $testsfile
sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list5.txt >> $testsfile
sed -e "s@file:///s|@file:$MOZ_SRC@" < file_list6.txt >> $testsfile

if test "$1"x = "baselinex"; then
  rm -r -f baseline
  mkdir baseline
  $viewer -d 1 -o baseline/ -f $testsfile
elif test "$1"x = "verifyx"; then
  rm -r -f verify
  mkdir verify
  $viewer -d 1 -o verify/ -rd baseline/ -f $testsfile
elif test "$1"x = "cleanx"; then
  rm -r -f verify baseline  
else
  echo "Usage: $0 baseline|verify|clean"
fi
rm -f $testsfile
