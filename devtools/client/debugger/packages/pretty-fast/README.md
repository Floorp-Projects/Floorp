# Pretty Fast

Pretty Fast is a source-map-generating JavaScript pretty printer, that is pretty
fast.

[![Build Status](https://travis-ci.org/mozilla/pretty-fast.png?branch=master)](https://travis-ci.org/mozilla/pretty-fast)

## Install

    npm install pretty-fast

## Usage

    var prettyFast = require("pretty-fast");

    var uglyJS = "function ugly(){code()}";

    var pretty = prettyFast(uglyJS, {
      url: "test.js",
      indent: "  "
    });

    console.log(pretty.code);
    // function ugly() {
    //   code()
    // }

    console.log(pretty.map);
    // [object SourceMapGenerator]

(See the [mozilla/source-map][0] library for information on SourceMapGenerator
instances, and source maps in general.)

[0]: https://github.com/mozilla/source-map

## Options

* `url` - The URL of the JavaScript source being prettified. Used in the
  generated source map. If you are prettifying JS that isn't from a file or
  doesn't have a URL, you can use a dummy value, such as "(anonymous)".

* `indent` - The string that you want your code indented by. Most people want
  one of `"  "`, `"    "`, or `"\t"`.

* `ecmaVersion` - Indicates the ECMAScript version to parse. 
   See acorn.parse documentation for more details. Defaults to `"latest"`.

## Issues

[Please use Bugzilla](https://bugzilla.mozilla.org/enter_bug.cgi?product=Firefox&component=Developer%20Tools%3A%20Debugger)

## Goals

* To be pretty fast, while still generating source maps.

* To avoid fully parsing the source text; we should be able to get away with
  only a tokenizer and some heuristics.

* Preserve comments.

* Pretty Fast should be able to run inside Web Workers.

## Non-goals

* Extreme configurability of types of indentation, where curly braces go, etc.

* To be the very fastest pretty printer in the universe. This goal is
  unattainable given that generating source maps is a requirement. We just need
  to be Pretty Fast.

* To perfectly pretty print *exactly* as you would have written the code. This
  falls out from both not wanting to support extreme configurability, and
  avoiding full on parsing. We just aim to pretty print Pretty Well.
