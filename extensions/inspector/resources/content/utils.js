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

var InsUtil = {
  /******************************************************************************
  * Convenience function for calling nsIChromeRegistry::convertChromeURL 
  *******************************************************************************/
  convertChromeURL: function(aURL)
  {
    var uri = XPCU.getService("@mozilla.org/network/simple-uri;1", "nsIURI");
    uri.spec = aURL;
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
  }

};

// ::::::: debugging utilities :::::: 

function debug(aText)
{
  // XX comment out to reduce noise
  //consoleDump(aText);
  dump(aText);
}

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

