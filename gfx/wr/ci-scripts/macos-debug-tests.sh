#!/usr/bin/env bash

# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/. */

# This must be run from the root webrender directory!
# Users may set the CARGOFLAGS environment variable to pass
# additional flags to cargo if desired.

set -o errexit
set -o nounset
set -o pipefail
set -o xtrace

CARGOFLAGS=${CARGOFLAGS:-"--verbose"}  # default to --verbose if not set

pushd webrender_api
cargo test ${CARGOFLAGS} --features "ipc"
popd

pushd webrender
cargo check ${CARGOFLAGS} --no-default-features
cargo check ${CARGOFLAGS} --no-default-features --features capture
cargo check ${CARGOFLAGS} --features capture,profiler
cargo check ${CARGOFLAGS} --features replay
cargo check ${CARGOFLAGS} --no-default-features --features pathfinder
popd

pushd wrench
cargo check ${CARGOFLAGS} --features env_logger
popd

pushd examples
cargo check ${CARGOFLAGS}
popd

cargo test ${CARGOFLAGS} --all
