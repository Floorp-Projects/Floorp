/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
                         "hc_attrnormalize", "hc_attrreplacechild1", "hc_attrreplacechild2",
                         "hc_attrsetvalue2", "hc_elementnormalize2", "hc_elementnotfounderr", "hc_elementremoveattribute", "hc_elementnormalize2",
                         "hc_elementnotfounderr", "hc_elementremoveattribute",
                         "hc_attrchildnodes1", "hc_attrfirstchild",
                         "hc_attrhaschildnodes", "hc_attrlastchild",
                         "hc_attrremovechild1", "hc_attrsetvalue1"];
var modTests = ["hc_elementwrongdocumenterr", "hc_namednodemapwrongdocumenterr", "hc_nodeappendchildnewchilddiffdocument", "hc_nodeinsertbeforenewchilddiffdocument",
                "hc_nodereplacechildnewchilddiffdocument", "hc_elementwrongdocumenterr", "hc_namednodemapwrongdocumenterr", "hc_nodeappendchildnewchilddiffdocument",
                "hc_nodeinsertbeforenewchilddiffdocument", "hc_nodereplacechildnewchilddiffdocument", "elementwrongdocumenterr", "namednodemapwrongdocumenterr",
                "nodeappendchildnewchilddiffdocument", "nodeinsertbeforenewchilddiffdocument", "nodereplacechildnewchilddiffdocument"];
// These tests rely on an implementation of document.createEntityReference.
var createEntityRef = ["documentinvalidcharacterexceptioncreateentref",
                       "documentinvalidcharacterexceptioncreateentref1",
                       "hc_attrgetvalue2", "hc_nodevalue03"];
var createProcessingInstructionHTML = ["documentinvalidcharacterexceptioncreatepi",
                                       "documentinvalidcharacterexceptioncreatepi1"];
// These tests expect Node.attributes to exist.
var attributesOnNode = [
  "hc_commentgetcomment",
  "hc_documentgetdoctype",
  "hc_nodeattributenodeattribute",
  "hc_nodecommentnodeattributes",
  "hc_nodecommentnodeattributes",
  "hc_nodedocumentfragmentnodevalue",
  "hc_nodedocumentnodeattribute",
  "hc_nodetextnodeattribute",
  "nodeattributenodeattribute",
  "nodecommentnodeattributes",
  "nodecommentnodeattributes",
  "nodedocumentfragmentnodevalue",
  "nodedocumentnodeattribute",
  "nodeprocessinginstructionnodeattributes",
  "nodetextnodeattribute",
  "nodecdatasectionnodeattribute",
  "nodedocumenttypenodevalue"
]

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
var exclusions = concat(dtdTests, indexErrTests, attributeModTests, modTests, createEntityRef, createProcessingInstructionHTML, attributesOnNode);
for (var excludedTestName in exclusions) { todoTests[exclusions[excludedTestName]] = true; }
