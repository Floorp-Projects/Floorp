# Overview

This directory contains BinAST files for testing BinAST parser, and also
a script to update BinAST test files in entire tree (including files in
other directories).


# Update BinAST files

## Requirement

To update BinAST files, you need to clone `binjs-ref` repository and
build `binjs_encode` command, from the following:

https://github.com/binast/binjs-ref


## Update all files

To update all BinAST files in tree, run the following command:

```
./encode.py \
    --topsrcdir={path to mozilla-unified} \
    --binjsdir={path to binjs-ref clone} \
    --binjs_encode={path to binjs_encode}
```

For example, if `mozilla-unified` and `binjs-ref` are located in
`~/somewhere`:

```
./encode.py \
    --topsrcdir=~/somewhere/mozilla-unified \
    --binjsdir=~/somewhere/binjs-ref \
    --binjs_encode=~/somewhere/binjs-ref/target/debug/binjs_encode
```


## Update specific files


To update specific BinAST files, run the following command,
where "filter" is the substring of the path to the file:

```
./encode.py \
    --topsrcdir={path to mozilla-unified} \
    --binjsdir={path to binjs-ref clone} \
    --binjs_encode={path to binjs_encode} \
    {filter} {filter} ...
```

For example, to update the following files:

 - js/src/jit-test/tests/binast/lazy/regexp_parse/Assertion.binjs
 - js/src/jit-test/tests/binast/nonlazy/regexp_parse/Assertion.binjs
 - js/src/jit-test/tests/binast/lazy/regexp_parse/Atom.binjs
 - js/src/jit-test/tests/binast/nonlazy/regexp_parse/Atom.binjs

Run the following command

```
./encode.py \
    --topsrcdir=~/somewhere/mozilla-unified \
    --binjsdir=~/somewhere/binjs-ref \
    --binjs_encode=~/somewhere/binjs-ref/target/debug/binjs_encode \
    regexp_parse/Assertion.binjs \
    regexp_parse/Atom.binjs
```


# jit-test

BinAST files in jit-test are encoded from the corresponding plain JS files
in js/src/jit-test/tests, recursively.

For example js/src/jit-test/tests/FOO/BAR.js is encoded into the following
2 files:

 - js/src/jit-test/tests/binast/lazy/FOO/BAR.binjs
 - js/src/jit-test/tests/binast/nonlazy/FOO/BAR.binjs

Also, if FOO.js contains `|jit-test|` in the first line, that line is
copied to the following 2 files:

 - js/src/jit-test/tests/binast/lazy/FOO/BAR.dir
 - js/src/jit-test/tests/binast/nonlazy/FOO/BAR.dir

If the directory contains `directives.txt`, the file is copied to the
following files.

 - js/src/jit-test/tests/binast/lazy/FOO/directives.txt
 - js/src/jit-test/tests/binast/nonlazy/FOO/directives.txt


If a test is known to fail in BinAST for some reasons, putting that path in
`jit-test.ignore` file in this directory skips encoding the file.

For more details about the BinAST encoded jit-test, see
js/src/jit-test/tests/binast/README.md file.


# web-platform test

BinAST files in web-platform tests are encoded from the corresponding plain
JS files in the same directory.

Files are directly listed in `convert_wpt` function in `encode.py`.


# jsapi-test

BinAST files in jsapi-test are encoded from the corresponding plain JS
files in the same directory (js/src/jsapi-tests/binast/parser/multipart/),
recursively.
