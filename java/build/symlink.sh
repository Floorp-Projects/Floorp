#!/bin/sh

#-------------------------------------------------------
# Hack to allow symlinking (required for Sun's JRE) in
# a zippy.
#
# usage: symlink.sh <srcfile> <newlink>
#
#-------------------------------------------------------

ln -s $1 $2
