# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

if [ $# = 0 ] ; then
    echo "usage: ./sync.sh ots-git-directory"
    exit 1
fi

echo "Updating LICENSE..."
cp $1/LICENSE .

echo "Updating src..."
cd src
ls | fgrep -v moz.build | xargs rm -rf
cp -r $1/src/* .
cd ..

echo "Updating include..."
rm -rf include/
cp -r $1/include .

echo "Updating README.mozilla..."
REVISION=`cd $1; git log | head -1 | sed "s/commit //"`
sed -e "s/\(Current revision: \).*/\1$REVISION/" -i "" README.mozilla

echo "Applying ots-visibility.patch..."
patch -p3 < ots-visibility.patch
