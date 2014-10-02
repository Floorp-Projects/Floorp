/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dtdTests = ["attrgetownerelement01", "documentimportnode03", 
            "documentimportnode04", "documentimportnode19",
            "documentimportnode20", "documentimportnode21",
            "documentimportnode22",
            "elementgetattributenodens03", "elementgetattributens02",
            "elementhasattribute02", "getAttributeNS01", "getElementById01",
            "getNamedItemNS03", "getNamedItemNS04", "hasAttribute02",
            "hasAttributeNS04", "importNode07", "importNode09",
            "importNode10", "importNode11", "importNode12", "importNode13",
            "internalSubset01", "localName02", "namednodemapgetnameditemns01",
            "namednodemapremovenameditemns02",
            "namednodemapremovenameditemns05", "namednodemapsetnameditemns05",
            "namednodemapsetnameditemns09", "namednodemapsetnameditemns10",
            "namednodemapsetnameditemns11", "namespaceURI01", 
            "nodeissupported04", "nodenormalize01", "nodesetprefix04",
            "prefix08", "removeAttributeNS01", "removeAttributeNS02",
            "removeNamedItemNS03", "setAttributeNodeNS02", "setAttributeNS03",
            "setNamedItemNS04"];

bug371552 = ["elementhasattributens02"];
wrongDocError = ["elementsetattributenodens05", "namednodemapsetnameditemns03",
                 "setAttributeNodeNS05", "setNamedItemNS02"];
attrExodus = ["elementsetattributenodens06", "importNode01",
              "hc_namednodemapinvalidtype1", "nodehasattributes02"];
bogusPrefix = ["nodesetprefix05", "nodesetprefix09", "prefix06", "prefix07"];
prefixReplacement = ["setAttributeNodeNS04"];

function concat(lst/*...*/) {
  var f = [];
  if (arguments !== null) {
    f = arguments[0];
  }
  for (var i = 1; i < arguments.length; i++) {
    f = f.concat(arguments[i]);
  }
  return f;
}

var todoTests = {};
var exclusions = concat(dtdTests, bug371552, wrongDocError, attrExodus,
                        bogusPrefix, prefixReplacement);
for (var excludedTestName in exclusions) { 
  todoTests[exclusions[excludedTestName]] = true; 
}
