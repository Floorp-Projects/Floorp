#!/bin/bash
rm -f calendar_windows.xpi
rm -f calendar_linux.xpi
./createBuildId.pl resources/content/about.html

#Linux
echo "Building Linux xpi..."
cd linux/components
chmod 644 *.*
cd ..
find components/ resources/ icons/ install.js \( -name CVS -o -name Makefile -o -name makefile.win -o -name Makefile.in -o -name test -o -name .cvsignore \) -prune -o -print | xargs zip calendar_linux.xpi
mv calendar_linux.xpi ../
echo "Done."

#Windows
echo "Building Windows xpi..."
cd ../windows
find components/ resources/ icons/ install.js \( -name CVS -o -name Makefile -o -name makefile.win -o -name Makefile.in -o -name test -o -name .cvsignore \) -prune -o -print | xargs zip calendar_windows.xpi
mv calendar_windows.xpi ../
echo "Done."
cd ..

#Mac
#cd ../mac
#cd components
#chmod 644 *.*
#cd ..
#find components/ resources/ install.js \( -name CVS -o -name Makefile -o -name makefile.win -o -name Makefile.in -o -name .cvsignore \) -prune -o -print | xargs zip calendar_mac.xpi
#mv calendar_mac.xpi ../
