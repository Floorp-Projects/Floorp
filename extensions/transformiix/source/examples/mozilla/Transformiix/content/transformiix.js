/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is TransforMiiX XSLT processor.
 * 
 * The Initial Developer of the Original Code is The MITRE Corporation.
 * Portions created by MITRE are Copyright (C) 1999 The MITRE Corporation.
 *
 * Portions created by Peter Van der Beken are Copyright (C) 2000
 * Peter Van der Beken. All Rights Reserved.
 *
 * Contributor(s):
 * Peter Van der Beken, peter.vanderbeken@pandora.be
 *    -- original author.
 *
 */

var xmlLoaded, xslLoaded;
var xmlDocument, xslDocument, resultDocument;
var theXMLURL = "chrome://transformiix/content/simple.xml";
var theXSLURL = "chrome://transformiix/content/simplexsl.xml";

function onLoadTransformiix() 
{
  onTransform();
}

function onTransform() 
{
  var docShellElement = document.getElementById("xml-source");
  var docShell = docShellElement.docShell;
  docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;
  var webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);
  var loadFlags = Components.interfaces.nsIWebNavigation.LOAD_FLAGS_NONE;
  webNav.loadURI(theXMLURL, loadFlags);
  docShellElement = document.getElementById("xsl-source");
  docShell = docShellElement.docShell;
  docShell.viewMode = Components.interfaces.nsIDocShell.viewSource;
  webNav = docShell.QueryInterface(Components.interfaces.nsIWebNavigation);
  webNav.loadURI(theXSLURL, loadFlags);
  docShellElement = document.getElementById("result-doc");
  resultDocument = webNav.document;
  xmlDocument = resultDocument.implementation.createDocument("", "", null);
  xmlDocument.addEventListener("load", xmlDocumentLoaded, false);
  xmlDocument.load(theXMLURL, "text/xml");
  xslDocument = resultDocument.implementation.createDocument("", "", null);
  xslDocument.addEventListener("load", xslDocumentLoaded, false);
  xslDocument.load(theXSLURL, "text/xml");
}

function xmlDocumentLoaded(e) {
	xmlLoaded = true;
	tryToTransform();
}

function xslDocumentLoaded(e) {
	xslLoaded = true;
	tryToTransform();
}

function tryToTransform() {
  if (xmlLoaded && xslLoaded) {
    try {
      var xsltProcessor = null;
      var xmlDocumentNode = xmlDocument.documentElement;
      var xslDocumentNode = xslDocument.documentElement;
      
      xsltProcessor = Components.classes["@mozilla.org/document-transformer;1?type=text/xsl"].getService();
      if (xsltProcessor)	xsltProcessor = xsltProcessor.QueryInterface(Components.interfaces.nsIDocumentTransformer);
    }
    catch (ex) {
      dump("failed to get transformiix service!\n");
      xsltProcessor = null;
    }
  dump("Mal sehen, "+xsltProcessor+"\n");
  var outDocument = resultDocument.implementation.createDocument("", "", null);
  xsltProcessor.TransformDocument(xmlDocumentNode, xslDocumentNode, outDocument, null);
  // DumpDOM(outDocument.documentElement);
  // DumpDOM(xmlDocument.documentElement);
  }
}
