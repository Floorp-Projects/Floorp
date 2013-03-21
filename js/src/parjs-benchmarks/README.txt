# Parallel JS Benchmarks

This is a preliminary benchmark suite for Parallel JS.  Each test is
intended to test various aspects of the engine, as described below.
The set of tests is very preliminary and is expected to grow and
eventually include more real-world examples.

To run the tests, do something like:

    parjs-benchmarks/run.sh build-opt/js parjs-benchmarks/*.js

Where `build-opt/js` is the path to your shell.

# The tests

- Mandelbrot: A test of embarassingly parallel arithmetic.  Exercises
  the comprehension form of 2D parallel arrays.
- Allocator: A test of parallel allocation.
