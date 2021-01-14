swgl
========

Software OpenGL implementation for WebRender

Overview
--------
This is a relatively simple single threaded software rasterizer designed
for use by WebRender. It will shade one quad at a time using a 4xf32 vector
with one vertex per lane. It rasterizes quads usings spans and shades that
span 4 pixels at a time.

Building
--------
clang-cl is required to build on Windows. This can be done by installing
the llvm binaries from https://releases.llvm.org/ and adding the installation
to the path with something like `set PATH=%PATH%;C:\Program Files\LLVM\bin`.
Then `set CC=clang-cl` and `set CXX=clang-cl`. That should be sufficient
for `cc-rs` to use `clang-cl` instead of `cl`.
