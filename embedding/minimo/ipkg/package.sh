#!/bin/sh

rm -rf build
mkdir -p build/usr/lib/mozilla-minimo
mkdir -p build/usr/share/applications
mkdir -p build/usr/share/pixmaps
mkdir -p build/usr/bin

cp -a $MOZ_OBJDIR/dist/Embed/* build/usr/lib/mozilla-minimo/

cp minimo         build/usr/bin/
cp minimo.desktop build/usr/share/applications/
cp minimo.png build/usr/share/pixmaps/

mkdir -p build/CONTROL
cp control.in build/CONTROL/control


