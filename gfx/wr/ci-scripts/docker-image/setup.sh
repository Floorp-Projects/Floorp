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
    python3-pip \
    python3-six \
    python3-setuptools \
    python3-mako \
    software-properties-common \
    clang

# Other stuff we need

# Normally, we'd
#    pip3 install servo-tidy==0.3.0
# but the last version of servo-tidy published on pypi doesn't have the
# python3 fixes that are in the servo repo.
git clone -n https://github.com/servo/servo/
git -C servo checkout 65a4d1646da46c37fe748add6dcf24b62ebb602a
pip3 install servo/python/tidy
rm -rf servo
