#!/bin/bash

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** $? $LINENO Generation did not complete successfully!"' ERR 

# If DEBUG_GEN is set all commands should be printed as they are executed
if [ ! "x$DEBUG_GEN" = "x" ]; then
  set -x
fi

if [ "x$GN" = "x" ]; then
  echo "GN is not defined, see README.md"
  exit
fi

if [ -f $GN ]; then
  echo "GN is $GN"
else
  echo "Path $GN is not found, see README.md"
  exit
fi

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

SYS_NAME=`uname`

# Check for modified files and abort if present.
MODIFIED_FILES=`hg status --modified --added --exclude "**/moz.build" --exclude "dom/media/webrtc/third_party_build/**.json"`
if [ "x$MODIFIED_FILES" = "x" ]; then
  # Completely clean the mercurial checkout before proceeding
  hg update -C -r .
  hg purge
else
  echo "There are modified files in the checkout. Cowardly aborting!"
  echo "$MODIFIED_FILES"
  exit 1
fi

IS_WIN=0
IS_DARWIN=0
IS_LINUX=0

# Erase content of third_party/libwebrtc/moz.build to help with generating
# files that may conflict with current moz.build files.  Each config calls
# ./mach configure.  If there are conflicts in any moz.build files under
# third_party/libwebrtc, it would keep our configure step from completing
# successfully.  Since this file will be regenerated at the end of this
# process, we can make it an empty file to avoid conflicts during the
# configure step.
echo "" > third_party/libwebrtc/moz.build

# For now, only macOS, Windows, and Linux (including Android builds) are supported here.
if [ "x$SYS_NAME" = "xDarwin" ]; then
  CONFIGS="x64_False_arm64_mac x64_True_arm64_mac x64_False_x64_mac x64_True_x64_mac"
  IS_DARWIN=1
elif [ "x$SYS_NAME" = "xMINGW32_NT-6.2" ]; then
  unset ANSICON
  CONFIGS="x64_True_arm64_win x64_False_arm64_win"
  CONFIGS="$CONFIGS x64_True_x64_win x64_False_x64_win"
  CONFIGS="$CONFIGS x64_True_x86_win x64_False_x86_win"
  IS_WIN=1
elif [ "x$SYS_NAME" = "xOpenBSD" ]; then
  CONFIGS="x64_False_x64_openbsd x64_True_x64_openbsd"
else
  # Ensure rust has the correct targets for building x86 and arm64.  These
  # operations succeed quickly if previously completed.
  rustup target add aarch64-unknown-linux-gnu
  rustup target add i686-unknown-linux-gnu

  CONFIGS="x64_False_x64_linux_False x64_True_x64_linux_False"
  CONFIGS="$CONFIGS x64_False_x64_linux_True x64_True_x64_linux_True"
  CONFIGS="$CONFIGS x64_False_x86_linux_False x64_True_x86_linux_False"
  CONFIGS="$CONFIGS x64_False_x86_linux_True x64_True_x86_linux_True"
  CONFIGS="$CONFIGS x64_False_arm64_linux_False x64_True_arm64_linux_False"
  CONFIGS="$CONFIGS x64_False_arm64_linux_True x64_True_arm64_linux_True"
  CONFIGS="$CONFIGS x64_False_arm_android x64_True_arm_android"
  CONFIGS="$CONFIGS x64_False_x64_android x64_True_x64_android"
  CONFIGS="$CONFIGS x64_False_x86_android x64_True_x86_android"
  CONFIGS="$CONFIGS x64_False_arm64_android x64_True_arm64_android"
  IS_LINUX=1
fi

CONFIG_DIR=dom/media/webrtc/third_party_build/gn-configs
echo "CONFIG_DIR is $CONFIG_DIR"

# Each mozconfig is for a particular "platform" and defines debug/non-debug and a
# MOZ_OBJDIR based on the name of the mozconfig to make it easier to know where to
# find the generated gn json file.
# The output of this step is a series of gn produced json files with a name format
# that follows the form a-b-c-d.json where:
#   a = generating cpu (example: x64)
#   b = debug (True / False)
#   c = target cpu (example: x64 / arm64)
#   d = target platform (mac/linux)
for THIS_BUILD in $CONFIGS
do
  echo "Building gn json file for $THIS_BUILD"
  export MOZCONFIG=$CONFIG_DIR/$THIS_BUILD.mozconfig
  echo "Using MOZCONFIG=$MOZCONFIG"

  ./mach configure | tee $THIS_BUILD.configure.log
  if [ ! -d obj-$THIS_BUILD ]; then
    echo "Expected build output directory obj-$THIS_BUILD is missing, ensure this is set in $MOZCONFIG"
    exit 1
  fi
  ./mach build-backend -b GnConfigGen --verbose | tee $THIS_BUILD.build-backend.log
  cp obj-$THIS_BUILD/third_party/libwebrtc/gn-output/$THIS_BUILD.json $CONFIG_DIR
done

# run some fixup (mostly removing dev-machine dependent info) on json files
for THIS_CONFIG in $CONFIGS
do
  echo "fixup file: $CONFIG_DIR/$THIS_CONFIG.json"
  ./$CONFIG_DIR/fixup_json.py $CONFIG_DIR/$THIS_CONFIG.json
  if [ "$IS_WIN" == 1 ]; then
    # Use Linux/UNIX line endings
    dos2unix $CONFIG_DIR/$THIS_CONFIG.json
  fi
done

#  The symlinks are no longer needed after generating the .json files.
if [ -L third_party/libwebrtc/buildtools ]; then
  rm third_party/libwebrtc/buildtools
else
  rm -rf third_party/libwebrtc/buildtools
fi

if [ -L third_party/libwebrtc/buildtools ]; then
  rm third_party/libwebrtc/.git
else
  rm -rf third_party/libwebrtc/.git
fi

# After all the json files are generated they are used to generate moz.build files.
echo "Building moz.build files from gn json files"
./mach build-backend -b GnMozbuildWriter --verbose

# Make sure all the moz.build files have unix line endings if generated
# on Windows.
if [ "$IS_WIN" == 1 ]; then
  MODIFIED_BUILD_FILES=`hg status --modified --added --no-status --include '**/moz.build'`
  for BUILD_FILE in $MODIFIED_BUILD_FILES
  do
    dos2unix $BUILD_FILE
  done
fi

echo
echo "Done generating gn build files. You should now be able to build with ./mach build"
