#!/usr/bin/sh
#
# use to this script to leverage transation from localized jar, xx-XX_jar, into the US embed_jar/
toLoc=$1
if [ "$toLoc" = "" ]
then
    echo "Usage: $0 toLocale"
    exit 1
fi
# leverage
files="communicator/pref/pref-scripts.dtd \
    communicator/pref/pref-smart_browsing.dtd \
    communicator/pref/preftree.dtd \
    communicator/wallet/wallet.properties \
    global/ldapAutoCompErrs.properties \
    global/printing.properties \
    global/regionNames.properties \
    global/tabbrowser.dtd \
    navigator/navigator.dtd \
    navigator/navigator.properties \
    navigator/pageInfo.dtd \
    navigator/turboDialog.dtd \
    pipnss/pipnss.properties"
#
for file in $files
do
    #
    subsKeyVal.pl embed_jar/locale/en-US/${file} ${toLoc}_jar/locale/${toLoc}/${file} ${toLoc}
    #
    # copying
    cp embed_jar/locale/en-US/${file}.${toLoc} embed_jar-${toLoc}/locale/${toLoc}/${file}
done
