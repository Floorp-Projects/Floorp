#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# This must be run from the root webrender directory!
# Users may set the CARGOFLAGS environment variable to pass
# additional flags to cargo if desired.
# The WRENCH_BINARY environment variable, if set, is used to run
# the precached reftest.

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

CARGOFLAGS=${CARGOFLAGS:-""}  # default to empty if not set
WRENCH_BINARY=${WRENCH_BINARY:-""}

python3 -m pip install -r $(dirname ${0})/requirements.txt

pushd wrench

# Test that all shaders compile successfully and pass tests.
python3 script/headless.py --precache test_init
python3 script/headless.py --precache --use-unoptimized-shaders test_init
python3 script/headless.py test_shaders

python3 script/headless.py reftest
python3 script/headless.py test_invalidation
if [[ -z "${WRENCH_BINARY}" ]]; then
    cargo build ${CARGOFLAGS} --release
    WRENCH_BINARY="../target/release/wrench"
fi
"${WRENCH_BINARY}" --precache \
    reftest reftests/clip/fixed-position-clipping.yaml
popd
