/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const I                    = Components.interfaces;
const C                    = Components.classes;

const nsILocalFile         = I.nsILocalFile;
const nsIProperties        = I.nsIProperties;
const nsIFileInputStream   = I.nsIFileInputStream;
const nsIInputStream       = I.nsIInputStream;

const nsIDOMParser         = I.nsIDOMParser;
const nsIDOMSerializer     = I.nsIDOMSerializer;
const nsIDOMDocument       = I.nsIDOMDocument;
const nsIDOMElement        = I.nsIDOMElement;
const nsIDOMNode           = I.nsIDOMNode;
const nsIDOMCharacterData  = I.nsIDOMCharacterData;
const nsIDOMAttr           = I.nsIDOMAttr;
const nsIDOMNodeList       = I.nsIDOMNodeList;
const nsIDOMXULElement     = I.nsIDOMXULElement;
const nsIDOMProcessingInstruction = I.nsIDOMProcessingInstruction;

function DOMParser() {
  var parser = C["@mozilla.org/xmlextras/domparser;1"].createInstance(nsIDOMParser);
  parser.init();
  return parser;
}

var __testsDirectory = null;

function ParseFile(file) {
  if (typeof(file) == "string") {
    if (!__testsDirectory) {
      __testsDirectory = do_get_cwd();
    }
    var fileObj = __testsDirectory.clone();
    fileObj.append(file);
    file = fileObj;
  }

  do_check_eq(file instanceof nsILocalFile, true);

  var fileStr = C["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(nsIFileInputStream);
  // Init for readonly reading
  fileStr.init(file,  0x01, 0400, nsIFileInputStream.CLOSE_ON_EOF);
  return ParseXML(fileStr);
}

function ParseXML(data) {
  if (typeof(data) == "string") {
    return DOMParser().parseFromString(data, "application/xml");
  }

  do_check_eq(data instanceof nsIInputStream, true);
  
  return DOMParser().parseFromStream(data, "UTF-8", data.available(),
                                     "application/xml");
}

function DOMSerializer() {
  return C["@mozilla.org/xmlextras/xmlserializer;1"]
          .createInstance(nsIDOMSerializer);
}

function SerializeXML(node) {
  return DOMSerializer().serializeToString(node);
}

function roundtrip(obj) {
  if (typeof(obj) == "string") {
    return SerializeXML(ParseXML(obj));
  }

  do_check_eq(obj instanceof nsIDOMNode, true);
  return ParseXML(SerializeXML(obj));
}

function do_compare_attrs(e1, e2) {
  const xmlns = "http://www.w3.org/2000/xmlns/";

  var a1 = e1.attributes;
  var a2 = e2.attributes;
  for (var i = 0; i < a1.length; ++i) {
    var att = a1.item(i);
    // Don't test for namespace decls, since those can just sorta be
    // scattered about
    if (att.namespaceURI != xmlns) {
      var att2 = a2.getNamedItemNS(att.namespaceURI, att.localName);
      if (!att2) {
        do_throw("Missing attribute with namespaceURI '" + att.namespaceURI +
                 "' and localName '" + att.localName + "'");
      }
      do_check_eq(att.QueryInterface(nsIDOMAttr).value, 
                  att2.QueryInterface(nsIDOMAttr).value);
    }
  }
}

function do_check_equiv(dom1, dom2) {
  do_check_eq(dom1.nodeType, dom2.nodeType);
  // There's no classinfo around, so we'll need to do some QIing to
  // make sure the right interfaces are flattened as needed.
  switch (dom1.nodeType) {
  case nsIDOMNode.PROCESSING_INSTRUCTION_NODE:
    do_check_eq(dom1.QueryInterface(nsIDOMProcessingInstruction).target, 
                dom2.QueryInterface(nsIDOMProcessingInstruction).target);
    do_check_eq(dom1.data, dom2.data);
  case nsIDOMNode.TEXT_NODE:
  case nsIDOMNode.CDATA_SECTION_NODE:
  case nsIDOMNode.COMMENT_NODE:
    do_check_eq(dom1.QueryInterface(nsIDOMCharacterData).data,
                dom2.QueryInterface(nsIDOMCharacterData).data);
    break;
  case nsIDOMNode.ELEMENT_NODE:
    do_check_eq(dom1.namespaceURI, dom2.namespaceURI);
    do_check_eq(dom1.localName, dom2.localName);
    // Compare attrs in both directions -- do_compare_attrs does a
    // subset check.
    do_compare_attrs(dom1, dom2);
    do_compare_attrs(dom2, dom1);
    // Fall through
  case nsIDOMNode.DOCUMENT_NODE:
    do_check_eq(dom1.childNodes.length, dom2.childNodes.length);
    for (var i = 0; i < dom1.childNodes.length; ++i) {
      do_check_equiv(dom1.childNodes.item(i), dom2.childNodes.item(i));
    }
    break;
  }
}

function do_check_serialize(dom) {
  do_check_equiv(dom, roundtrip(dom));
}

function Pipe() {
  var p = C["@mozilla.org/pipe;1"].createInstance(I.nsIPipe);
  p.init(false, false, 0, 0xffffffff, null);
  return p;
}

function ScriptableInput(arg) {
  if (arg instanceof I.nsIPipe) {
    arg = arg.inputStream;
  }

  var str = C["@mozilla.org/scriptableinputstream;1"].
    createInstance(I.nsIScriptableInputStream);

  str.init(arg);

  return str;
}
