# BinAST encoded jit-test

Files inside `lazy` and `nonlazy` directories are encoded from the
corresponding plain JS files.

`lazy` directory contains BinAST files that mark every functions "lazy",
so that those functions are compiled lazily.

`nonlazy` directory contains BinST files that mark every functions "eager",
so that those functions are compiled eagerly.

See js/src/jsapi-tests/binast/README.md for more details how to encode or
update them.


# Run BinAST encoded jit-test

By default, BinAST encoded jit-test files are skipped for automation
reason (to avoid consuming much time on non linux64 env)

Passing `--run-binast` parameter to `jit_test.py` enables them.

```
./jit_test.py {PATH_TO_SHELL} --run-binast
```

To run specific BinAST encoded files, specify the path to the BinAST files
just like plain JS file's case.

```
./jit_test.py {PATH_TO_SHELL} --run-binast  {PATH_TO_BINAST_FILES}
```


# Run BinAST encoded jit-test on try

On automation, BinAST encoded jit-test files are executed only on the
following jobs:

 - linux64 opt SM(p)
 - linux64 debug SM(p)
 - linux64 debug SM(cgc)

For more details, see `conditional-env` property in
js/src/devtools/automation/variants/{plain,plaindebug,compacting} files.
