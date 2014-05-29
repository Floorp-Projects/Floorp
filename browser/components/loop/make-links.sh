#!/bin/bash

if [ `basename $PWD` != "loop" ]; then
  echo "this script must be executed in the loop directory"
  exit -1
fi

if [ ! -d "$LOOP_CLIENT_REPO" ]; then
  echo "LOOP_CLIENT_REPO in the environment must be set to a valid directory"
  exit -1
fi

SHARED_CONTENT_TARGET=./content/shared
rm -f ${SHARED_CONTENT_TARGET}

SHARED_CONTENT=${LOOP_CLIENT_REPO}/content/shared
ln -s ${SHARED_CONTENT} ${SHARED_CONTENT_TARGET}

SHARED_TEST_TARGET=./test/shared
rm -f ${SHARED_TEST_TARGET}

SHARED_TEST=${LOOP_CLIENT_REPO}/test/shared
ln -s ${SHARED_TEST} ${SHARED_TEST_TARGET}


