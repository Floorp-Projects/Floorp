A parser generator used to generate the following files:

- js/src/frontend/BinASTParser.h
- js/src/frontend/BinASTParser.cpp
- js/src/frontent/BinASTToken.h

from the following files:

- js/src/frontend/BinAST.webidl_ (specifications of BinAST)
- js/src/frontend/BinAST.yaml (parser generator driver)

To use it:
```sh
$ cd $(topsrcdir)/js/src/frontend/binast
% ./build.sh
```
