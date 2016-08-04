#!/bin/bash

set -e

if ! git remote -v | grep origin | grep -q cctools-port; then
    echo "must be in a cctools-port checkout"
    exit 1
fi

mkdir build-cctools
cd build-cctools

CFLAGS='-mcpu=generic -mtune=generic' MACOSX_DEPLOYMENT_TARGET=10.7 ../cctools/configure --target=x86_64-apple-darwin11
env MACOSX_DEPLOYMENT_TARGET=10.7 make -s -j4

if test ! -e ld64/src/ld/ld; then
    echo "ld did not get built"
    exit 1
fi

gtar jcf cctools.tar.bz2 ld64/src/ld/ld --transform 's#ld64/src/ld#cctools/bin#'

cd ../

echo "build from $(git show --pretty=format:%H -s HEAD) complete!"
echo "upload the build-cctools/cctools.tar.bz2 file to tooltool"
