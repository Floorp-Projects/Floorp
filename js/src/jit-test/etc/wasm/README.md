# Wasm Spec Tests

This directory contains scripts and configs to manage the in-tree import of the
wasm spec testsuite.

## Format of tests

Spec tests are given in `test/core` of the `spec` repository as `.wast` files.
A `.wast` file is a superset of the `.wat` format with commands for running
tests.

The spec interpreter can natively consume `.wast` files to run the tests, or
generate `.js` files which rely on the WebAssembly JS-API plus a harness file
to implement unit-test functionality.

We rely on the spec interpreter to generate `.js` files for us to run.

## Running tests

Tests are imported to `jit-test` and `wpt` to be run in both the JS shell or the
browser.

To run under `jit-test`:
```bash
cd js/src
./jit-test.py path_to_build/dist/bin/js wasm/spec/
```

To run under `wpt` (generally not necessary):
```bash
./mach web-platform-test testing/web-platform/mozilla/tests/wasm/
```

## Test importing

There are many proposals in addition to the canonical spec. Each proposal is a
fork of the canonical spec and may modify any amount of files.

This causes a challenge for any engine implementing multiple proposals, as any
proposal may conflict (and often will) with each other. This makes it
infeasible to merge all spec and proposal repos together.

Testing each proposal separately in full isn't an attractive option either, as
most tests are unchanged and that would cause a significant amount of wasted
computation time.

For this reason, we use a tool [1] to generate a set of separate test-suites
that are 'pruned' to obtain a minimal set of tests. The tool works by merging
each proposal with the proposal it is based off of and removing tests that
have not changed.

[1] https://github.com/eqrion/wasm-generate-testsuite

### Configuration

The import tool relies on `config.toml` for the list of proposals to import,
and `config-lock.toml` for a list of commits to use for each proposal.

The lock file makes test importing deterministic and controllable. This is
useful as proposals often make inconvenient and breaking changes.

### Operation

```bash
# Add, remove, or modify proposals
vim config.toml
# Remove locks for any proposals you wish to pull the latest changes on
vim config-lock.toml
# Import the tests
make update
# View the tests that were imported
hg stat
# Run the imported tests and note failures
./jit-test.py dist/bin/js wasm/spec/
# Exclude test failures
vim config.toml
# Re-import the tests to exclude failing tests
make update
# Commit the changes
hg commit
```

### Debugging test failures

Imported tests use the binary format, which is inconvenient for understanding
why a test is failing.

Luckily, each assertion in an imported test contains the line of the source file
that it corresponds with.

Unfortunately, the '.wast' files are not commited in the tree and so you must
use the import tool to get the original source.

Follow the steps in 'Operation' to get a `wasm-generate-testsuite` repo with
the '.wast' files of each proposal. All proposals are stored in a single git
repo named `specs/`. Each proposal is a branch, and you can find all tests
under `test/core`.

### Debugging import failures

Proposals can often have conflicts with their upstream proposals. This is okay,
and the test importer will fallback to building tests on an unmerged tree.

This will likely result in extra tests being imported due to spurious
differences between the proposal and upstream, but generally is okay.

It's also possible that a proposal is 'broken' and fails to generate '.js' test
files with the spec interpreter. The best way to debug this is to check out the
proposal repo and debug why `./test/build.py` is failing.

The import tool uses `RUST_LOG` to output debug information. `Makefile`
automatically uses `RUST_LOG=info`. Change that to `RUST_LOG=debug` to get
verbose output of all the commands run.
