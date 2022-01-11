# Testing & Debugging Rust Code

This page explains how to test and debug Rust code in Firefox.

The [build documentation](/build/buildsystem/rust.rst) explains how to add
new Rust code to Firefox. The [code documentation](/writing-rust-code/index.md)
explains how to write and work with Rust code in Firefox.

## Testing Mozilla crates

Rust code will naturally be tested as part of system tests such as Mochitests.
This section describes the two methods for unit testing of individual Rust
crates. Which method should be used depends on the circumstances.

### Rust tests

If a Mozilla crate has "normal" Rust tests (i.e. `#[test]` functions that run
with `cargo test`), you can add the crate's name to `RUST_TESTS` in
[toolkit/library/rust/moz.build](https://searchfox.org/mozilla-central/source/toolkit/library/rust/moz.build).
(Cargo features can be activated for Rust tests by adding them to
`RUST_TEST_FEATURES` in the same file.)

Rust tests are run with `./mach rusttests`. They run on automation in a couple
of `rusttests` jobs, but not on all platforms.

Rust tests have one major restriction: they cannot link against Gecko symbols.
Therefore, Rust tests cannot be used for crates that use Gecko crates like
`nsstring` and `xpcom`.

It's also possible to use `RUST_TESTS` in a different `moz.build` file. See
`testing/geckodriver/moz.build` and the [geckodriver testing docs] for an
example.

[geckodriver testing docs]: /testing/geckodriver/Testing.md

### GTests

Another way to unit test a Mozilla crate is by writing a GTest that uses FFI to
call into Rust code. This requires the following steps.
- Create a new test crate whose name is the same as the name of crate being
  tested, with a `-gtest` suffix.
- Add to the test crate a Rust file, a C++ file containing GTest `TEST()`
  functions that use FFI to call into the Rust file, a `Cargo.toml` file that
  references the Rust file, and a `moz.build` file that references the C++
  file.
- Add an entry to the `[dependencies]` section in
  [toolkit/library/gtest/rust/Cargo.toml](https://searchfox.org/mozilla-central/source/toolkit/library/gtest/rust/Cargo.toml).
- Add an `extern crate` entry to
  [toolkit/library/gtest/rust/lib.rs](https://searchfox.org/mozilla-central/source/toolkit/library/gtest/rust/lib.rs).

See
[xpcom/rust/gtest/nsstring/](https://searchfox.org/mozilla-central/source/xpcom/rust/gtest/nsstring)
for a simple example. (Note that the `moz.build` file is in the parent
directory for that crate.)

A Rust GTest can be run like any other GTest via `./mach gtest`, using the C++
`TEST()` functions as the starting point.

Unlike Rust tests, GTests can be used when linking against Gecko symbols is required.

## Testing third-party crates

In general we don't run tests for third-party crates. The assumption is that
these crates are sufficiently well-tested elsewhere.

## Debugging Rust code

In theory, Rust code is debuggable much like C++ code, using standard tools
like `gdb`, `rr`, and the Microsoft Visual Studio Debugger. In practice, the
experience can be worse, because shortcomings such as the following can occur.
- Inability to print local variables, even in non-optimized builds.
- Inability to call generic functions.
- Missing line numbers and stack frames.
- Printing of basic types such as `Option` and `Vec` is sometimes sub-optimal.
  If you see a warning "Missing auto-load script at offset 0 in section
  `.debug_gdb_scripts`" when starting `gdb`, the `rust-gdb` wrapper may give
  better results.

## Logging from Rust code

### Rust logging

The `RUST_LOG` environment variable (from the `env_logger` crate) can be used
to enable logging to stderr from Rust code in Firefox. The logging macros from
the `log` crate can be used. In order of importance, they are: `error!`,
`warn!`, `info!`, `debug!`, `trace!`.

For example, to show all log messages of `info` level or higher, run:
```
RUST_LOG=info firefox
```
Module-level logging can also be specified, see the [documentation] for the
`env_logger` crate for details.

To restrict logging to child processes, use `RUST_LOG_CHILD` instead of
`RUST_LOG`.

[documentation]: https://docs.rs/env_logger/

### Gecko logging

Rust logging can also be forwarded to the [Gecko logger] for capture via
`MOZ_LOG` and `MOZ_LOG_FILE`.

[Gecko logger]: /xpcom/logging.rst

- When parsing modules from `MOZ_LOG`, modules containing `::` are considered
  to be Rust modules. To log everything in a top-level module like
  `neqo_transport`, specify it as `neqo_transport::*`. For example:
```
MOZ_LOG=timestamp,sync,nsHostResolver:5,neqo_transport::*:5,proxy:5 firefox
```
- When logging from a submodule the `::*` is allowed but isn't necessary.
  So these two lines are equivalent:
```
MOZ_LOG=timestamp,sync,neqo_transport::recovery:5 firefox
MOZ_LOG=timestamp,sync,neqo_transport::recovery::*:5 firefox
```
- `debug!` and `trace!` logs will not appear in non-debug builds. This is due
  to our use of the `release_max_level_info` feature in the `log` crate.

- When using both `MOZ_LOG` and `RUST_LOG`, modules that are specified in
  `MOZ_LOG` will not appear in `RUST_LOG`.
