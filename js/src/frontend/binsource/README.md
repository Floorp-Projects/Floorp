A parser generator used to generate the following files:

- js/src/frontend/BinSource-auto.h
- js/src/frontend/BinSource-auto.cpp
- js/src/frontent/BinToken.h

from the following files:

- js/src/frontend/BinSource.webidl_ (specifications of BinAST)
- js/src/frontend/BinSource.yaml (parser generator driver)

To use it:
```sh
$ cd $(topsrcdir)/js/src/frontend/binsource
% cargo run -- $(topsrcdir)/js/src/frontend/BinSource.webidl_ $(topsrcdir)/js/src/frontend/BinSource.yaml \
      --out-class $(topsrcdir)/js/src/frontend/BinSource-auto.h    \
      --out-impl $(topsrcdir)/js/src/frontend/BinSource-auto.cpp   \
      --out-token $(topsrcdir)/js/src/frontend/BinToken.h
```
