# WebGPU CTS vendor checkout

This directory contains the following:

```sh
.
├── README.md # You are here!
├── arguments.txt # Used by `vendor/`
├── checkout/ # Our vendored copy of WebGPU CTS
├── myexpectations.txt # Used by `vendor/`
└── vendor/ # Rust binary crate for updating `checkout/` and generating WPT tests
```

## Re-vendoring

You can re-vendor by running the Rust binary crate from its Cargo project root. Change your working
directory to `vendor/` and invoke `cargo run -- --help` for more details.
