#!/bin/sh

cargo run -- \
      ../BinSource.webidl_ \
      ../BinSource.yaml \
      --out-class ../BinASTParser.h    \
      --out-impl ../BinASTParser.cpp   \
      --out-enum ../BinASTEnum.h    \
      --out-token ../BinToken.h

MACH=../../../../mach

${MACH} clang-format --path \
        ../BinASTParser.h \
        ../BinASTParser.cpp \
        ../BinASTEnum.h \
        ../BinToken.h
