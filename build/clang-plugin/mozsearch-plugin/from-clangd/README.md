The facilities in this subdirectory are copied over from clangd
(https://clangd.llvm.org/).

The files here are currently copies of the following upstream files:
https://github.com/llvm/llvm-project/blob/48bc71505e03694caac6afb2431ff1157a2382a8/clang-tools-extra/clangd/HeuristicResolver.h
https://github.com/llvm/llvm-project/blob/48bc71505e03694caac6afb2431ff1157a2382a8/clang-tools-extra/clangd/HeuristicResolver.cpp

If, in the future, these facilities are moved from clangd to
to libclangTooling and exposed in the clang API headers, we can
switch to consuming them from there directly.
