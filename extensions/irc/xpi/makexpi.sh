#! /bin/bash

# Set up paths for finding files.
if [ -z "$FEDIR" ]; then FEDIR=$PWD/..; fi
if [ -z "$CONFIGDIR" ]; then CONFIGDIR=$FEDIR/../../config; fi
if [ -z "$XPIFILES" ]; then XPIFILES=$PWD/resources; fi
if [ -z "$XPIROOT" ]; then XPIROOT=$PWD/xpi-tree; fi
if [ -z "$JARROOT" ]; then JARROOT=$PWD/jar-tree; fi


if [ "$1" == "clean" ]; then
  echo -n "Cleaning up files"
  echo -n .
  rm -rf "$XPIROOT"
  echo -n .
  rm -rf "$JARROOT"
  echo   ". done."
  
  exit
fi


# Check setup.
if ! [ -d "$FEDIR" ]; then
  echo "ERROR: Base ChatZilla directory (FEDIR) not found."
  exit 1
fi
if ! [ -d "$CONFIGDIR" ]; then
  echo "ERROR: mozilla/config directory (CONFIGDIR) not found."
  exit 1
fi


# Extract version number.
VERSION=`grep "const __cz_version" "$FEDIR/xul/content/static.js" | sed "s|.*\"\([^\"]\+\)\".*|\1|"`

echo Begining build of ChatZilla $VERSION...


# Check for existing.
if [ -r "chatzilla-$VERSION.xpi" ]; then
  echo "  WARNING: output XPI will be overwritten."
fi


# Check for required directory layouts.
echo -n "  Checking XPI structure"
echo -n .
if ! [ -d xpi-tree ]; then mkdir xpi-tree; fi
echo -n .
if ! [ -d xpi-tree/chrome ]; then mkdir xpi-tree/chrome; fi
echo -n .
if ! [ -d xpi-tree/components ]; then mkdir xpi-tree/components; fi
echo -n .
if ! [ -d xpi-tree/defaults ]; then mkdir xpi-tree/defaults; fi
echo   ".            done"

echo -n "  Checking JAR structure"
echo -n .
if ! [ -d jar-tree ]; then mkdir jar-tree; fi
echo   ".               done"


# Make Firefox updates.
echo -n "  Updating Firefox Extension files"
echo -n .
sed "s|@REVISION@|$VERSION|g" < "$XPIFILES/install.rdf" > "$XPIROOT/install.rdf"
echo -n .
cp "$XPIFILES/chatzilla-window.ico" "$XPIROOT/defaults/chatzilla-window.ico"
echo -n .
cp "$XPIFILES/chatzilla-window.xpm" "$XPIROOT/defaults/chatzilla-window.xpm"
echo -n .
cp "$XPIFILES/chatzilla-window16.xpm" "$XPIROOT/defaults/chatzilla-window16.xpm"
echo   ".  done"


# Make Mozilla Suite updates.
echo -n "  Updating Mozilla Extension files"
echo -n .
sed "s|@REVISION@|$VERSION|g" < "$XPIFILES/install.js" > "$XPIROOT/install.js"
echo -n .
mv "$FEDIR/xul/content/contents.rdf" "$FEDIR/xul/content/contents.rdf.in"
sed "s|@MOZILLA_VERSION@|cz-$VERSION|g;s|\(chrome:displayName=\)\"[^\"]\+\"|\1\"ChatZilla $VERSION\"|g" < "$FEDIR/xul/content/contents.rdf.in" > "$FEDIR/xul/content/contents.rdf"
rm "$FEDIR/xul/content/contents.rdf.in"
echo -n .
mv "$FEDIR/xul/locale/en-US/contents.rdf" "$FEDIR/xul/locale/en-US/contents.rdf.in"
sed "s|@MOZILLA_VERSION@|cz-$VERSION|g" < "$FEDIR/xul/locale/en-US/contents.rdf.in" > "$FEDIR/xul/locale/en-US/contents.rdf"
rm "$FEDIR/xul/locale/en-US/contents.rdf.in"
echo   ".   done"


# Create JAR.
echo -n "  Constructing JAR package"
echo -n .
pushd "$CONFIGDIR" >/dev/null
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s "$FEDIR" -d "$JARROOT" < "$FEDIR/jar.mn" 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s "$FEDIR/sm" -d "$JARROOT" < "$FEDIR/sm/jar.mn" 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s "$FEDIR/ff" -d "$JARROOT" < "$FEDIR/ff/jar.mn" 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo -n .
popd >/dev/null
echo   ".         done"


# Make XPI.
echo -n "  Constructing XPI package"
echo -n .
OUT=`cp -v "$JARROOT/chatzilla.jar" "$XPIROOT/chrome/"`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo -n .
OUT=`cp -v "$FEDIR/js/lib/chatzilla-service.js" "$XPIROOT/components/"`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo -n .
chmod 664 "$XPIROOT/chrome/chatzilla.jar"
echo -n .
chmod 664 "$XPIROOT/components/chatzilla-service.js"
echo -n .
pushd "$XPIROOT" >/dev/null; OUT=`zip -vr ../chatzilla-$VERSION.xpi * 2>&1`; popd >/dev/null
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; exit 1; fi
echo   ".         done"


echo "Build of ChatZilla $VERSION...         ALL DONE"
