# Platform microbenchmarks

Platform microbenchmarks benchmarks specific low-level operations used
by the gecko platform. If a test regresses, it could result in the
degradation in the performance of some user-visible feature.

The list of tests and their descriptions is currently incomplete. If
something is missing, please search for it in the gecko source and
update this page (or ask the original author to do so, if you're still
not sure).

## String tests

* PerfStripWhitespace 
* PerfCompressWhitespace 
* PerfStripCharsWhitespace 
* PerfStripCRLF 
* PerfStripCharsCRLF

These tests measure the amount of time it takes to perform a large
number of operations on low-level strings.
