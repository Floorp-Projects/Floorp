#!/usr/bin/sh
#
# needed files / dirs
#    embed_jar/{content,skin,locale}
#    ${langcode}_jar/
#    jar.mn-ab-CD
#    installed-chrome.txt-en-US
#
# needed scripts: 
#    make-jars.pl, leverage.sh, subsKeyVal.pl  
#    and strsubs.pl if $2, $3 present
#   
langcode=$1
if [ "$langcode" = "" ]
then
    echo "Usage: $0 langcode [localeVer1 localeVer2]"
    exit 1
fi
# 1. copy jar.mn-ab-CD
# 2. replace all ab-CD to ${langcode}
#
strsubs.pl "ab-CD" ${langcode} jar.mn-ab-CD
mv jar.mn-ab-CD jar.mn-${langcode}
mv jar.mn-ab-CD.org jar.mn-ab-CD
#
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
./leverage.sh ${langcode}
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