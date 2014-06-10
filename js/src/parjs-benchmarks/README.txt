This directory contains benchmarks for PJS, to be run manually for
the time being.

ray.js is a ray tracer and ray-expected.ppm is its expected output.
  Instructions are in a comment in the program.  Expect runs on
  release builds to take 30+ seconds even in the best case.

  The ray tracer allocates somewhere in the vicinity of 360GB on a
  64-bit system.

The cat-convolve-mapPar-*.js programs are two variations on a simple
  convolver benchmark.  These programs allocate hardly anything, and
  nothing within the PJS parallel section.

  Instructions are in comments in the programs.

  All the convolution tests must be run from a directory containing
  the input file "cat.pgm".  That file is a free photo of an Amur
  leopard taken from morguefile.com on 27 May 2014, URL
  http://mrg.bz/MT0rPL.  It has been scaled down significantly,
  cropped slightly, and converted from jpeg to pgm.

SimPJS-test.js is an allocation-heavy program originating with Intel.
  It runs parallel and sequential versions of the same code and prints
  the times for both.

unhex.c is a utility that is used to process the output of some of
  these tests; the output is represented as hex-encoded byte streams.
  Compile this first if you need to run the programs in non-benchmark
  mode to check the output.
