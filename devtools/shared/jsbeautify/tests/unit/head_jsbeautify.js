/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;
const Cr = Components.results;

const { require } = Cu.import("resource://gre/modules/devtools/Loader.jsm", {});

var beautify = require("devtools/jsbeautify");
var SanityTest = require('devtools/toolkit/jsbeautify/sanitytest');
var Urlencoded = require('devtools/toolkit/jsbeautify/urlencode_unpacker');
var {run_beautifier_tests} = require('devtools/toolkit/jsbeautify/beautify-tests');
