#!/bin/sh

#-------------------------------------------------------
# Hack to allow chmodding in
# a zippy.
#
# usage: chmod <fileToChmod>
#
#-------------------------------------------------------

echo chmodding $1
chmod 777 $1

