#!/bin/sh

cargo run -- \
      ../BinSource.webidl_ \
      ../BinSource.yaml \
      --out-class ../BinSource-auto.h    \
      --out-impl ../BinSource-auto.cpp   \
      --out-enum ../BinASTEnum.h    \
      --out-token ../BinToken.h
