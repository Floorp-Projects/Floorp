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

# Search for source files with the same basename.
# The build does not support duplicate file names.
function find_duplicates {
  local readonly duplicate_file_names=$(find \
    $BASE_DIR/$LIBAOM_SRC_DIR/vp8 \
    $BASE_DIR/$LIBAOM_SRC_DIR/vp9 \
    $BASE_DIR/$LIBAOM_SRC_DIR/av1 \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_dsp \
    -type f -name \*.c  | xargs -I {} basename {} | sort | uniq -d \
  )

  if [ -n "${duplicate_file_names}" ]; then
    echo "ERROR: DUPLICATE FILES FOUND"
    for file in  ${duplicate_file_names}; do
      find \
        $BASE_DIR/$LIBAOM_SRC_DIR/vp8 \
        $BASE_DIR/$LIBAOM_SRC_DIR/vp9 \
        $BASE_DIR/$LIBAOM_SRC_DIR/av1 \
        $BASE_DIR/$LIBAOM_SRC_DIR/aom \
        $BASE_DIR/$LIBAOM_SRC_DIR/aom_dsp \
        -name $file
    done
    exit 1
  fi
}

# Generate sources.mozbuild with a list of source files.
# $1 - Array name for file list. This is processed with 'declare' below to
#      regenerate the array locally.
# $2 - Variable name.
# $3 - Output file.
function write_sources {
  # Convert the first argument back in to an array.
  declare -a file_list=("${!1}")

  echo "  '$2': [" >> "$3"
  for f in $file_list
  do
    echo "    '$LIBAOM_SRC_DIR/$f'," >> "$3"
  done
  echo "]," >> "$3"
}

# Convert a list of source files into sources.mozbuild.
# $1 - Input file.
# $2 - Output prefix.
function convert_srcs_to_project_files {
  # Do the following here:
  # 1. Filter .c, .h, .s, .S and .asm files.
  # 3. Convert .asm.s to .asm because moz.build will do the conversion.

  local source_list=$(grep -E '(\.c|\.h|\.S|\.s|\.asm)$' $1)

  # Remove aom_config.c.
  source_list=$(echo "$source_list" | grep -v 'aom_config\.c')

  # The actual ARM files end in .asm. We have rules to translate them to .S
  source_list=$(echo "$source_list" | sed s/\.asm\.s$/.asm/)

  # Exports - everything in aom, aom_mem, aom_ports, aom_scale
  local exports_list=$(echo "$source_list" | \
    egrep '^(aom|aom_mem|aom_ports|aom_scale)/.*h$')
  # but not anything in one level down, like 'internal'
  exports_list=$(echo "$exports_list" | egrep -v '/(internal|src)/')
  # or any of the other internal-ish header files.
  exports_list=$(echo "$exports_list" | egrep -v '/(emmintrin_compat.h|mem_.*|msvc.h|aom_once.h)$')

  # Remove these files from the main list.
  source_list=$(comm -23 <(echo "$source_list") <(echo "$exports_list"))

  # Write a single file that includes all source files for all archs.
  local c_sources=$(echo "$source_list" | egrep '.(asm|c)$')
  local exports_sources=$(echo "$exports_list" | egrep '.h$')

  write_sources exports_sources ${2}_EXPORTS "$BASE_DIR/sources.mozbuild"
  write_sources c_sources ${2}_SOURCES "$BASE_DIR/sources.mozbuild"
}

# Clean files from previous make.
function make_clean {
  make clean > /dev/null
  rm -f libaom_srcs.txt
}

# Print the configuration.
# $1 - Header file directory.
function print_config {
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_config.h \
    -a $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_config.asm
}

# Generate *_rtcd.h files.
# $1 - Header file directory.
# $2 - Architecture.
# $3 - Optional - any additional arguments to pass through.
function gen_rtcd_header {
  echo "Generate $LIBAOM_CONFIG_DIR/$1/*_rtcd.h files."

  rm -rf $TEMP_DIR/libaom.config
  $BASE_DIR/lint_config.sh -p \
    -h $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_config.h \
    -a $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_config.asm \
    -o $TEMP_DIR/libaom.config

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_rtcd $DISABLE_AVX $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/av1/common/av1_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/av1_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_scale_rtcd $DISABLE_AVX $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_scale/aom_scale_rtcd.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_scale_rtcd.h

  $BASE_DIR/$LIBAOM_SRC_DIR/build/make/rtcd.pl \
    --arch=$2 \
    --sym=aom_dsp_rtcd $DISABLE_AVX $3 \
    --config=$TEMP_DIR/libaom.config \
    $BASE_DIR/$LIBAOM_SRC_DIR/aom_dsp/aom_dsp_rtcd_defs.pl \
    > $BASE_DIR/$LIBAOM_CONFIG_DIR/$1/aom_dsp_rtcd.h

  rm -rf $TEMP_DIR/libaom.config
}

# Generate Config files. "--enable-external-build" must be set to skip
# detection of capabilities on specific targets.
# $1 - Header file directory.
# $2 - Config command line.
function gen_config_files {
  ./configure $2 #> /dev/null

  # Disable HAVE_UNISTD_H.
  ( echo '/HAVE_UNISTD_H'; echo 'd' ; echo 'w' ; echo 'q' ) | ed -s aom_config.h

  local ASM_CONV=ads2gas.pl

  # Generate aom_config.asm.
  if [[ "$1" == *x64* ]] || [[ "$1" == *ia32* ]]; then
    egrep "#define [A-Z0-9_]+ [01]" aom_config.h | awk '{print "%define " $2 " " $3}' > aom_config.asm
  else
    egrep "#define [A-Z0-9_]+ [01]" aom_config.h | awk '{print $2 " EQU " $3}' | perl $BASE_DIR/$LIBAOM_SRC_DIR/build/make/$ASM_CONV > aom_config.asm
  fi

  mkdir -p $BASE_DIR/$LIBAOM_CONFIG_DIR/$1
  cp aom_config.* $BASE_DIR/$LIBAOM_CONFIG_DIR/$1
  make_clean
  rm -rf aom_config.*
}

find_duplicates

echo "Creating temporary directory."
TEMP_DIR="$BASE_DIR/.temp"
rm -rf $TEMP_DIR
cp -R $LIBAOM_SRC_DIR $TEMP_DIR
cd $TEMP_DIR

echo "Generating config files."
all_platforms="--enable-external-build --disable-examples --disable-docs --disable-unit-tests"
all_platforms="${all_platforms} --size-limit=8192x4608 --enable-pic"
x86_platforms="--enable-postproc --as=yasm"
arm_platforms="--enable-runtime-cpu-detect --enable-realtime-only"
gen_config_files linux/x64 "--target=x86_64-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files linux/ia32 "--target=x86-linux-gcc ${all_platforms} ${x86_platforms}"
gen_config_files mac/x64 "--target=x86_64-darwin9-gcc ${all_platforms} ${x86_platforms}"
gen_config_files win/x64 "--target=x86_64-win64-vs14 ${all_platforms} ${x86_platforms}"
gen_config_files win/ia32 "--target=x86-win32-vs14 ${all_platforms} ${x86_platforms}"

gen_config_files linux/arm "--target=armv7-linux-gcc ${all_platforms} ${arm_platforms}"

gen_config_files generic "--target=generic-gnu ${all_platforms}"

echo "Remove temporary directory."
cd $BASE_DIR
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

gen_rtcd_header linux/arm armv7

gen_rtcd_header generic generic

echo "Prepare Makefile."
./configure --target=generic-gnu > /dev/null
make_clean

# Remove existing source files.
rm -rf $BASE_DIR/sources.mozbuild
write_license $BASE_DIR/sources.mozbuild
echo "files = {" >> $BASE_DIR/sources.mozbuild

echo "Generate X86_64 source list."
config=$(print_config linux/x64)
make_clean
make libaom_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libaom_srcs.txt X64

# Copy aom_version.h once. The file is the same for all platforms.
cp aom_version.h $BASE_DIR/$LIBAOM_CONFIG_DIR

echo "Generate IA32 source list."
config=$(print_config linux/ia32)
make_clean
make libaom_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libaom_srcs.txt IA32

echo "Generate ARM source list."
config=$(print_config linux/arm)
make_clean
make libaom_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libaom_srcs.txt ARM

echo "Generate generic source list."
config=$(print_config generic)
make_clean
make libaom_srcs.txt target=libs $config > /dev/null
convert_srcs_to_project_files libaom_srcs.txt GENERIC

echo "}" >> $BASE_DIR/sources.mozbuild

echo "Remove temporary directory."
cd $BASE_DIR
rm -rf $TEMP_DIR

cd $BASE_DIR/$LIBAOM_SRC_DIR

cd $BASE_DIR
