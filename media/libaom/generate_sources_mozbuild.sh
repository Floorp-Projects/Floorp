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

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - any additional arguments to pass through.
function gen_rtcd_header {
  echo "Generate $LIBAOM_CONFIG_DIR/$1/*_rtcd.h files."

  AOM_CONFIG=$BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_config.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/cmake/rtcd.pl \
    --arch=$2 \
    --sym=av1_rtcd $3 \
    --config=$AOM_CONFIG \
    $BASE_DIR/$LIBAOM_SRC_DIR/av1/common/av1_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/av1_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/cmake/rtcd.pl \
    --arch=$2 \
    --sym=aom_scale_rtcd $3 \
    --config=$AOM_CONFIG \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_scale/aom_scale_rtcd.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_scale_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/cmake/rtcd.pl \
    --arch=$2 \
    --sym=aom_dsp_rtcd $3 \
    --config=$AOM_CONFIG \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_dsp/aom_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/config/aom_dsp_rtcd.h
}

echo "Generating config files."
python3 -m venv temp
. temp/bin/activate
pip install pyparsing==2.4.7
python3 generate_sources_mozbuild.py
deactivate
rm -r temp

# Copy aom_version.h once. The file is the same for all platforms.
cp aom_version.h $BASE_DIR/$LIBAOM_CONFIG_DIR

gen_rtcd_header linux/x64 x86_64
gen_rtcd_header linux/ia32 x86
gen_rtcd_header mac/x64 x86_64
gen_rtcd_header win/x64 x86_64
gen_rtcd_header win/ia32 x86

gen_rtcd_header linux/arm armv7

gen_rtcd_header generic generic

cd $BASE_DIR/$LIBAOM_SRC_DIR

cd $BASE_DIR
