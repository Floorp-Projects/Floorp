#!/bin/sh

if [ "x$MOZ_LIBWEBRTC" = "x" ]; then
  echo "MOZ_LIBWEBRTC is not defined, see README.md"
  exit
fi

if [ -d $MOZ_LIBWEBRTC ]; then
  echo "MOZ_LIBWEBRTC is $MOZ_LIBWEBRTC"
else
  echo "Path $MOZ_LIBWEBRTC is not found, see README.md"
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

SYS_NAME=`uname`

# For now, only macOS and linux are supported here.  This is where we'd add Windows
# support.
if [ "x$SYS_NAME" = "xDarwin" ]; then
  CONFIGS="x64_False_arm64_mac x64_True_arm64_mac x64_False_x64_mac x64_True_x64_mac"
else
  CONFIGS="x64_False_x64_linux x64_True_x64_linux"
fi

# Completely clean the mercurial checkout before proceeding
hg update -C -r .
hg purge

# The path to DEPOT_TOOLS should be on our path, and make sure that it doesn't
# auto-update.
export PATH=$DEPOT_TOOLS:$PATH
export DEPOT_TOOLS_UPDATE=0

# Symlink in the buildtoosl and .git director from our copy of libwebrtc.
ln -s $MOZ_LIBWEBRTC/src/buildtools ./third_party/libwebrtc/
ln -s $MOZ_LIBWEBRTC/.git ./third_party/libwebrtc/

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
  rm .mozconfig
  ln -s dom/media/webrtc/third_party_build/gn-configs/$THIS_BUILD.mozconfig .mozconfig

  ./mach configure > $THIS_BUILD.configure.log
  ./mach build-backend -b GnConfigGen > $THIS_BUILD.build-backend.log

  cp obj-$THIS_BUILD/third_party/libwebrtc/gn-output/*.json dom/media/webrtc/third_party_build/gn-configs
done

# run some fixup (mostly removing dev-machine dependent info) from json files
for file in `ls dom/media/webrtc/third_party_build/gn-configs/*.json`
do
  echo "fixup file: $file"
  ./dom/media/webrtc/third_party_build/gn-configs/fixup_json.py $file
done

# The symlinks are no longer needed after generating the .json files.
rm third_party/libwebrtc/buildtools
rm third_party/libwebrtc/.git

# After all the json files are generated they are used to generate moz.build files.
echo "Building moz.build files from gn json files"
./mach build-backend -b GnMozbuildWriter

echo "Done"
