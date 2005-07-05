#!/bin/bash

if [ -z "$MOZ_OBJDIR" ]
then
echo MOZ_OBJDIR is not set!!
exit 1
else
echo MOZ_OBJDIR is $MOZ_OBJDIR
fi

pushd $MOZ_OBJDIR/dist
rm -rf wince

echo Copying over files from MOZ_OBJDIR

mkdir wince
cp -a bin/js3250.dll                                     wince
cp -a bin/minimo.exe                                     wince
cp -a bin/nspr4.dll                                      wince
cp -a bin/plc4.dll                                       wince
cp -a bin/plds4.dll                                      wince
cp -a bin/xpcom.dll                                      wince
cp -a bin/xpcom_compat.dll                               wince
cp -a bin/xpcom_core.dll                                 wince

cp -a bin/nss3.dll                                       wince
cp -a bin/nssckbi.dll                                    wince
cp -a bin/smime3.dll                                     wince
cp -a bin/softokn3.dll                                   wince
cp -a bin/ssl3.dll                                       wince


mkdir -p wince/chrome
cp -a bin/chrome/classic.jar                             wince/chrome
cp -a bin/chrome/comm.jar                                wince/chrome
cp -a bin/chrome/en-US.jar                               wince/chrome
cp -a bin/chrome/en-win.jar                              wince/chrome
cp -a bin/chrome/installed-chrome.txt                    wince/chrome
cp -a bin/chrome/minimo.jar                              wince/chrome
cp -a bin/chrome/modern.jar                              wince/chrome
cp -a bin/chrome/toolkit.jar                             wince/chrome
cp -a bin/chrome/US.jar                                  wince/chrome

cp -a bin/chrome/pippki.jar                              wince/chrome
cp -a bin/chrome/pipnss.jar                              wince/chrome

mkdir -p wince/components
cp -a bin/components/nsDictionary.js                     wince/components
cp -a bin/components/nsHelperAppDlg.js                   wince/components
cp -a bin/components/nsInterfaceInfoToIDL.js             wince/components
cp -a bin/components/nsProgressDialog.js                 wince/components
cp -a bin/components/nsProxyAutoConfig.js                wince/components
cp -a bin/components/nsXmlRpcClient.js                   wince/components
cp -a bin/components/xulappinfo.js                       wince/components

cp -a bin/components/snav.dll                            wince/components

mkdir -p wince/greprefs
cp -a bin/greprefs/*                                     wince/greprefs

mkdir -p wince/res
cp -a bin/res/*                                          wince/res
rm -rf wince/res/samples
rm -rf wince/res/throbber

mkdir -p wince/plugins

echo Linking XPT files.

host/bin/host_xpt_link wince/components/all.xpt                        bin/components/*.xpt

echo Copying over customized files

popd

cp -a ../all.js                                         $MOZ_OBJDIR/dist/wince/greprefs

echo Applying SSR

cat ../smallScreen.css >>                               $MOZ_OBJDIR/dist/wince/res/ua.css

echo Copying ARM shunt lib.  Adjust if you are not building ARM

cp -a ../../../build/wince/shunt/build/ARMV4Rel/shunt.dll $MOZ_OBJDIR/dist/wince

echo Zipping

zip $MOZ_OBJDIR/dist/wince.zip $MOZ_OBJDIR/dist/wince

echo Done.
