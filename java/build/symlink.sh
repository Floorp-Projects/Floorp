#!/bin/sh

#-------------------------------------------------------
# Hack to allow symlinking (required for Sun's JRE) in
# a zippy.
#
# usage: symlink.sh <srcfile> <newlink>
#
#-------------------------------------------------------

echo linking $1 to $2
ln -s $1 $2
