#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# The accompanying setup.sh file is used when building both the "standalone"
# docker image (via the accompanying Dockerfile) for use in the
# github-taskcluster test, as well as by the docker image built for Firefox CI.
# This additional setup script contains only the additional things needed
# for the "standalone" docker image.

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

# Pull clang-9 from Firefox CI's taskcluster instance. We could probably
# add the LLVM repository to Debian and pull from there, but the Firefox CI
# version is patched and for consistency with Firefox CI we use the Firefox
# version.
FIREFOX_TC=https://firefox-ci-tc.services.mozilla.com/api/index/v1/task
CLANG9_INDEX=gecko.cache.level-3.toolchains.v3.linux64-clang-9.latest
curl -sSfL -o clang.tar.xz \
  "${FIREFOX_TC}/${CLANG9_INDEX}/artifacts/public/build/clang.tar.xz"
tar xf clang.tar.xz
