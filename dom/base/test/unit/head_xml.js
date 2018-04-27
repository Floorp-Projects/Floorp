/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const I                    = Ci;
const C                    = Cc;

const nsIFile         = I.nsIFile;
const nsIProperties        = I.nsIProperties;
const nsIFileInputStream   = I.nsIFileInputStream;
const nsIInputStream       = I.nsIInputStream;

const nsIDOMDocument       = I.nsIDOMDocument;
const nsIDOMNode           = I.nsIDOMNode;

Cu.importGlobalProperties(["DOMParser", "Element", "XMLSerializer"]);

function getParser() {
  var parser = new DOMParser();
  parser.forceEnableXULXBL();
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

  Assert.equal(file instanceof nsIFile, true);

  var fileStr = C["@mozilla.org/network/file-input-stream;1"]
                 .createInstance(nsIFileInputStream);
  // Init for readonly reading
  fileStr.init(file,  0x01, 0o400, nsIFileInputStream.CLOSE_ON_EOF);
  return ParseXML(fileStr);
}

function ParseXML(data) {
  if (typeof(data) == "string") {
    return getParser().parseFromString(data, "application/xml");
  }

  Assert.equal(data instanceof nsIInputStream, true);
  
  return getParser().parseFromStream(data, "UTF-8", data.available(),
                                     "application/xml");
}

function DOMSerializer() {
  return new XMLSerializer();
}

function SerializeXML(node) {
  return DOMSerializer().serializeToString(node);
}

function roundtrip(obj) {
  if (typeof(obj) == "string") {
    return SerializeXML(ParseXML(obj));
  }

  Assert.equal(obj instanceof nsIDOMNode, true);
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
      Assert.equal(att.value, att2.value);
    }
  }
}

function do_check_equiv(dom1, dom2) {
  Assert.equal(dom1.nodeType, dom2.nodeType);
  switch (dom1.nodeType) {
  case nsIDOMNode.PROCESSING_INSTRUCTION_NODE:
    Assert.equal(dom1.target, dom2.target);
    Assert.equal(dom1.data, dom2.data);
  case nsIDOMNode.TEXT_NODE:
  case nsIDOMNode.CDATA_SECTION_NODE:
  case nsIDOMNode.COMMENT_NODE:
    Assert.equal(dom1.data, dom2.data);
    break;
  case nsIDOMNode.ELEMENT_NODE:
    Assert.equal(dom1.namespaceURI, dom2.namespaceURI);
    Assert.equal(dom1.localName, dom2.localName);
    // Compare attrs in both directions -- do_compare_attrs does a
    // subset check.
    do_compare_attrs(dom1, dom2);
    do_compare_attrs(dom2, dom1);
    // Fall through
  case nsIDOMNode.DOCUMENT_NODE:
    Assert.equal(dom1.childNodes.length, dom2.childNodes.length);
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
