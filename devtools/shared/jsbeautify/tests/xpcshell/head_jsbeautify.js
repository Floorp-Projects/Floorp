/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

var beautify = require("devtools/shared/jsbeautify/beautify");
var SanityTest = require('devtools/shared/jsbeautify/lib/sanitytest');
var Urlencoded = require('devtools/shared/jsbeautify/lib/urlencode_unpacker');
var {run_beautifier_tests} = require('devtools/shared/jsbeautify/src/beautify-tests');
