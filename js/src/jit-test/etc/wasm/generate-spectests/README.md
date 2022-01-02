# generate-spectests

A tool to generate a combined testsuite for the wasm spec and all proposals.

This tool tries to be as robust as possible to deal with upstream breakage, while still generating a minimal testsuite for proposals that don't change many tests.

## Usage

```bash
# Configure the tests you want
vim config.toml

# Generate the tests
# This will create a `repos/` and `tests/` in your working directory
cargo run
```

## config.toml

```toml
# (optional) Text to add to a 'directives.txt' file put in 'js/${repo}/harness'
harness_directive = ""

# (optional) Text to add to a 'directives.txt' file put in 'js/${repo}'
directive = ""

# (optional) Tests to include even if they haven't changed with respect to their parent repository
included_tests = ["test.wast"]

# (optional) Tests to exclude
excluded_tests = ["test.wast"]

[[repos]]
# Name of the repository
name = "sign-extension-ops"

# Url of the repository
url = "https://github.com/WebAssembly/sign-extension-ops"

# (optional) Name of the repository that is the upstream for this repository.
# This repository will attempt to merge with this upstream when generating
# tests. The parent repository must be specified before this repository in the
# 'config.toml'. If you change this, you must delete the 'repos' directory
# before generating tests again.
parent = "spec"

# (optional) Whether to skip merging with upstream, if it exists.
skip_merge = "false"

# (optional) The commit to checkout when generating tests. If not specified,
# defaults to the latest 'origin/master'.
commit = "df34ea92"

# (optional) Text to add to a 'directives.txt' file put in 'js/{$repo}'
directive = ""

# (optional) Tests to include even if they haven't changed with respect to their parent repository
included_tests = ["test.wast"]

# (optional) Tests to exclude
excluded_tests = ["test.wast"]
```
