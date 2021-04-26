#!/bin/bash

# Print an Error message if `set -eE` causes the script to exit due to a failed command
trap 'echo "*** ERROR *** Generation did not complete successfully!"' ERR 

# If DEBUG_GEN is set all commands should be printed as they are executed
if [ ! "x$DEBUG_GEN" = "x" ]; then
  set -x
fi

if [ "x$MOZ_LIBWEBRTC" = "x" ]; then
  echo "MOZ_LIBWEBRTC is not defined, see README.md"
  exit
fi

if [ -d $MOZ_LIBWEBRTC ]; then
  echo "MOZ_LIBWEBRTC is $MOZ_LIBWEBRTC"
else
  echo "Path $MOZ_LIBWEBRTC is not found, see README.md"
  exit
fi

# git clone and gclient checkout may be in different places 
if [ "x$MOZ_LIBWEBRTC_GIT" = "x" ]; then
  MOZ_LIBWEBRTC_GIT=$MOZ_LIBWEBRTC
fi

if [ ! -d $MOZ_LIBWEBRTC_GIT/.git ]; then
  echo "No .git directory is found in the libwebrtc checkout, see README.md"
  exit
fi

if [ ! -d $MOZ_LIBWEBRTC/src/buildtools ]; then
  echo "Path $MOZ_LIBWEBRTC/src/buildtools is not found, see README.md"
  echo "Please run the following commands from inside $MOZ_LIBWEBRTC:"
  echo "\tgclient config https://github.com/mozilla/libwebrtc"
  echo "\tgclient sync -D --force --reset --with_branch_heads # this make take a while"
  exit
fi

if [ "x$DEPOT_TOOLS" = "x" ]; then
  echo "DEPOT_TOOLS is not defined, see README.md"
  exit
fi

if [ -d $DEPOT_TOOLS ]; then
  echo "DEPOT_TOOLS is $DEPOT_TOOLS"
else
  echo "Path $DEPOT_TOOLS is not found, see README.md"
fi

# After this point:
# * eE: All commands should succede.
# * u: All variables should be defined before use.
# * o pipefail: All stages of all pipes should succede.
set -eEuo pipefail

SYS_NAME=`uname`

# For now, only macOS, Windows, and Linux are supported here.
if [ "x$SYS_NAME" = "xDarwin" ]; then
  CONFIGS="x64_False_arm64_mac x64_True_arm64_mac x64_False_x64_mac x64_True_x64_mac"
elif [ "x$SYS_NAME" = "xMINGW32_NT-6.2" ]; then
  CONFIGS="x64_True_x64_win x64_False_x64_win"
else
  CONFIGS="x64_False_x64_linux x64_True_x64_linux"
fi

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

# The path to DEPOT_TOOLS should be on our path, and make sure that it doesn't
# auto-update.
export PATH=$DEPOT_TOOLS:$PATH
export DEPOT_TOOLS_UPDATE=0

# Symlink in the buildtoosl and .git director from our copy of libwebrtc.
if [ -L ./third_party/libwebrtc/buildtools ]; then
  rm ./third_party/libwebrtc/buildtools
elif [ -d ./third_party/libwebrtc/buildtools ]; then
  rm -rf ./third_party/libwebrtc/buildtools
fi
ln -s $MOZ_LIBWEBRTC/src/buildtools ./third_party/libwebrtc/

if [ -L ./third_party/libwebrtc/.git ]; then
  rm ./third_party/libwebrtc/.git
elif [ -d ./third_party/libwebrtc/.git ]; then
  rm -rf ./third_party/libwebrtc/.git
fi
ln -s $MOZ_LIBWEBRTC_GIT/.git ./third_party/libwebrtc/

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
  export MOZCONFIG=dom/media/webrtc/third_party_build/gn-configs/$THIS_BUILD.mozconfig 
  echo "Using MOZCONFIG=$MOZCONFIG"

  ./mach configure | tee $THIS_BUILD.configure.log
  if [ ! -d obj-$THIS_BUILD ]; then
    echo "Expected build output directory obj-$THIS_BUILD is missing, ensure this is set in $MOZCONFIG"
    exit 1
  fi
  ./mach build-backend -b GnConfigGen --verbose | tee $THIS_BUILD.build-backend.log
  cp obj-$THIS_BUILD/third_party/libwebrtc/gn-output/$THIS_BUILD.json dom/media/webrtc/third_party_build/gn-configs
done

# run some fixup (mostly removing dev-machine dependent info) from json files
for file in `ls dom/media/webrtc/third_party_build/gn-configs/*.json`
do
  echo "fixup file: $file"
  ./dom/media/webrtc/third_party_build/gn-configs/fixup_json.py $file
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

echo
echo "Done generating gn build files. You should now be able to build with ./mach build"
