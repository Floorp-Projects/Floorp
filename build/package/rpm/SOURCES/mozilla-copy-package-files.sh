#!/bin/bash

# check our arguments

LIST_FILE=$1
PACKAGE_FILE=$2
TARGET_DIR=$3
SRC_DIR=$4
DIR_NAME=$5

if [ -z "$LIST_FILE" ]; then
    echo $0 list-file package-file target-dir src-dir
    exit 1
fi

if [ -z "$PACKAGE_FILE" ]; then
    echo $0 list-file package-file target-dir src-dir
    exit 1
fi

if [ -z "$TARGET_DIR" ]; then
    echo $0 list-file package-file target-dir src-dir
    exit 1
fi

if [ -z "$SRC_DIR" ]; then
    echo $0 list-file package-file target-dir src-dir
    exit 1
fi

if [ -z "$DIR_NAME" ]; then
    echo $0 list-file package-file target-dir src-dir
    exit 1
fi

if [ ! -f "$LIST_FILE" ]; then
    echo $0 list file not found
    exit 1
fi

if [ ! -d "$TARGET_DIR" ]; then
    echo $0 target dir not found
    exit 1
fi

if [ ! -d "$SRC_DIR" ]; then
    echo $0 src dir not found
    exit 1
fi

# try to figure out if we should be using cp -Lr or cp -r

touch /tmp/foo-copy-package
cp -L /tmp/foo-copy-package /tmp/foo-copy-package2 2>/dev/null >/dev/null
if [ "$?" -ne "0" ]; then
    COPY="cp"
else
    COPY="cp -L"
fi

rm -f /tmp/foo-copy-package /tmp/foo-copy-package2

# OK, do our file copy

cd $SRC_DIR

for i in `cat $LIST_FILE`
do
    $COPY -vrP ${i} ${TARGET_DIR}
    if [ "$?" -ne "0" ]; then
	echo "Copy of $i to $TARGET_DIR failed!"
    else
	echo ${DIR_NAME}/${i} >> $PACKAGE_FILE
    fi
done


