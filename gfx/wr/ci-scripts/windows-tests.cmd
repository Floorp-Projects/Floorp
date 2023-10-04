:: This Source Code Form is subject to the terms of the Mozilla Public
:: License, v. 2.0. If a copy of the MPL was not distributed with this
:: file, You can obtain one at http://mozilla.org/MPL/2.0/. */

:: This must be run from the root webrender directory!
:: Users may set the CARGOFLAGS environment variable to pass
:: additional flags to cargo if desired.

if NOT DEFINED CARGOFLAGS SET CARGOFLAGS=--verbose

python3 -m pip install -r %0\\..\\requirements.txt

pushd webrender_api
cargo test %CARGOFLAGS%
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
popd

pushd webrender
cargo test %CARGOFLAGS%
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
popd

pushd wrench
cargo test --verbose
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
:: Test that all shaders compile successfully and pass tests.
:: --precache compiles all shaders during initialization, therefore if init
:: is successful then the shaders compile.
cargo run --release -- --angle --precache test_init
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
cargo run --release -- --angle --precache --use-unoptimized-shaders test_init
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
cargo run --release -- --angle test_shaders
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%

cargo run --release -- --angle reftest
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
popd

pushd examples
cargo check --verbose
if %ERRORLEVEL% NEQ 0 EXIT /b %ERRORLEVEL%
popd
