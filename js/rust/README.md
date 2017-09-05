# The `js` Crate: Rust Bindings to SpiderMonkey

[User Documentation](http://doc.servo.org/js/)

## Building

To build a release version of SpiderMonkey and the Rust code with optimizations
enabled:

```
$ cargo build --release
```

To build with SpiderMonkey's DEBUG checks and assertions:

```
$ cargo build --features debugmozjs
```

Raw FFI bindings to JSAPI are machine generated with
[`rust-lang-nursery/rust-bindgen`][bindgen], and requires libclang >= 3.9. See
`./build.rs` for details.

[bindgen]: https://github.com/rust-lang-nursery/rust-bindgen

## Cargo Features

* `debugmozjs`: Create a DEBUG build of SpiderMonkey with many extra assertions
  enabled. This is decoupled from whether the crate and its Rust code is built
  in debug or release mode.

* `promises`: Enable SpiderMonkey native promises.

* `nonzero`: Leverage the unstable `NonZero` type. Requires nightly Rust.

## Testing

Make sure to test both with and without the `debugmozjs` feature because various
structures have different sizes and get passed through functions differently at
the ABI level! At minimum, you should test with `debugmozjs` to get extra
assertions and checking.

```
$ cargo test
$ cargo test --features debugmozjs
```
