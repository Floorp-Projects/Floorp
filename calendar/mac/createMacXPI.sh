#!/bin/bash
suffix="so"

if [ -f calendar_mac.xpi ]; then 
   rm -rf calendar_mac.xpi
fi

mkdir -p viewer/components
mkdir -p viewer/chrome
ln -sf ../../../../dist/bin/components/libxpical.$suffix viewer/components/libxpical.$suffix
ln -sf ../../../../dist/bin/components/calendar.xpt viewer/components/calendar.xpt
ln -sf ../../../resources/content viewer/chrome/
ln -sf ../../../resources/locale viewer/chrome/
ln -sf ../../../resources/skin viewer/chrome/
find install.js viewer/components viewer/chrome/content/ viewer/chrome/locale/ viewer/chrome/skin/ \( -name CVS \) -prune -o -print | xargs zip calendar_mac.xpi
