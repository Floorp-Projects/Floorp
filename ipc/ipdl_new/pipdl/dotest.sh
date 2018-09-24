#!/bin/bash

# Run the parser on every error ipdl file in the passed-in directory.
find $1 -name "*.ipdl" -print0 | sort -z | xargs -0 -n 1 cargo run --release --example try_parse --
echo status $? >&2
