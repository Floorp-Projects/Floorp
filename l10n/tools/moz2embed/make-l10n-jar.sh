#!/usr/bin/sh
#
# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is Mozilla.
#
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 2002
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Tao Cheng <tao@netscape.com>
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
#
# Author: Tao Cheng <tao@netscape.com>
# Date  : July 22, 2002
#
# needed scripts: 
#    make-jars.pl, mozLock.pm, leverage.sh, 
#    and strsubs.pl if $2, $3 present
#
#    note: you can get the most recent copies of make-jars.pl and mozLock.pm
#          from mozilla/config
#
# needed file: jar.mn-ab-CD
#
langcode=$1
if [ "$langcode" = "" ]
then
    echo "Usage: $0 langcode [localeVer1 localeVer2]"
    exit 1
fi
# 1. copy jar.mn-ab-CD
# 2. replace all ab-CD to ${langcode}
# 3. make 1st ver of jar
echo ""
echo "-- calling make-jars.pl ..."
echo ""
make-jars.pl -s ${langcode}_jar < jar.mn-${langcode}
#
# expand it
#
echo ""
echo "-- calling unzip ..."
echo ""
unzip embed.jar -d embed_jar-${langcode}
#
# 4. rename embed.jar and installed-chrome.txt
#
mv embed.jar embed.jar-${langcode}
mv installed-chrome.txt installed-chrome.txt-${langcode}
#
# 5. run leverage.sh to leverage the xlations from ${langcode}_jar into embed_jar/locale/en-US/
#      and then replace the English files in embed_jar-${langcode}
#
echo ""
echo "-- calling leverage.sh ${langcode} ..."
echo ""
leverage.sh ${langcode}
#
# 6. update localeVersion if they are outdated
#
ver1=$2
ver2=$3
echo ""
echo "-- from ${ver1} to ${ver2} ?? ..."
echo ""
if [ "$ver1" != "$ver2" ]
then
  echo "-- updating localeVersion in embed_jar-${langcode} from ${ver1} to ${ver2}"
  find embed_jar-${langcode} -name "contents.rdf" -exec grep -l ${ver1} {} \; -exec strsubs.pl ${ver1} ${ver2} {} \;
  echo "-- removing *.org"
  find embed_jar-${langcode} -name "contents*.rdf.*" -exec rm {} \;
fi
#
# 
echo ""
echo "-- generating embed_jar-${langcode}/installed-chrome.txt ..."
echo ""
grep --invert "locale" installed-chrome.txt-en-US > embed_jar-${langcode}/installed-chrome.txt
cat installed-chrome.txt-${langcode} >> embed_jar-${langcode}/installed-chrome.txt
echo "locale,install,select,${langcode}" >> embed_jar-${langcode}/installed-chrome.txt
#
echo ""
echo "-- generating embed_jar-${langcode}/embed.jar ..."
echo ""
cp -r embed_jar/{content,skin} embed_jar-${langcode}
cd embed_jar-${langcode}
zip embed.jar -r content skin locale
zip embed_jar-${langcode}.zip embed.jar installed-chrome.txt
#
echo ""
echo "--DONE: ${langcode} embed.jar and installed-chrome.txt is in"
echo "        `pwd`/embed_jar-${langcode}.zip !! --"
exit 0