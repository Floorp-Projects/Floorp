/* -*- Mode: C; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Platform for Privacy Preferences.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Harish Dhurvasula <harishd@netscape.com>
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

const nsIPolicyReference     = Components.interfaces.nsIPolicyReference;
const nsIPolicyTarget        = Components.interfaces.nsIPolicyTarget;
const nsIIOService           = Components.interfaces.nsIIOService;
const nsIPromptService       = Components.interfaces.nsIPromptService;
const nsIURL                 = Components.interfaces.nsIURL;
const nsIStringBundleService = Components.interfaces.nsIStringBundleService;
const nsIHttpChannel         = Components.interfaces.nsIHttpChannel;

const STYLESHEET_URL_200201 = "chrome://communicator/content/p3p/p3p200201.xsl";
const STYLESHEET_URL_200109 = "chrome://communicator/content/p3p/p3p200109.xsl";
const STYLESHEET_URL_200012 = "chrome://communicator/content/p3p/p3p200012.xsl";
const STYLESHEET_URL_200010 = "chrome://communicator/content/p3p/p3p200010.xsl";
const STYLESHEET_URL_200005 = "chrome://communicator/content/p3p/p3p200005.xsl";

const FAILURE       = 0;
const SUCCESS       = 1;

const LOAD_POLICY   = 2;
const LOAD_SUMMARY  = 3;
const LOAD_OPTIONS  = 4;

const POLICY_ERROR  = 5;
const SUMMARY_ERROR = 6;
const OPTIONS_ERROR = 7;

const DO_TRANSFORM  = 8;

var gIOService           = null;
var gPromptService       = null;
var gStrBundleService    = null;
var gStrBundle           = null;
var gBrandName           = null;

/* class nsPolicyViewer
 *
 * Used for viewing site's privacy policy, policy summary ( generated ),
 * and opt-ins/opt-outs
 * @param - aMainURI -> The URI of the loaded document.
 *
 */
function nsPolicyViewer(aDoc)
{
  if (!gIOService) {
    gIOService =
      Components.classes["@mozilla.org/network/io-service;1"].getService(nsIIOService);
  }

  try {
    this.mMainURI = gIOService.newURI(aDoc.location.href, null, null);
  }
  catch(ex) {
    this.mSelectedURI = aDoc.location.href;
    this.reportError(LOAD_POLICY);
  }

}

nsPolicyViewer.prototype =
{
  mAction          : null,
  mPolicyLocation  : null,
  mPolicyReference : null,
  mPolicyTarget    : null,
  mXMLHttpRequest  : null,
  mXSLTProcessor   : null,
  mDocument        : null,
  mPolicyURL       : null,
  mMainURI         : null,
  mSelectedURI     : null,
  mLinkedURI       : null,
  mStyle           : null,
  mFragment        : null,
  mFlag            : 0,

  /*
   * Initiate privacy policy loading for a URI selected
   * in the Page-Info-Privacy tab window.
   * @param - aSelectedURI -> Absolute URI that requests a privacy policy
   * @param - aAction      -> HUMAN READABLE | GENERATE SUMMARY | OPT-IN/OPT-OUT
   */
  load : function (aSelectedURI, aAction)
  {
    // Since P3P operates by definition on HTTP(S) URLs only,
    // bail out here if the URL is not HTTP(S).
    if (aSelectedURI.match(/^\s*https?:/i)) {
      if (this.init(aSelectedURI, aAction) == SUCCESS) {
        this.handleURI(this.mSelectedURI, this.mFlag);
      }
    } else {
      // XXX Error message should reflect that this operation
      // is not supported for this protocol (which is not HTTP(S)).
      this.mSelectedURI = aSelectedURI;
      this.reportError(aAction);
    }
  },

  /*
   * Initialize nsPolicyViewer
   * @param - aSelectedURI -> URI that requests a privacy policy
   * @param - aAction      -> HUMAN READABLE | MACHINE READABLE | OPT-IN/OPT-OUT
   */
  init : function(aSelectedURI, aAction)
  {
    try {
      if (!this.mPolicyReference) {
        this.mPolicyReference =
          Components.classes["@mozilla.org/p3p/policyreference;1"].createInstance(nsIPolicyReference);

        this.mPolicyTarget = this.mPolicyReference.QueryInterface(nsIPolicyTarget);
        this.mPolicyTarget.setupPolicyListener(this);
        this.mPolicyReference.initialize(this.mMainURI);
      }

      this.mAction = aAction;

      // aSelectedURI is already absolute spec
      if (!this.mSelectedURI) {
        this.mSelectedURI = gIOService.newURI(aSelectedURI, null, null);
      }
      else {
        this.mSelectedURI.spec = aSelectedURI;
      }

      // Ex. http://foo.com:80/ == http://foo.com/
      if (isEquivalent(this.mMainURI, this.mSelectedURI)) {
        this.mFlag = nsIPolicyReference.IS_MAIN_URI;
      }
      else {
        this.mFlag = nsIPolicyReference.IS_EMBEDDED_URI;
      }

      return SUCCESS;
    }
    catch (ex) {
      this.reportError(aAction);
      return FAILURE;
    }
  },

  /*
   * This is a callback method. This method gets called
   * when a full privacy policy is loaded or when a style
   * sheet is loaded to begin transformation.
   */
  handleEvent : function(aEvent) {
    switch (this.mAction) {
      case LOAD_POLICY :
        this.viewHumanReadablePolicy(); break;
      case LOAD_SUMMARY :
        this.viewMachineReadablePolicy(); break;
      case LOAD_OPTIONS :
        this.viewOptInOptOutPolicy(); break;
      case DO_TRANSFORM :
        this.transform(); break;
      default:
        reportError(POLICY_ERROR); break;
    }
  },

  /*
   * Initiate policy reference file ( PRF ) loading. A policy reference file
   * is used for locating the full p3p policy location.
   * @param - aURI   -> URI that requests a privacy policy
   * @param - aFlag  -> IS_MAIN_URI     - The loaded document's URI
   *                    IS_EMBEDDED_URI - URI, embedded in the current document, with a different host.
   *                    IS_LINKED_URI   - URI referenced via HTML/XHTML LINK tag.
   */
  handleURI : function (aURI, aFlag)
  {
    try {
      this.mPolicyReference.loadPolicyReferenceFileFor(aURI, aFlag);
    }
    catch(ex) {
      this.reportError(this.mAction);
    }
  },

  /*
   * This is a callback method.
   * @param - aLocation -> A full p3p policy location
   * @param - aError    -> POLICY_LOAD_SUCCESS | POLICY_LOAD_FAILURE
   */
  notifyPolicyLocation : function (aLocation, aError)
  {
    if (aError == nsIPolicyReference.POLICY_LOAD_SUCCESS) {
     if (!this.mPolicyURL) {
       this.mPolicyURL = gIOService.newURI(aLocation, null, null).QueryInterface(nsIURL);
     }
     else {
       this.mPolicyURL.spec = aLocation;
     }

      // We have located the full p3p policy location from
      // the policy reference file. Now let's try to load full p3p
      // policy document.
      if (!this.mXMLHttpRequest) {
        this.mXMLHttpRequest = new XMLHttpRequest();
      }

      // If a fragment identifier is specified then we have to locate
      // the POLICY with a name specified by the fragment.
      // Ex. <POLICY name="one"> .... </POLICY> <POLICY name="two"> ...</POLICY>
      // if the fragment identifier is "#two" then we should find the POLICY named "two"
      this.mFragment = this.mPolicyURL.ref;

      // If we've already loaded a full p3p policy then the policy
      // document should be available ( note: Only for the same host's URIs ).
      if (this.mFlag & nsIPolicyReference.IS_MAIN_URI) {
        if (this.mDocument) {
          if (this.mPolicyLocation.match(aLocation)) {
            this.handleEvent(null);
            return;
          }
          this.mFlag &= ~nsIPolicyReference.IS_MAIN_URI;
        }
        else {
          if (this.mMainURI.spec.match(this.mSelectedURI.spec)) {
            this.mPolicyLocation = aLocation;
          }
          else {
            this.mFlag &= ~nsIPolicyReference.IS_MAIN_URI;
          }
        }
      }

      this.mXMLHttpRequest.onload = this;
      // Override the mime type because without it XMLHttpRequest hangs
      // on certain sites that serve HTML instead ofXML.
      this.mXMLHttpRequest.overrideMimeType("text/xml");
      this.mXMLHttpRequest.open("GET", aLocation);
      this.mXMLHttpRequest.send(null);
    }
    else if (this.mFlag & nsIPolicyReference.IS_MAIN_URI) {
      // Weren't able to locate the full p3p policy via PRF?
      // Let's see if the policy location is referenced via the
      // current document's link tag.
      var links  = gTopDoc.getElementsByTagName("LINK");
      var length = links.length;
      for (index = 0; index < length; index++) {
        var node = links[index];
        if (node.getAttribute("rel") == "P3Pv1") {
          try {
            if (!this.mLinkedURI) {
              this.mLinkedURI = gIOService.newURI(node.href, null, null);
            }
            else {
              this.mLinkedURI.spec = node.href;
            }
            // Yay!, we have found the policy location no need to search the other link tags.
            this.handleURI(this.mLinkedURI, this.mFlag = nsIPolicyReference.IS_LINKED_URI);
            return;
          }
          catch (ex) {
            // fall through to report error.
          }
        }
      }
      this.reportError(this.mAction);
    }
    else {
      this.reportError(this.mAction);
    }
  },

  /*
   * Called on a policy load error.
   * @param - aError -> POLICY_LOAD_SUCCESS | POLICY_LOAD_FAILURE
   */
  notifyPolicyError : function (aError)
  {
    this.reportError(this.mAction);
  },

  /*
   * Signal policy reference ( native code ) that we're done
   * with the policy viewer and therefore all the objects can
   * be released
   */
  finalize: function ()
  {
    if (this.mPolicyReference) {
      this.mPolicyReference.finalize();
    }
  },

  /*
   * This is required to make the wrapper code happy
   */
  QueryInterface: function (iid)
  {
    if (iid.equals(Components.interfaces.nsISupports) ||
        iid.equals(Components.interfaces.nsISupportsWeakReference) ||
        iid.equals(Components.interfaces.nsIDOMEventListener))
      return this;

    Components.returnCode = Components.results.NS_ERROR_NO_INTERFACE;
    return null;
  },

  /*
   * The purpose of this method is to display a human readable
   * version of the site's policy. That is, we load, on a new window,
   * the value of the "discuri" attribute of the POLICY node.
   */
  viewHumanReadablePolicy : function ()
  {
    var document = this.getDocument();
    if (document) {
      // Note: P3P policies can be associated with at least five
      // different namespaces. For performance reasons intentionally
      // accessing element by name without namespace.
      var nodelist = document.getElementsByTagName("POLICY");
      var length   = nodelist.length;
      if (length > 0) {
        for (index = 0; index < length; index++) {
          var node = nodelist[index];
          var name = node.getAttribute("name");

          // If a fragment identifier is specified then we have to locate
          // the POLICY with a name specified by that fragment.
          if (!this.mFragment || this.mFragment == name) {
            var discuri = node.getAttribute("discuri");
            if (discuri) {
              discuri = this.mPolicyURL.resolve(discuri);
              if (discuri.match(/^\s*https?:/i)) {
                window.open(discuri);
                return;
              }
            }
            break;
          }
        }
        this.reportError(POLICY_ERROR);
      }
      else {
        this.reportError(POLICY_ERROR);
      }
    }
  },

  /*
   * The purpose of this method is to generate a summary ( using XSLT )
   * based on the machine readable format ( XML ) of the full p3p policy.
   */
  viewMachineReadablePolicy : function ()
  {
    var document = this.getDocument();
    if (document) {
      // Note: P3P policies can be associated with at least five
      // different namespaces. For performance reasons intentionally
      // accessing element by name without namespace.
      var nodelist = document.getElementsByTagName("POLICY");
      var length   = nodelist.length;
      if (length > 1) {
        var index = 0;
        while (index < length) {
          var node = nodelist[index];
          var name = node.getAttribute("name");

          // If a fragment identifier is specified then target
          // the POLICY with that fragment name. To achieve that
          // remove all other POLICY nodes that don't match the name.
          // By doing this the XSLT code doesn't have to know what
          // part of the full policy needs to be transformed.
          if (this.mFragment && this.mFragment != name) {
            node.parentNode.removeChild(node);
            // When a node gets removed it will be reflected on the nodelist.
            // That is, the nodelist length will decrease. To stay within
				// bounds of the nodelist array do not increment index.
            --length;
          }
          else {
            ++index;
          }
        }
      }

      this.preTransform();
    }
  },

  /*
   * The purpose of this method is to display a human readable
   * version of the site's opt-in / opt-out policy. That is, we
   * load, on a new window, the value of the "opturi" attribute
   * of the POLICY node.
   */
  viewOptInOptOutPolicy : function ()
  {
    var document = this.getDocument();

    if (document) {
      // Note: P3P policies can be associated with at least five
      // different namespaces. For performance reasons intentionally
      // accessing element by name without namespace.
      var nodelist = document.getElementsByTagName("POLICY");
      var length   = nodelist.length;
      if (length > 0) {
        for (index = 0; index < length; index++) {
          var node = nodelist[index];
          var name = node.getAttribute("name");

          // If a fragment identifier is specified then we have to locate
          // the POLICY with a name specified by that fragment.
          if (!this.mFragment || this.mFragment == name) {
            var opturi = node.getAttribute("opturi");
            if (opturi) {
              opturi = this.mPolicyURL.resolve(opturi);
              if (opturi.match(/^\s*https?:/i)) {
                window.open(opturi);
                return;
              }
            }
            break;
          }
        }
        this.reportError(OPTIONS_ERROR);
      }
      else {
        this.reportError(OPTIONS_ERROR);
      }
    }
  },

  /*
   * The purpose of this method is to retrieve a full p3p
   * policy document.
   * @return document or null.
   */
  getDocument : function ()
  {
    var document = null;
    var channel  = null;
    if (this.mFlag & nsIPolicyReference.IS_MAIN_URI) {
      if (!this.mDocument) {
        channel = this.mXMLHttpRequest.channel.QueryInterface(nsIHttpChannel);
        if (channel.requestSucceeded) {
          this.mDocument = this.mXMLHttpRequest.responseXML;
        }
        else {
          this.reportError(this.mAction);
          return null;
        }
      }
      document = this.mDocument;
    }
    else {
      channel = this.mXMLHttpRequest.channel.QueryInterface(nsIHttpChannel);
      if (channel.requestSucceeded) {
        document = this.mXMLHttpRequest.responseXML;
      }
      else {
        this.reportError(aAction);
        return null;
      }
    }

    return document;
  },

  /*
   * The purpose of this method is to select a style sheet ( XSL )
   * based on the document's namespace and then initiate transformation
   * of the machine readable privacy policy into a human readable summary.
   */
  preTransform : function ()
  {
    try {
      this.mStyle  = document.implementation.createDocument("", "", null);
      this.mAction = DO_TRANSFORM;
      this.mStyle.addEventListener("load", this, false);

      var sheet = null;
      var ns = this.getDocument().documentElement.namespaceURI;

      if (ns == "http://www.w3.org/2002/01/P3Pv1") {
        sheet = STYLESHEET_URL_200201;
      } else if (ns == "http://www.w3.org/2001/09/P3Pv1") {
        sheet = STYLESHEET_URL_200109;
      } else if (ns == "http://www.w3.org/2000/12/P3Pv1") {
        sheet = STYLESHEET_URL_200012;
      } else if (ns == "http://www.w3.org/2000/10/18/P3Pv1") {
        sheet = STYLESHEET_URL_200010;
      } else if (ns == "http://www.w3.org/2000/P3Pv1") {
        sheet = STYLESHEET_URL_200005;
      } else {
        this.reportError(SUMMARY_ERROR);
      }

      if (sheet) {
        this.mStyle.load(sheet);
      }
    }
    catch(ex) {
      this.reportError(LOAD_SUMMARY);
    }
  },

  /*
   * This is a callback method. This method assumes that
   * a style sheet is available, for the document in question,
   * and therefore the document ( XML ) is ready for the
   * transformation.
   */
  transform : function ()
  {
    this.mStyle.removeEventListener("load", this, false);

    if (!this.mXSLTProcessor) {
      this.mXSLTProcessor = new XSLTProcessor();
    }

    // An onload event ( triggered when a result window is opened )
    // would start the transformation.
    var resultWin = window.openDialog("chrome://p3p/content/p3pSummary.xul", "_blank",
                                      "chrome,all,dialog=no", this.mXSLTProcessor,
                                      this.getDocument(), this.mStyle, this.mPolicyURL);
  },

  /*
   * The purpose of this method is to display localized errors to the user
   */
  reportError : function (aAction)
  {
    var name = null;
    switch (aAction) {
      case LOAD_POLICY :
        name = "PolicyLoadFailed"; break;
      case LOAD_SUMMARY :
        name = "SummaryLoadFailed"; break;
      case LOAD_OPTIONS :
        name = "OptionLoadFailed";  break;
      case POLICY_ERROR :
        name = "PolicyError"; break;
      case SUMMARY_ERROR :
        name = "SummaryError"; break;
      case OPTIONS_ERROR :
        name = "OptionsError"; break;
      case DO_TRANSFORM:
        name = "SummaryError"; break;
      default:
        name = "PolicyLoadFailed"; break;
    }

    var spec = this.mSelectedURI.spec;
    if (!spec) {
      spec = this.mSelectedURI;
      this.mSelectedURI = null;
    }

    var errorMessage = getBundle().formatStringFromName(name, [spec], 1);
    alertMessage(errorMessage);
  }
};

/*
 * @return string bundle service.
 */
function getStrBundleService ()
{
  if (!gStrBundleService) {
    gStrBundleService =
      Components.classes["@mozilla.org/intl/stringbundle;1"].getService(nsIStringBundleService);
  }
  return gStrBundleService;
}

/*
 * This method is required for localized messages.
 * @return string bundle.
 */
function getBundle ()
{
  if (!gStrBundle) {
    gStrBundle = getStrBundleService().createBundle("chrome://p3p/locale/P3P.properties");
  }
  return gStrBundle;
}

/*
 * @return brand name.
 */
function getBrandName ()
{
  if (!gBrandName) {
    var brandBundle = getStrBundleService().createBundle("chrome://global/locale/brand.properties");
    gBrandName = brandBundle.GetStringFromName("brandShortName")
  }
  return gBrandName;
}

/*
 * This functions compares two URLs for equality
 * That is, if the scheme, host, and port match then
 * the URLs are considered to be equal.
 * Ex. http://www.example.com/ == http://www.example.com/doc
 *     http://www.example.com:80/ == http://www.example.com
 */
function isEquivalent (aLHS, aRHS)
{
  var scheme1 = aLHS.scheme;
  var scheme2 = aRHS.scheme;
  if (scheme1.match(scheme2) && aLHS.host.match(aRHS.host)) {
    var port1 = aLHS.port;
    var port2 = aRHS.port;
    // A port value of -1 corresponds to the protocol's default port.
    // 80 -> http, 443 -> https
    if (scheme1.match("http")) {
      port1 = (port1 == 80) ? -1 : port1;
      port2 = (port2 == 80) ? -1 : port2;
    }
    else if (scheme1.match("https")) {
      port1 = (port1 == 443) ? -1 : port1;
      port2 = (port2 == 443) ? -1 : port2;
    }

    if (port1 == port2) {
      return true;
    }
  }
  return false;
}

function alertMessage(aMessage)
{
  if (!gPromptService) {
    gPromptService =
      Components.classes["@mozilla.org/embedcomp/prompt-service;1"].getService(nsIPromptService);
  }
  gPromptService.alert(window, getBrandName(), aMessage);
}
