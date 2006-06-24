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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 *   Jason Barnabe <jason_barnabe@fastmail.fm>
 *   Shawn Wilsher <me@shawnwilsher.com>
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

/***************************************************************
* Inspector Utils ----------------------------------------------
*  Common functions and constants used across the app.
* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
* REQUIRED IMPORTS:
* chrome://inspector/content/jsutil/xpcom/XPCU.js
* chrome://inspector/content/jsutil/rdf/RDFU.js
****************************************************************/

//////////// global constants ////////////////////

const kInspectorNSURI = "http://www.mozilla.org/inspector#";
const kXULNSURI = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const kHTMLNSURI = "http://www.w3.org/1999/xhtml";
const nsITransactionManager = Components.interfaces.nsITransactionManager;
var gEntityConverter;
const kCharTable = {
  '&': "&amp;",
  '<': "&lt;",
  '>': "&gt;",
  '"': "&quot;"
};

var InsUtil = {
  /******************************************************************************
  * Convenience function for calling nsIChromeRegistry::convertChromeURL 
  *******************************************************************************/
  convertChromeURL: function(aURL)
  {
    var uri = XPCU.getService("@mozilla.org/network/io-service;1", "nsIIOService")
                  .newURI(aURL, null, null);
    var reg = XPCU.getService("@mozilla.org/chrome/chrome-registry;1", "nsIChromeRegistry");
    return reg.convertChromeURL(uri);
  },

  /******************************************************************************
  * Convenience function for getting a literal value from nsIRDFDataSource::GetTarget
  *******************************************************************************/
  getDSProperty: function(/*nsISupports*/ aDS, /*String*/ aId, /*String*/ aPropName) 
  {
    var ruleRes = gRDF.GetResource(aId);
    var ds = XPCU.QI(aDS, "nsIRDFDataSource"); // just to be sure
    var propRes = ds.GetTarget(ruleRes, gRDF.GetResource(kInspectorNSURI+aPropName), true);
    propRes = XPCU.QI(propRes, "nsIRDFLiteral");
    return propRes.Value;
  },
  
  /******************************************************************************
  * Convenience function for persisting an element's persisted attributes.
  *******************************************************************************/
  persistAll: function(aId)
  {
    var el = document.getElementById(aId);
    if (el) {
      var attrs = el.getAttribute("persist").split(" ");
      for (var i = 0; i < attrs.length; ++i) {
        document.persist(aId, attrs[i]);
      }
    }
  },

  /******************************************************************************
  * Convenience function for escaping HTML strings.
  *******************************************************************************/
  unicodeToEntity: function(text)
  {
    const entityVersion = Components.interfaces.nsIEntityConverter.entityW3C;

    function charTableLookup(letter) {
      return kCharTable[letter];
    }

    function convertEntity(letter) {
      try {
        return gEntityConverter.ConvertToEntity(letter, entityVersion);
      } catch (ex) {
        return letter;
      }
    }

    if (!gEntityConverter) {
      try {
        gEntityConverter =
          Components.classes["@mozilla.org/intl/entityconverter;1"]
                    .createInstance(Components.interfaces.nsIEntityConverter);
      } catch (ex) { }
    }

    // replace chars in our charTable
    text = text.replace(/[<>&"]/g, charTableLookup);

    // replace chars > 0x7f via nsIEntityConverter
    text = text.replace(/[^\0-\u007f]/g, convertEntity);

    return text;
  }
};

// ::::::: debugging utilities :::::: 

function debug(aText)
{
  // XX comment out to reduce noise 
  consoleDump(aText);
  //dump(aText);
}

// dump text to the Error Console
function consoleDump(aText)
{
  var csClass = Components.classes['@mozilla.org/consoleservice;1'];
  var cs = csClass.getService(Components.interfaces.nsIConsoleService);
  cs.logStringMessage(aText);
}

function dumpDOM(aNode, aIndent)
{
  if (!aIndent) aIndent = "";
  debug(aIndent + "<" + aNode.localName + "\n");
  var attrs = aNode.attributes;
  var attr;
  for (var a = 0; a < attrs.length; ++a) { 
    attr = attrs[a];
    debug(aIndent + "  " + attr.nodeName + "=\"" + attr.nodeValue + "\"\n");
  }
  debug(aIndent + ">\n");

  aIndent += "  ";
  
  for (var i = 0; i < aNode.childNodes.length; ++i)
    dumpDOM(aNode.childNodes[i], aIndent);
}

var gStringBundle;

// convert nodeType constant into human readable form
function nodeTypeToText (nodeType)
{
  if (!gStringBundle) {
    var strBundleService =
       Components.classes["@mozilla.org/intl/stringbundle;1"].
                  getService(Components.interfaces.nsIStringBundleService);
    gStringBundle = strBundleService.createBundle("chrome://inspector/locale/inspector.properties"); 
  }

  if (gStringBundle) {
    const nsIDOMNode = Components.interfaces.nsIDOMNode;
    if (nodeType >= nsIDOMNode.ELEMENT_NODE && nodeType <= nsIDOMNode.NOTATION_NODE) {
      return gStringBundle.GetStringFromName(nodeType);
    }
  }

  return nodeType;
}

// ::::::: nsITransaction helper functions :::::::

function txnQueryInterface(theUID, theResult)
{
  const nsITransaction = Components.interfaces.nsITransaction;
  const nsISupports    = Components.interfaces.nsISupports;
  if (theUID == nsITransaction || theUID == nsISupports) {
    return this;
  }
  return null;
}

function txnMerge()
{
  return false;
}

function txnRedoTransaction() {
  this.doTransaction();
}

/**
 * A nsITransaction for clipboard copy.
 * @param an array of objects that define a clipboard flavor, a delimiter, and
 *   toString().
 */
function cmdEditCopy(aObjects) {
  // remove this line for bug 179621, Phase Three
  this.txnType = "standard";

  // required for nsITransaction
  this.QueryInterface = txnQueryInterface;
  this.merge = txnMerge;
  this.isTransient = true;

  this.objects = aObjects;
}

cmdEditCopy.prototype.doTransaction = 
function doTransaction() {
  if (this.objects.length == 1)
    viewer.pane.panelset.setClipboardData(this.objects[0],
                                          this.objects[0].flavor,
                                          this.objects.toString());
  else
    viewer.pane.panelset.setClipboardData(this.objects,
                                          this.objects[0].flavor + "s",
                                          this.objects.join(this.objects[0].delimiter));
}


/**
 * Represents a CSS declaration.
 * @param aProperty the property of the declaration
 * @param aValue the value of the declaration
 */
function CSSDeclaration(aProperty, aValue, aImportant) {
  this.flavor = "inspector/css-declaration";
  this.delimiter = "\n";
  this.property = aProperty;
  this.value = aValue;
  this.important = aImportant == true;
}
/**
 * Returns a usable CSS string for the CSSDeclaration.
 * @return a string in the form "property: value;"
 */
CSSDeclaration.prototype.toString = function toString() {
  return this.property + ": " + this.value + (this.important ? " !important" : "") + ";";
}

/**
 * Represents a DOM attribute.
 * @param aNode the attribute node
 */
function DOMAttribute(aNode)
{
  this.flavor = "inspector/dom-attribute";
  this.node = aNode.cloneNode(false);
  this.delimiter = " ";
}
/**
 * Returns a string representing an attribute name/value pair
 * @return a string in the form of 'name="value"'
 */
DOMAttribute.prototype.toString = function toString()
{
  return this.node.nodeName + "=\"" + InsUtil.unicodeToEntity(this.node.nodeValue) + "\"";
};
