#!/usr/bin/env bash

# A script to generate toolkit/devtools/server/CoreDump.pb.{h,cc} from
# toolkit/devtools/server/CoreDump.proto. This script assumes you have
# downloaded and installed the protocol buffer compiler, and that it is either
# on your $PATH or located at $PROTOC_PATH.
#
# These files were last compiled with libprotoc 2.4.1.

set -e

cd $(dirname $0)

if [ -n $PROTOC_PATH ]; then
    PROTOC_PATH=`which protoc`
fi

if [ ! -e $PROTOC_PATH ]; then
    echo You must install the protocol compiler from
    echo https://code.google.com/p/protobuf/downloads/list
    exit 1
fi

echo Using $PROTOC_PATH as the protocol compiler

$PROTOC_PATH --cpp_out="." CoreDump.proto
