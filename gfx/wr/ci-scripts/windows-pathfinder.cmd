:: This Source Code Form is subject to the terms of the Mozilla Public
:: License, v. 2.0. If a copy of the MPL was not distributed with this
:: file, You can obtain one at http://mozilla.org/MPL/2.0/. */

:: This must be run from the root webrender directory!
:: Users may set the CARGOFLAGS environment variable to pass
:: additional flags to cargo if desired.

if NOT DEFINED CARGOFLAGS SET CARGOFLAGS=--verbose

pushd webrender
cargo check %CARGOFLAGS% --no-default-features --features pathfinder
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
popd
