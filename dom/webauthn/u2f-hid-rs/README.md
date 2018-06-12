# A Rust HID library for interacting with U2F Security Keys

[![Build Status](https://travis-ci.org/jcjones/u2f-hid-rs.svg?branch=master)](https://travis-ci.org/jcjones/u2f-hid-rs)
![Maturity Level](https://img.shields.io/badge/maturity-beta-yellow.svg)

This is a cross-platform library for interacting with U2F Security Key-type devices via Rust.

* **Supported Platforms**: Windows, Linux, FreeBSD, and macOS.
* **Supported HID Transports**: USB.
* **Supported Protocols**: [FIDO U2F over USB](https://fidoalliance.org/specs/fido-u2f-v1.1-id-20160915/fido-u2f-raw-message-formats-v1.1-id-20160915.html).

This library currently focuses on U2F security keys, but is expected to be extended to
support additional protocols and transports.

## Usage

There's only a simple example function that tries to register and sign right now. It uses
[env_logger](http://rust-lang-nursery.github.io/log/env_logger/) for logging, which you
configure with the `RUST_LOG` environment variable:

```
cargo build --example main
RUST_LOG=debug cargo run --example main
```

Proper usage should be to call into this library from something else - e.g., Firefox. There are
some [C headers exposed for the purpose](u2f-hid-rs/blob/master/src/u2fhid-capi.h).

## Tests

There are some tests of the cross-platform runloop logic and the protocol decoder:

```
cargo test
```

## Fuzzing

There are fuzzers for the USB protocol reader, basically fuzzing inputs from the HID layer.
There are not (yet) fuzzers for the C API used by callers (such as Gecko).

To fuzz, you will need cargo-fuzz (the latest version from GitHub) as well as Rust Nightly.

```
rustup install nightly
cargo install --git https://github.com/rust-fuzz/cargo-fuzz/

cargo +nightly fuzz run u2f_read -- -max_len=512
cargo +nightly fuzz run u2f_read_write -- -max_len=512
```
