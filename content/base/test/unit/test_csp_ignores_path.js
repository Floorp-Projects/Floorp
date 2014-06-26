/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/CSPUtils.jsm');

var ioService = Cc["@mozilla.org/network/io-service;1"]
                  .getService(Ci.nsIIOService);
var self = ioService.newURI("http://test1.example.com:80", null, null);

function testValidSRCsHostSourceWithSchemeAndPath() {
  var csps = [
    "http://test1.example.com",
    "http://test1.example.com/",
    "http://test1.example.com/path-1",
    "http://test1.example.com/path-1/",
    "http://test1.example.com/path-1/path_2/",
    "http://test1.example.com/path-1/path_2/file.js",
    "http://test1.example.com/path-1/path_2/file_1.js",
    "http://test1.example.com/path-1/path_2/file-2.js",
    "http://test1.example.com/path-1/path_2/f.js",
    "http://test1.example.com/path-1/path_2/f.oo.js"
  ]

  var obj;
  var expected = "http://test1.example.com";
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    dump("expected: " + expected + "\n");
    dump("got:      " + obj._sources[0] + "\n");
    do_check_eq(1, obj._sources.length);
    do_check_eq(obj._sources[0], expected);
  }
}

function testValidSRCsRegularHost() {
  var csps = [
    "test1.example.com",
    "test1.example.com/",
    "test1.example.com/path-1",
    "test1.example.com/path-1/",
    "test1.example.com/path-1/path_2/",
    "test1.example.com/path-1/path_2/file.js",
    "test1.example.com/path-1/path_2/file_1.js",
    "test1.example.com/path-1/path_2/file-2.js",
    "test1.example.com/path-1/path_2/f.js",
    "test1.example.com/path-1/path_2/f.oo.js"
  ]

  var obj;
  var expected = "http://test1.example.com";
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    do_check_eq(1, obj._sources.length);
    do_check_eq(obj._sources[0], expected);
  }
}

function testValidSRCsWildCardHost() {
  var csps = [
    "*.example.com",
    "*.example.com/",
    "*.example.com/path-1",
    "*.example.com/path-1/",
    "*.example.com/path-1/path_2/",
    "*.example.com/path-1/path_2/file.js",
    "*.example.com/path-1/path_2/file_1.js",
    "*.example.com/path-1/path_2/file-2.js",
    "*.example.com/path-1/path_2/f.js",
    "*.example.com/path-1/path_2/f.oo.js"
  ]

  var obj;
  var expected = "http://*.example.com";
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    do_check_eq(1, obj._sources.length);
    do_check_eq(obj._sources[0], expected);
  }
}

function testValidSRCsRegularPort() {
  var csps = [
    "test1.example.com:80",
    "test1.example.com:80/",
    "test1.example.com:80/path-1",
    "test1.example.com:80/path-1/",
    "test1.example.com:80/path-1/path_2",
    "test1.example.com:80/path-1/path_2/",
    "test1.example.com:80/path-1/path_2/file.js",
    "test1.example.com:80/path-1/path_2/f.ile.js"
  ]

  var obj;
  var expected = "http://test1.example.com";
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    do_check_eq(1, obj._sources.length);
    do_check_eq(obj._sources[0], expected);
  }
}

function testValidSRCsWildCardPort() {
  var csps = [
    "test1.example.com:*",
    "test1.example.com:*/",
    "test1.example.com:*/path-1",
    "test1.example.com:*/path-1/",
    "test1.example.com:*/path-1/path_2",
    "test1.example.com:*/path-1/path_2/",
    "test1.example.com:*/path-1/path_2/file.js",
    "test1.example.com:*/path-1/path_2/f.ile.js"
  ]

  var obj;
  var expected = "http://test1.example.com:*";
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    do_check_eq(1, obj._sources.length);
    do_check_eq(obj._sources[0], expected);
  }
}


function testInvalidSRCs() {
  var csps = [
    "test1.example.com:88path-1/",
    "test1.example.com:80.js",
    "test1.example.com:*.js",
    "test1.example.com:*."
  ]

  var obj;
  var expected = [];
  for (let i in csps) {
    var src = csps[i];
    obj = CSPSourceList.fromString(src, undefined, self);
    do_check_eq(0, obj._sources.length);
  }
}

function run_test() {
  testValidSRCsHostSourceWithSchemeAndPath();
  testValidSRCsRegularHost();
  testValidSRCsWildCardHost();
  testValidSRCsRegularPort();
  testValidSRCsWildCardPort();
  testInvalidSRCs();
  do_test_finished();
}
