#!/bin/bash
# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

set -vex

if [[ -z "${MOZ_AUTOMATION}" ]]; then
	echo "Angle is only meant to be updated via ./mach vendor in automation."
	echo "However, if you set a few environment variables yourself, this script"
	echo "will probably work locally."
	exit 1
fi

if [ "$#" -ne 1 ]; then
	echo "Usage: auto-update-angle.sh upstream-tag"
	exit 1
fi

export DEPOT_TOOLS_WIN_TOOLCHAIN=0
export GYP_MSVS_OVERRIDE_PATH="$MOZ_FETCHES_DIR/VS"
export GYP_MSVS_VERSION=2019
export vs2019_install="$MOZ_FETCHES_DIR/VS"
export WINDOWSSDKDIR="$MOZ_FETCHES_DIR/SDK"
export WINDIR="$MOZ_FETCHES_DIR/WinDir"

set +v
export INCLUDE=""
export INCLUDE="$INCLUDE;$MOZ_FETCHES_DIR/VS/VC/Tools/MSVC/14.29.30133/ATLMFC/include"
export INCLUDE="$INCLUDE;$MOZ_FETCHES_DIR/VS/VC/Tools/MSVC/14.29.30133/include"

export LIB=""
export LIB="$LIB;$MOZ_FETCHES_DIR/VS/VC/Tools/MSVC/14.29.30133/ATLMFC/lib/x64"
export LIB="$LIB;$MOZ_FETCHES_DIR/VS/VC/Tools/MSVC/14.29.30133/lib/x64"
export LIB="$LIB;$MOZ_FETCHES_DIR/SDK/Lib/10.0.19041.0/um/x64"
export LIB="$LIB;$MOZ_FETCHES_DIR/SDK/Lib/10.0.19041.0/ucrt/x64"
set -v

# depot_tools
# This needs to use the /c/ format, rather than C:/ format.  PWD will translate for us though.
pushd $MOZ_FETCHES_DIR
MOZ_FETCHES_PATH=$(pwd)
popd
export PATH="$MOZ_FETCHES_PATH/depot_tools:$PATH"

# Do not update depot tools automatically
export DEPOT_TOOLS_UPDATE=0
pushd "$MOZ_FETCHES_DIR/depot_tools"
touch .disable_auto_update

################################################
if test -n "$GENERATE_DEPOT_TOOLS_BINARIES"; then
	# We're generating binaries, so run the setup manually
	cmd '/c cipd_bin_setup.bat'

	pushd bootstrap
	cmd '/c win_tools.bat'
	popd
else
	# Move the preloaded binaries into place so we don't need to do any setup
	mv "$MOZ_FETCHES_DIR"/depot_tools-preloaded-binaries/* .
	# Move the hidden files also. If we don't do the .[^.]* we get an error trying to move . and ..
	mv "$MOZ_FETCHES_DIR"/depot_tools-preloaded-binaries/.[^.]* .
fi

################################################

popd

# do the update
cd "$MOZ_FETCHES_DIR"
git clone https://chromium.googlesource.com/angle/angle
cd angle
git checkout "origin/$1"

python3 scripts/bootstrap.py

gclient sync

python3 "$GECKO_PATH/gfx/angle/update-angle.py" origin

cd $GECKO_PATH
hg status
