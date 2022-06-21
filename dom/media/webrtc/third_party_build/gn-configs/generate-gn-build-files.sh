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

CONFIG_DIR=dom/media/webrtc/third_party_build/gn-configs
echo "CONFIG_DIR is $CONFIG_DIR"

export MOZ_OBJDIR=$(mktemp -d -p . obj-XXXXXXXXXX)
./mach configure
if [ ! -d $MOZ_OBJDIR ]; then
  echo "Expected build output directory $MOZ_OBJDIR is missing"
  exit 1
fi
./mach build-backend -b GnConfigGen --verbose | tee $MOZ_OBJDIR/build-backend.log
cp $MOZ_OBJDIR/third_party/libwebrtc/gn-output/*.json $CONFIG_DIR

# run some fixup (mostly removing dev-machine dependent info) on json files
for THIS_CONFIG in $CONFIG_DIR/*.json
do
  echo "fixup file: $THIS_CONFIG"
  ./$CONFIG_DIR/fixup_json.py $THIS_CONFIG
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

rm -rf $MOZ_OBJDIR

echo
echo "Done generating gn build files. You should now be able to build with ./mach build"
