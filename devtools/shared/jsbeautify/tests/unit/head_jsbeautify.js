/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var Cr = Components.results;

const { require } = Cu.import("resource://devtools/shared/Loader.jsm", {});

var beautify = require("devtools/shared/jsbeautify/beautify");
var SanityTest = require('devtools/shared/jsbeautify/lib/sanitytest');
var Urlencoded = require('devtools/shared/jsbeautify/lib/urlencode_unpacker');
var {run_beautifier_tests} = require('devtools/shared/jsbeautify/src/beautify-tests');
