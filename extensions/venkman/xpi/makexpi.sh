#! /bin/bash

# Set up paths for finding files.
FEDIR=$PWD/../resources
XPIFILES=$PWD
XPIROOT=$PWD/xpi-tree
JARROOT=$PWD/jar-tree


if [ "$1" == "clean" ]; then
  echo -n "Cleaning up files"
  echo -n .
  rm -rf $XPIROOT
  echo -n .
  rm -rf $JARROOT
  echo   ". done."
  
  exit
fi


# Extract version number.
VERSION=`grep "const __vnk_version" $FEDIR/content/venkman-static.js | sed "s|.*\"\([^\"]\+\)\".*|\1|"`

echo Begining build of JavaScript Debugger $VERSION...


# Check for existing.
if [ -r "venkman-$VERSION.xpi" ]; then
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
echo   ".               done"

echo -n "  Checking JAR structure"
echo -n .
if ! [ -d jar-tree ]; then mkdir jar-tree; fi
echo   ".                 done"


# Make Firefox updates.
echo -n "  Updating Firefox Extension files"
echo -n .
sed "s|@REVISION@|$VERSION|g" < $XPIFILES/install.rdf > $XPIROOT/install.rdf
echo   ".       done"


# Make Mozilla Suite updates.
echo -n "  Updating Mozilla Extension files"
echo -n .
mv $FEDIR/content/contents.rdf $FEDIR/content/contents.rdf.in
echo -n .
sed "s|@MOZILLA_VERSION@|vnk-$VERSION|g;s|\(chrome:displayName=\)\"[^\"]\+\"|\1\"JavaScript Debugger $VERSION\"|g" < $FEDIR/content/contents.rdf.in > $FEDIR/content/contents.rdf
echo -n .
rm $FEDIR/content/contents.rdf.in
echo -n .
mv $FEDIR/locale/en-US/contents.rdf $FEDIR/locale/en-US/contents.rdf.in
echo -n .
sed "s|@MOZILLA_VERSION@|vnk-$VERSION|g" < $FEDIR/locale/en-US/contents.rdf.in > $FEDIR/locale/en-US/contents.rdf
echo -n .
rm $FEDIR/locale/en-US/contents.rdf.in
echo   ".  done"


# Create JAR.
echo -n "  Constructing JAR package"
echo -n .
cd $FEDIR/../../../config
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s $FEDIR -d $JARROOT < $FEDIR/jar.mn 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s $FEDIR/sm -d $JARROOT < $FEDIR/sm/jar.mn 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo -n .
OUT=`perl make-jars.pl -v -z zip -p preprocessor.pl -s $FEDIR/ff -d $JARROOT < $FEDIR/ff/jar.mn 2>&1`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo -n .
cd $XPIFILES
echo -n .
sed "s|@REVISION@|$VERSION|g" < install.js > $XPIROOT/install.js
echo   ".          done"


# Make XPI.
echo -n "  Constructing XPI package"
echo -n .
OUT=`cp -v $JARROOT/venkman.jar $XPIROOT/chrome/`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo -n .
OUT=`cp -v $FEDIR/../js/venkman-service.js $XPIROOT/components/`
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo -n .
chmod 664 $XPIROOT/chrome/venkman.jar
echo -n .
chmod 664 $XPIROOT/components/venkman-service.js
echo -n .
cd $XPIROOT; OUT=`zip -vr ../venkman-$VERSION.xpi * 2>&1`; cd $XPIFILES
if [ "$?" != "0" ]; then echo ERROR; echo $OUT; fi
echo   ".           done"


echo "Build of JavaScript Debugger $VERSION... ALL DONE"
