#!/bin/sh

echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"
echo "Packaging Minimo from"
echo $MOZ_OBJDIR
echo "XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX"

while true; do
    echo -n "Delete dist/Embed (y/n) "
    read yn
    case $yn in
	y* | Y* ) rm -rf $MOZ_OBJDIR/dist/Embed ; break ;;
	[nN]* )   echo "skipping clean" ; break ;;
	q* ) exit ;;
	* ) echo "unknown response.  Asking again" ;;
    esac
done  

pushd ..
./package.sh
popd

rm -rf build
mkdir -p build/usr/lib/mozilla-minimo
mkdir -p build/usr/share/applications
mkdir -p build/usr/share/pixmaps
mkdir -p build/usr/bin

cp -a $MOZ_OBJDIR/dist/Embed/* build/usr/lib/mozilla-minimo/
cp -a startpage build/usr/lib/mozilla-minimo

cp minimo         build/usr/bin/
cp minimo.desktop build/usr/share/applications/
cp minimo.png     build/usr/share/pixmaps/

mkdir -p build/CONTROL
cp control.in build/CONTROL/control

ipkg-build -o root -g root build
