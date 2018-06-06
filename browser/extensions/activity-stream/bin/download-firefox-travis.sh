#!/bin/bash
# Copied and slightly modified from https://github.com/lidel/ipfs-firefox-addon/commit/d656832eec807ebae59543982dde96932ce5bb7c
# Licensed under Creative Commons -  CC0 1.0 Universal - https://github.com/lidel/ipfs-firefox-addon/blob/master/LICENSE
BUILD_TYPE=${1:-$FIREFOX_RELEASE}
echo "Looking up latest URL for $BUILD_TYPE"
BUILD_ROOT="/pub/firefox/tinderbox-builds/mozilla-${BUILD_TYPE}/"
ROOT="https://archive.mozilla.org"
LATEST=$(curl -s "$ROOT$BUILD_ROOT" | grep $BUILD_TYPE | grep -Po '<a href=".+">\K[[:digit:]]+' | sort -n | tail -1)
echo "Latest build located at $ROOT$BUILD_ROOT$LATEST"
FILE=$(curl -s "$ROOT$BUILD_ROOT$LATEST/" | grep '.tar.' | grep -Po '<a href="\K[^"]*')
echo "URL: $ROOT$FILE"
wget -O "firefox-${BUILD_TYPE}.tar.bz2" "$ROOT$FILE" && tar xf "firefox-${BUILD_TYPE}.tar.bz2"
