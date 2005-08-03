#!/bin/bash

if [ $# -ne 2 ]
then
  echo "Usage: `basename $0` OBJDIR SRCDIR"
  exit $E_BADARGS 
fi

OBJDIR=$1
SRCDIR=$2

echo ---------------------------------------------------
echo OBJDIR = $OBJDIR
echo SRCDIR = $SRCDIR
echo ---------------------------------------------------

pushd $OBJDIR/dist
rm -rf wince
rm -f wince.zip

echo Copying over files from OBJDIR

mkdir wince
cp -a bin/js3250.dll                                     wince
cp -a bin/minimo.exe                                     wince
cp -a bin/nspr4.dll                                      wince
cp -a bin/plc4.dll                                       wince
cp -a bin/plds4.dll                                      wince
cp -a bin/xpcom.dll                                      wince
cp -a bin/xpcom_core.dll                                 wince

cp -a bin/nss3.dll                                       wince
cp -a bin/nssckbi.dll                                    wince
cp -a bin/smime3.dll                                     wince
cp -a bin/softokn3.dll                                   wince
cp -a bin/ssl3.dll                                       wince


mkdir -p wince/chrome

cp -a bin/chrome/classic.jar                             wince/chrome
cp -a bin/chrome/classic.manifest                        wince/chrome

cp -a bin/chrome/en-US.jar                               wince/chrome
cp -a bin/chrome/en-US.manifest                          wince/chrome

cp -a bin/chrome/minimo.jar                              wince/chrome

cp -a bin/chrome/toolkit.jar                             wince/chrome
cp -a bin/chrome/toolkit.manifest                        wince/chrome

cp -a bin/chrome/pippki.jar                              wince/chrome
cp -a bin/chrome/pippki.manifest                         wince/chrome

cp -a bin/chrome/installed-chrome.txt                    wince/chrome


mkdir -p wince/components

cp -a bin/components/nsHelperAppDlg.js                   wince/components
cp -a bin/components/nsProgressDialog.js                 wince/components

cp -a bin/components/nsDictionary.js                     wince/components
cp -a bin/components/nsInterfaceInfoToIDL.js             wince/components
cp -a bin/components/nsXmlRpcClient.js                   wince/components

cp -a bin/extensions/spatial-navigation@extensions.mozilla.org/components/* wince/components

mkdir -p wince/greprefs
cp -a bin/greprefs/*                                     wince/greprefs

mkdir -p wince/res
cp -a bin/res/*                                          wince/res
rm -rf wince/res/samples
rm -rf wince/res/throbber

mkdir -p wince/plugins

echo Linking XPT files.

host/bin/host_xpt_link wince/components/all.xpt          bin/components/*.xpt

echo Chewing on chrome

cd wince/chrome

unzip classic.jar
rm -rf classic.jar
rm -rf skin/classic/communicator
rm -rf skin/classic/editor
rm -rf skin/classic/messenger
rm -rf skin/classic/navigator
zip -r classic.jar skin
rm -rf skin

unzip en-US.jar
rm -rf en-US.jar
rm -rf locale/en-US/communicator
rm -rf locale/en-US/navigator
zip -r en-US.jar locale
rm -rf locale

echo Copying over customized files

popd

pushd $SRCDIR

cp -a all.js                                            $OBJDIR/dist/wince/greprefs

echo Applying SSR

cat ../smallScreen.css >>                               $OBJDIR/dist/wince/res/ua.css

echo Copying ARM shunt lib.  Adjust if you are not building ARM

cp -a ../../../build/wince/shunt/build/ARMV4Rel/shunt.dll $OBJDIR/dist/wince


echo Rebasing

pushd $OBJDIR/dist/wince

rebase -b 0x0100000 -R . -v *.dll components/*.dll

echo Zipping

zip -r $OBJDIR/dist/wince.zip $OBJDIR/dist/wince

popd

echo Done.
