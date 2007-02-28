/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

dtdTests = ["attrgetownerelement01", "documentimportnode03", 
            "documentimportnode04", "documentimportnode19",
            "documentimportnode20", "documentimportnode21",
            "documentimportnode22", "documenttypeinternalSubset01", 
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
attrAppendChild = ["elementsetattributenodens06", "importNode01"];
removeNamedItemNS = ["namednodemapremovenameditemns06",
                     "namednodemapremovenameditemns07",
                     "namednodemapremovenameditemns08",
                     "removeNamedItemNS02"];
bogusPrefix = ["nodesetprefix05", "nodesetprefix09", "prefix06", "prefix07"];
prefixReplacement = ["setAttributeNodeNS04"];

var todoTests = {};
var exclusions = concat(dtdTests, bug371552, wrongDocError, attrAppendChild,
                        removeNamedItemNS, bogusPrefix, prefixReplacement);
for (var excludedTestName in exclusions) { 
  todoTests[exclusions[excludedTestName]] = true; 
}
