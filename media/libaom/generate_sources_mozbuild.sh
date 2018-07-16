#!/bin/bash -e
#
# Copyright (c) 2012 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# Modified from chromium/src/third_party/libaom/generate_gni.sh

# This script is used to generate sources.mozbuild and files in the
# config/platform directories needed to build libaom.
# Every time libaom source code is updated just run this script.
#
# Usage:
# $ ./generate_sources_mozbuild.sh

export LC_ALL=C
BASE_DIR=$(pwd)
LIBAOM_SRC_DIR="../../third_party/aom"
LIBAOM_CONFIG_DIR="config"

# Print license header.
# $1 - Output base name
function write_license {
  echo "# This file is generated. Do not edit." >> $1
  echo "" >> $1
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_config.h \
    -a $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_config.asm
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - any additional arguments to pass through.
function gen_rtcd_header {
  echo "Generate $LIBAOM_CONFIG_DIR/$1/*_rtcd.h files."

  rm -rf $TEMP_DIR/libaom.config
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_config.h \
    -a $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_config.asm \
    -o $TEMP_DIR/libaom.config

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_rtcd $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/av1/common/av1_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/av1_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_scale_rtcd $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_scale/aom_scale_rtcd.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_scale_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_dsp_rtcd $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_dsp/aom_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_dsp_rtcd.h

  rm -rf $TEMP_DIR/libaom.config
}

echo "Generating config files."
cd $BASE_DIR
python generate_sources_mozbuild.py

# Copy aom_version.h once. The file is the same for all platforms.
cp aom_version.h $BASE_DIR/$LIBAOM_CONFIG_DIR

echo "Remove temporary directory."
rm -rf $TEMP_DIR

echo "Create temporary directory."
TEMP_DIR="$BASE_DIR/.temp"
rm -rf $TEMP_DIR
cp -R $LIBAOM_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/ia32 x86
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header win/x64 x86_64
gen_rtcd_header win/ia32 x86
gen_rtcd_header win/mingw32 x86
gen_rtcd_header win/mingw64 x86_64

gen_rtcd_header linux/arm armv7

gen_rtcd_header generic generic

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

cd $BASE_DIR/$LIBAOM_SRC_DIR

cd $BASE_DIR
