#!/bin/bash
suffix="dylib"

if [ -f calendar_mac.xpi ]; then 
   rm -rf calendar_mac.xpi
fi

mkdir -p viewer/components
mkdir -p viewer/chrome
ln -sf ../../../../dist/bin/components/libxpical.$suffix viewer/components/libxpical.$suffix
ln -sf ../../../../dist/bin/components/calendar.xpt viewer/components/calendar.xpt
ln -sf ../../../../dist/bin/chrome/calendar.jar viewer/chrome/calendar.jar
#ln -sf ../../../../other-licenses/libical/src/libical/.libs/libical.$suffix viewer/libical.$suffix
#ln -sf ../../../../other-licenses/libicalss/src/libical/.libs/libicalss.$suffix viewer/libicalss.$suffix
#ln -sf ../../../../other-licenses/libical/src/libical/.libs/libicalvcal.$suffix viewer/libicalvcal.$suffix
#ln -sf ../../../resources/content viewer/chrome/
#ln -sf ../../../resources/locale viewer/chrome/
#ln -sf ../../../resources/skin viewer/chrome/
#find install.js viewer/components viewer/chrome/content/ viewer/chrome/locale/ viewer/chrome/skin/ \( -name CVS \) -prune -o -print | xargs zip calendar_mac.xpi
find install.js viewer/components viewer/chrome \( -name CVS \) -prune -o -print | xargs zip calendar_mac.xpi
