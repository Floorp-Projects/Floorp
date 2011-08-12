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


// tests that depend on DTD features no one cares about
var dtdTests = ["attrdefaultvalue","attrnotspecifiedvalue", "attrremovechild1",
                "attrreplacechild1", "attrsetvaluenomodificationallowederr",
                "attrsetvaluenomodificationallowederrEE", "attrspecifiedvalueremove",
                "characterdataappenddatanomodificationallowederr", "characterdataappenddatanomodificationallowederrEE",
                "characterdatadeletedatanomodificationallowederr", "characterdatadeletedatanomodificationallowederrEE",
                "characterdatainsertdatanomodificationallowederr", "characterdatainsertdatanomodificationallowederrEE",
                "characterdatareplacedatanomodificationallowederr", "characterdatareplacedatanomodificationallowederrEE",
                "characterdatasetdatanomodificationallowederr", "characterdatasetdatanomodificationallowederrEE",
                "documentcreateelementdefaultattr", "documentcreateentityreference", "documentcreateentityreferenceknown",
                "documenttypegetentities", "documenttypegetentitieslength", "documenttypegetentitiestype",
                "documenttypegetnotations", "documenttypegetnotationstype", "elementremoveattribute",
                "elementremoveattributenodenomodificationallowederr", "elementremoveattributenodenomodificationallowederrEE",
                "elementremoveattributenomodificationallowederr", "elementremoveattributenomodificationallowederrEE",
                "elementremoveattributerestoredefaultvalue", "elementretrieveallattributes",
                "elementsetattributenodenomodificationallowederr", "elementsetattributenodenomodificationallowederrEE",
                "elementsetattributenomodificationallowederr", "elementsetattributenomodificationallowederrEE",
                "entitygetentityname", "entitygetpublicid", "entitygetpublicidnull", "namednodemapremovenameditem",
                "namednodemapremovenameditemgetvalue", "nodeappendchildnomodificationallowederr", "nodeappendchildnomodificationallowederrEE",
                "nodeentitynodeattributes", "nodeentitynodename", "nodeentitynodetype", "nodeentitynodevalue",
                "nodeentityreferencenodeattributes", "nodeentityreferencenodename", "nodeentityreferencenodetype",
                "nodeentityreferencenodevalue", "nodeentitysetnodevalue", "nodeinsertbeforenomodificationallowederr",
                "nodeinsertbeforenomodificationallowederrEE", "nodenotationnodeattributes", "nodenotationnodename",
                "nodenotationnodetype", "nodenotationnodevalue", "noderemovechildnomodificationallowederr",
                "noderemovechildnomodificationallowederrEE", "nodereplacechildnomodificationallowederr",
                "nodereplacechildnomodificationallowederrEE", "nodesetnodevaluenomodificationallowederr",
                "nodesetnodevaluenomodificationallowederrEE", "nodevalue03","nodevalue07", "nodevalue08",
                "notationgetnotationname", "notationgetpublicid", "notationgetpublicidnull", "notationgetsystemid",
                "notationgetsystemidnull", "processinginstructionsetdatanomodificationallowederr",
                "processinginstructionsetdatanomodificationallowederrEE", "textsplittextnomodificationallowederr",
                "textsplittextnomodificationallowederrEE"];

// we don't pass these, unfortunately
var indexErrTests = ["characterdataindexsizeerrdeletedatacountnegative", "characterdataindexsizeerrreplacedatacountnegative",
                     "characterdataindexsizeerrsubstringcountnegative", "hc_characterdataindexsizeerrdeletedatacountnegative",
                     "hc_characterdataindexsizeerrreplacedatacountnegative", "hc_characterdataindexsizeerrsubstringcountnegative"];

var attributeModTests = ["hc_attrappendchild1", "hc_attrappendchild3", "hc_attrappendchild5",
                         "hc_attrappendchild6", "hc_attrchildnodes2", "hc_attrclonenode1", "hc_attrinsertbefore1",
                         "hc_attrinsertbefore2", "hc_attrinsertbefore3", "hc_attrinsertbefore4", "hc_attrinsertbefore6",
                         "hc_attrnormalize", "hc_attrremovechild2", "hc_attrreplacechild1", "hc_attrreplacechild2",
                         "hc_attrsetvalue2", "hc_elementnormalize2", "hc_elementnotfounderr", "hc_elementremoveattribute", "hc_elementnormalize2",
                         "hc_elementnotfounderr", "hc_elementremoveattribute", ];
var modTests = ["hc_elementwrongdocumenterr", "hc_namednodemapwrongdocumenterr", "hc_nodeappendchildnewchilddiffdocument", "hc_nodeinsertbeforenewchilddiffdocument",
                "hc_nodereplacechildnewchilddiffdocument", "hc_elementwrongdocumenterr", "hc_namednodemapwrongdocumenterr", "hc_nodeappendchildnewchilddiffdocument",
                "hc_nodeinsertbeforenewchilddiffdocument", "hc_nodereplacechildnewchilddiffdocument", "elementwrongdocumenterr", "namednodemapwrongdocumenterr",
                "nodeappendchildnewchilddiffdocument", "nodeinsertbeforenewchilddiffdocument", "nodereplacechildnewchilddiffdocument"];
// These tests rely on an implementation of document.createEntityReference.
var createEntityRef = ["documentinvalidcharacterexceptioncreateentref",
                       "documentinvalidcharacterexceptioncreateentref1",
                       "hc_attrgetvalue2", "hc_nodevalue03"];


var todoTests = {};
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
var exclusions = concat(dtdTests, indexErrTests, attributeModTests, modTests, createEntityRef);
for (var excludedTestName in exclusions) { todoTests[exclusions[excludedTestName]] = true; }
