#!/bin/sh
#
# FTP script for downloading /share/builds/components stuff.
#
# syntax:
#      nsftp <srcdir-relative to /share/builds/components> <destdir>
#
# Example 
#
#      nsftp ldapsdk/19961108 c:\3.0\ns\components\ldapsdk
#

SERVER=ftp-rel
USER=ftpman
PASSWD=ftpman
TMPFILE=tmp.foo

SRC=$1
DEST=$2
if [ -z "$3" ]; then 
  FILENAME=*
else
  FILENAME=$3
fi

echo ${USER} contents of ${SRC} to ${DEST}

cd ${DEST}
ftp -n ${SERVER} << -=EOF=-
user ${USER} ${PASSWD}
binary
hash
prompt
cd ${SRC}
mget ${FILENAME}
quit
-=EOF=-

