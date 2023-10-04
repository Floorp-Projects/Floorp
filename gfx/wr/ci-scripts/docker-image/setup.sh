#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

test "$(whoami)" == 'root'

# Install stuff we need
apt-get -y update
apt-get install -y \
    bison \
    bzip2 \
    cmake \
    curl \
    flex \
    gcc \
    git \
    g++ \
    libfontconfig1-dev \
    libgl1-mesa-dev \
    libx11-dev \
    llvm-dev \
    ninja-build \
    pkg-config \
    software-properties-common \
    clang

# some reftests fail with freetype >= 2.10, so downgrade to the version in
# Debian buster. See bug 1804782.
apt-get -y remove libfreetype-dev
curl -LO http://snapshot.debian.org/archive/debian/20220718T031307Z/pool/main/f/freetype/libfreetype6_2.9.1-3%2Bdeb10u3_amd64.deb
curl -LO http://snapshot.debian.org/archive/debian/20220718T031307Z/pool/main/f/freetype/libfreetype6-dev_2.9.1-3%2Bdeb10u3_amd64.deb

dpkg -i libfreetype6*.deb
rm libfreetype6*.deb
