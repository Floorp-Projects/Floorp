#!/bin/sh
# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is the Netscape Portable Runtime (NSPR).
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

# FTP script for downloading /share/builds/components stuff.
#
# syntax:
#      nsftp <srcdir-relative to /share/builds/components> <destdir>
#
# Example 
#
#      nsftp ldapsdk/19961108 c:\3.0\ns\components\ldapsdk
#      nsftp ldapsdk/19961108 c:\3.0\ns\components\ldapsdk
 sdk_binary.jar
#

#SERVER=ftp-rel
#SERVER=gaida
SERVER=$COMPONENT_FTP_SERVER
USER=ftpman
PASSWD=ftpman
TMPFILE=tmp.foo

SRC=$1
DEST=$2
if [ -z $3 ]; then 
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
get ${FILENAME}
quit
-=EOF=-

