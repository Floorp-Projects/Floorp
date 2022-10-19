# l10nregistry-rs

The `L10nRegistry` is responsible for taking `FileSources` across the app, and turning them into bundles. It is hooked into the `L10nRegistry` global available from privileged JavaScript. See the [L10nRegistry.webidl](https://searchfox.org/mozilla-central/source/dom/chrome-webidl/L10nRegistry.webidl#100) for detailed information about this API, and `intl/l10n/test/test_l10nregistry.js` for integration tests with examples of how it can be used.

## Testing

Tests can be run directly from this directory via:

```
cargo test --all-features
```

Benchmarks are also available. First uncomment the `criterion` dependency in the `Cargo.toml` and then run.

```
cargo test bench --all-features
```
