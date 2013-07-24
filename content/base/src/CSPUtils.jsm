/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Content Security Policy Utilities
 *
 * Overview
 * This contains a set of classes and utilities for CSP.  It is in this
 * separate file for testing purposes.
 */

const Cu = Components.utils;
const Ci = Components.interfaces;

const WARN_FLAG = Ci.nsIScriptError.warningFlag;
const ERROR_FLAG = Ci.nsIScriptError.ERROR_FLAG;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "Services",
                                  "resource://gre/modules/Services.jsm");

// Module stuff
this.EXPORTED_SYMBOLS = ["CSPRep", "CSPSourceList", "CSPSource", "CSPHost",
                         "CSPdebug", "CSPViolationReportListener", "CSPLocalizer"];

var STRINGS_URI = "chrome://global/locale/security/csp.properties";

// these are not exported
var gIoService = Components.classes["@mozilla.org/network/io-service;1"]
                 .getService(Ci.nsIIOService);

var gETLDService = Components.classes["@mozilla.org/network/effective-tld-service;1"]
                   .getService(Ci.nsIEffectiveTLDService);

// These regexps represent the concrete syntax on the w3 spec as of 7-5-2012
// scheme          = <scheme production from RFC 3986>
const R_SCHEME     = new RegExp ("([a-zA-Z0-9\\-]+)", 'i');
const R_GETSCHEME  = new RegExp ("^" + R_SCHEME.source + "(?=\\:)", 'i');

// scheme-source   = scheme ":"
const R_SCHEMESRC  = new RegExp ("^" + R_SCHEME.source + "\\:$", 'i');

// host-char       = ALPHA / DIGIT / "-"
const R_HOSTCHAR   = new RegExp ("[a-zA-Z0-9\\-]", 'i');

// host            = "*" / [ "*." ] 1*host-char *( "." 1*host-char )
const R_HOST       = new RegExp ("\\*|(((\\*\\.)?" + R_HOSTCHAR.source +
                              "+)" + "(\\." + R_HOSTCHAR.source + "+)*)", 'i');

// port            = ":" ( 1*DIGIT / "*" )
const R_PORT       = new RegExp ("(\\:([0-9]+|\\*))", 'i');

// host-source     = [ scheme "://" ] host [ port ]
const R_HOSTSRC    = new RegExp ("^((" + R_SCHEME.source + "\\:\\/\\/)?("
                                       +   R_HOST.source + ")"
                                       +   R_PORT.source + "?)$", 'i');

// ext-host-source = host-source "/" *( <VCHAR except ";" and ","> )
//                 ; ext-host-source is reserved for future use.
const R_EXTHOSTSRC = new RegExp ("^" + R_HOSTSRC.source + "\\/[:print:]+$", 'i');

// keyword-source  = "'self'" / "'unsafe-inline'" / "'unsafe-eval'"
const R_KEYWORDSRC = new RegExp ("^('self'|'unsafe-inline'|'unsafe-eval')$", 'i');

// source-exp      = scheme-source / host-source / keyword-source
const R_SOURCEEXP  = new RegExp (R_SCHEMESRC.source + "|" +
                                   R_HOSTSRC.source + "|" +
                                R_KEYWORDSRC.source,  'i');


var gPrefObserver = {
  get debugEnabled () {
    if (!this._branch)
      this._initialize();
    return this._debugEnabled;
  },

  _initialize: function() {
    var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Ci.nsIPrefService);
    this._branch = prefSvc.getBranch("security.csp.");
    this._branch.addObserver("", this, false);
    this._debugEnabled = this._branch.getBoolPref("debug");
  },

  unregister: function() {
    if (!this._branch) return;
    this._branch.removeObserver("", this);
  },

  observe: function(aSubject, aTopic, aData) {
    if (aTopic != "nsPref:changed") return;
    if (aData === "debug")
      this._debugEnabled = this._branch.getBoolPref("debug");
  },
};

this.CSPdebug = function CSPdebug(aMsg) {
  if (!gPrefObserver.debugEnabled) return;

  aMsg = 'CSP debug: ' + aMsg + "\n";
  Components.classes["@mozilla.org/consoleservice;1"]
                    .getService(Ci.nsIConsoleService)
                    .logStringMessage(aMsg);
}

// Callback to resume a request once the policy-uri has been fetched
function CSPPolicyURIListener(policyURI, docRequest, csp) {
  this._policyURI = policyURI;    // location of remote policy
  this._docRequest = docRequest;  // the parent document request
  this._csp = csp;                // parent document's CSP
  this._policy = "";              // contents fetched from policyURI
  this._wrapper = null;           // nsIScriptableInputStream
  this._docURI = docRequest.QueryInterface(Ci.nsIChannel)
                 .URI;    // parent document URI (to be used as 'self')
}

CSPPolicyURIListener.prototype = {

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest:
  function(request, context) {},

  onDataAvailable:
  function(request, context, inputStream, offset, count) {
    if (this._wrapper == null) {
      this._wrapper = Components.classes["@mozilla.org/scriptableinputstream;1"]
                      .createInstance(Ci.nsIScriptableInputStream);
      this._wrapper.init(inputStream);
    }
    // store the remote policy as it becomes available
    this._policy += this._wrapper.read(count);
  },

  onStopRequest:
  function(request, context, status) {
    if (Components.isSuccessCode(status)) {
      // send the policy we received back to the parent document's CSP
      // for parsing
      this._csp.refinePolicy(this._policy, this._docURI,
                             this._csp._specCompliant);
    }
    else {
      // problem fetching policy so fail closed
      this._csp.refinePolicy("default-src 'none'", this._docURI,
                             this._csp._specCompliant);
    }
    // resume the parent document request
    this._docRequest.resume();
  }
};

//:::::::::::::::::::::::: CLASSES :::::::::::::::::::::::::://

/**
 * Class that represents a parsed policy structure.
 *
 * @param aSpecCompliant: true: this policy is a CSP 1.0 spec
 *                   compliant policy and should be parsed as such.
 *                   false or undefined: this is a policy using
 *                   our original implementation's CSP syntax.
 */
this.CSPRep = function CSPRep(aSpecCompliant) {
  // this gets set to true when the policy is done parsing, or when a
  // URI-borne policy has finished loading.
  this._isInitialized = false;

  this._allowEval = false;
  this._allowInlineScripts = false;

  // don't auto-populate _directives, so it is easier to find bugs
  this._directives = {};

  // Is this a 1.0 spec compliant CSPRep ?
  // Default to false if not specified.
  this._specCompliant = (aSpecCompliant !== undefined) ? aSpecCompliant : false;

  // Only CSP 1.0 spec compliant policies block inline styles by default.
  this._allowInlineStyles = !aSpecCompliant;
}

// Source directives for our original CSP implementation.
// These can be removed when the original implementation is deprecated.
CSPRep.SRC_DIRECTIVES_OLD = {
  DEFAULT_SRC:      "default-src",
  SCRIPT_SRC:       "script-src",
  STYLE_SRC:        "style-src",
  MEDIA_SRC:        "media-src",
  IMG_SRC:          "img-src",
  OBJECT_SRC:       "object-src",
  FRAME_SRC:        "frame-src",
  FRAME_ANCESTORS:  "frame-ancestors",
  FONT_SRC:         "font-src",
  XHR_SRC:          "xhr-src"
};

// Source directives for our CSP 1.0 spec compliant implementation.
CSPRep.SRC_DIRECTIVES_NEW = {
  DEFAULT_SRC:      "default-src",
  SCRIPT_SRC:       "script-src",
  STYLE_SRC:        "style-src",
  MEDIA_SRC:        "media-src",
  IMG_SRC:          "img-src",
  OBJECT_SRC:       "object-src",
  FRAME_SRC:        "frame-src",
  FRAME_ANCESTORS:  "frame-ancestors",
  FONT_SRC:         "font-src",
  CONNECT_SRC:      "connect-src"
};

CSPRep.URI_DIRECTIVES = {
  REPORT_URI:       "report-uri", /* list of URIs */
  POLICY_URI:       "policy-uri"  /* single URI */
};

// These directives no longer exist in CSP 1.0 and
// later and will eventually be removed when we no longer
// support our original implementation's syntax.
CSPRep.OPTIONS_DIRECTIVE = "options";
CSPRep.ALLOW_DIRECTIVE   = "allow";

/**
  * Factory to create a new CSPRep, parsed from a string.
  *
  * @param aStr
  *        string rep of a CSP
  * @param self (optional)
  *        URI representing the "self" source
  * @param docRequest (optional)
  *        request for the parent document which may need to be suspended
  *        while the policy-uri is asynchronously fetched
  * @param csp (optional)
  *        the CSP object to update once the policy has been fetched
  * @returns
  *        an instance of CSPRep
  */
CSPRep.fromString = function(aStr, self, docRequest, csp) {
  var SD = CSPRep.SRC_DIRECTIVES_OLD;
  var UD = CSPRep.URI_DIRECTIVES;
  var aCSPR = new CSPRep();
  aCSPR._originalText = aStr;
  aCSPR._innerWindowID = innerWindowFromRequest(docRequest);

  var selfUri = null;
  if (self instanceof Ci.nsIURI) {
    selfUri = self.cloneIgnoringRef();
    // clean userpass out of the URI (not used for CSP origin checking, but
    // shows up in prePath).
    try {
      // GetUserPass throws for some protocols without userPass
      selfUri.userPass = '';
    } catch (ex) {}
  }

  var dirs = aStr.split(";");

  directive:
  for each(var dir in dirs) {
    dir = dir.trim();
    if (dir.length < 1) continue;

    var dirname = dir.split(/\s+/)[0];
    var dirvalue = dir.substring(dirname.length).trim();

    if (aCSPR._directives.hasOwnProperty(dirname)) {
      // Check for (most) duplicate directives
      cspError(aCSPR, CSPLocalizer.getFormatStr("duplicateDirective",
                                                [dirname]));
      CSPdebug("Skipping duplicate directive: \"" + dir + "\"");
      continue directive;
    }

    // OPTIONS DIRECTIVE ////////////////////////////////////////////////
    if (dirname === CSPRep.OPTIONS_DIRECTIVE) {
      if (aCSPR._allowInlineScripts || aCSPR._allowEval) {
        // Check for duplicate options directives
        cspError(aCSPR, CSPLocalizer.getFormatStr("duplicateDirective",
                                                  [dirname]));
        CSPdebug("Skipping duplicate directive: \"" + dir + "\"");
        continue directive;
      }

      // grab value tokens and interpret them
      var options = dirvalue.split(/\s+/);
      for each (var opt in options) {
        if (opt === "inline-script")
          aCSPR._allowInlineScripts = true;
        else if (opt === "eval-script")
          aCSPR._allowEval = true;
        else
          cspWarn(aCSPR, CSPLocalizer.getFormatStr("doNotUnderstandOption",
                                                   [opt]));
      }
      continue directive;
    }

    // ALLOW DIRECTIVE //////////////////////////////////////////////////
    // parse "allow" as equivalent to "default-src", at least until the spec
    // stabilizes, at which time we can stop parsing "allow"
    if (dirname === CSPRep.ALLOW_DIRECTIVE) {
      cspWarn(aCSPR, CSPLocalizer.getStr("allowDirectiveIsDeprecated"));
      if (aCSPR._directives.hasOwnProperty(SD.DEFAULT_SRC)) {
        // Check for duplicate default-src and allow directives
        cspError(aCSPR, CSPLocalizer.getFormatStr("duplicateDirective",
                                                  [dirname]));
        CSPdebug("Skipping duplicate directive: \"" + dir + "\"");
        continue directive;
      }
      var dv = CSPSourceList.fromString(dirvalue, aCSPR, selfUri, true);
      if (dv) {
        aCSPR._directives[SD.DEFAULT_SRC] = dv;
        continue directive;
      }
    }

    // SOURCE DIRECTIVES ////////////////////////////////////////////////
    for each(var sdi in SD) {
      if (dirname === sdi) {
        // process dirs, and enforce that 'self' is defined.
        var dv = CSPSourceList.fromString(dirvalue, aCSPR, selfUri, true);
        if (dv) {
          aCSPR._directives[sdi] = dv;
          continue directive;
        }
      }
    }

    // REPORT URI ///////////////////////////////////////////////////////
    if (dirname === UD.REPORT_URI) {
      // might be space-separated list of URIs
      var uriStrings = dirvalue.split(/\s+/);
      var okUriStrings = [];

      for (let i in uriStrings) {
        var uri = null;
        try {
          // Relative URIs are okay, but to ensure we send the reports to the
          // right spot, the relative URIs are expanded here during parsing.
          // The resulting CSPRep instance will have only absolute URIs.
          uri = gIoService.newURI(uriStrings[i],null,selfUri);

          // if there's no host, don't do the ETLD+ check.  This will throw
          // NS_ERROR_FAILURE if the URI doesn't have a host, causing a parse
          // failure.
          uri.host;

          // Verify that each report URI is in the same etld + 1 and that the
          // scheme and port match "self" if "self" is defined, and just that
          // it's valid otherwise.
          if (self) {
            if (gETLDService.getBaseDomain(uri) !==
                gETLDService.getBaseDomain(selfUri)) {
              cspWarn(aCSPR,
                      CSPLocalizer.getFormatStr("notETLDPlus1",
                                            [gETLDService.getBaseDomain(uri)]));
              continue;
            }
            if (!uri.schemeIs(selfUri.scheme)) {
              cspWarn(aCSPR, CSPLocalizer.getFormatStr("notSameScheme",
                                                       [uri.asciiSpec]));
              continue;
            }
            if (uri.port && uri.port !== selfUri.port) {
              cspWarn(aCSPR, CSPLocalizer.getFormatStr("notSamePort",
                                                       [uri.asciiSpec]));
              continue;
            }
          }
        } catch(e) {
          switch (e.result) {
            case Components.results.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS:
            case Components.results.NS_ERROR_HOST_IS_IP_ADDRESS:
              if (uri.host !== selfUri.host) {
                cspWarn(aCSPR,
                        CSPLocalizer.getFormatStr("pageCannotSendReportsTo",
                                                  [selfUri.host, uri.host]));
                continue;
              }
              break;

            default:
              cspWarn(aCSPR, CSPLocalizer.getFormatStr("couldNotParseReportURI",
                                                       [uriStrings[i]]));
              continue;
          }
        }
        // all verification passed: same ETLD+1, scheme, and port.
        okUriStrings.push(uri.asciiSpec);
      }
      aCSPR._directives[UD.REPORT_URI] = okUriStrings.join(' ');
      continue directive;
    }

    // POLICY URI //////////////////////////////////////////////////////////
    if (dirname === UD.POLICY_URI) {
      // POLICY_URI can only be alone
      if (aCSPR._directives.length > 0 || dirs.length > 1) {
        cspError(aCSPR, CSPLocalizer.getStr("policyURINotAlone"));
        return CSPRep.fromString("default-src 'none'");
      }
      // if we were called without a reference to the parent document request
      // we won't be able to suspend it while we fetch the policy -> fail closed
      if (!docRequest || !csp) {
        cspError(aCSPR, CSPLocalizer.getStr("noParentRequest"));
        return CSPRep.fromString("default-src 'none'");
      }

      var uri = '';
      try {
        uri = gIoService.newURI(dirvalue, null, selfUri);
      } catch(e) {
        cspError(aCSPR, CSPLocalizer.getFormatStr("policyURIParseError",
                                                  [dirvalue]));
        return CSPRep.fromString("default-src 'none'");
      }

      // Verify that policy URI comes from the same origin
      if (selfUri) {
        if (selfUri.host !== uri.host) {
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingHost",
                                                    [uri.host]));
          return CSPRep.fromString("default-src 'none'");
        }
        if (selfUri.port !== uri.port) {
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingPort",
                                                    [uri.port.toString()]));
          return CSPRep.fromString("default-src 'none'");
        }
        if (selfUri.scheme !== uri.scheme) {
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingScheme",
                                                    [uri.scheme]));
          return CSPRep.fromString("default-src 'none'");
        }
      }

      // suspend the parent document request while we fetch the policy-uri
      try {
        docRequest.suspend();
        var chan = gIoService.newChannel(uri.asciiSpec, null, null);
        // make request anonymous (no cookies, etc.) so the request for the
        // policy-uri can't be abused for CSRF
        chan.loadFlags |= Ci.nsIChannel.LOAD_ANONYMOUS;
        chan.loadGroup = docRequest.loadGroup;
        chan.asyncOpen(new CSPPolicyURIListener(uri, docRequest, csp), null);
      }
      catch (e) {
        // resume the document request and apply most restrictive policy
        docRequest.resume();
        cspError(aCSPR, CSPLocalizer.getFormatStr("errorFetchingPolicy",
                                                  [e.toString()]));
        return CSPRep.fromString("default-src 'none'");
      }

      // return a fully-open policy to be intersected with the contents of the
      // policy-uri when it returns
      return CSPRep.fromString("default-src *");
    }

    // UNIDENTIFIED DIRECTIVE /////////////////////////////////////////////
    cspWarn(aCSPR, CSPLocalizer.getFormatStr("couldNotProcessUnknownDirective",
                                             [dirname]));

  } // end directive: loop

  // the X-Content-Security-Policy syntax requires an allow or default-src
  // directive to be present.
  if (!aCSPR._directives[SD.DEFAULT_SRC]) {
    cspWarn(aCSPR, CSPLocalizer.getStr("allowOrDefaultSrcRequired"));
    return CSPRep.fromString("default-src 'none'", selfUri);
  }
  return aCSPR;
};

/**
  * Factory to create a new CSPRep, parsed from a string, compliant
  * with the CSP 1.0 spec.
  *
  * @param aStr
  *        string rep of a CSP
  * @param self (optional)
  *        URI representing the "self" source
  * @param docRequest (optional)
  *        request for the parent document which may need to be suspended
  *        while the policy-uri is asynchronously fetched
  * @param csp (optional)
  *        the CSP object to update once the policy has been fetched
  * @returns
  *        an instance of CSPRep
  */
// When we deprecate our original CSP implementation, we rename this to
// CSPRep.fromString and remove the existing CSPRep.fromString above.
CSPRep.fromStringSpecCompliant = function(aStr, self, docRequest, csp) {
  var SD = CSPRep.SRC_DIRECTIVES_NEW;
  var UD = CSPRep.URI_DIRECTIVES;
  var aCSPR = new CSPRep(true);
  aCSPR._originalText = aStr;
  aCSPR._innerWindowID = innerWindowFromRequest(docRequest);

  var selfUri = null;
  if (self instanceof Ci.nsIURI) {
    selfUri = self.cloneIgnoringRef();
    // clean userpass out of the URI (not used for CSP origin checking, but
    // shows up in prePath).
    try {
      // GetUserPass throws for some protocols without userPass
      selfUri.userPass = '';
    } catch (ex) {}
  }

  var dirs_list = aStr.split(";");
  var dirs = {};
  for each(var dir in dirs_list) {
    dir = dir.trim();
    if (dir.length < 1) continue;

    var dirname = dir.split(/\s+/)[0];
    var dirvalue = dir.substring(dirname.length).trim();
    dirs[dirname] = dirvalue;
  }

  // Spec compliant policies have different default behavior for inline
  // scripts, styles, and eval. Bug 885433
  aCSPR._allowEval = true;
  aCSPR._allowInlineScripts = true;
  aCSPR._allowInlineStyles = true;

  // In CSP 1.0, you need to opt-in to blocking inline scripts and eval by
  // specifying either default-src or script-src, and to blocking inline
  // styles by specifying either default-src or style-src.
  if ("default-src" in dirs) {
    // Parse the source list (look ahead) so we can set the defaults properly,
    // honoring the 'unsafe-inline' and 'unsafe-eval' keywords
    var defaultSrcValue = CSPSourceList.fromString(dirs["default-src"], null, self);
    if (!defaultSrcValue._allowUnsafeInline) {
      aCSPR._allowInlineScripts = false;
      aCSPR._allowInlineStyles = false;
    }
    if (!defaultSrcValue._allowUnsafeEval) {
      aCSPR._allowEval = false;
    }
  }
  if ("script-src" in dirs) {
    aCSPR._allowInlineScripts = false;
    aCSPR._allowEval = false;
  }
  if ("style-src" in dirs) {
    aCSPR._allowInlineStyles = false;
  }

  directive:
  for (var dirname in dirs) {
    var dirvalue = dirs[dirname];

    if (aCSPR._directives.hasOwnProperty(dirname)) {
      // Check for (most) duplicate directives
      cspError(aCSPR, CSPLocalizer.getFormatStr("duplicateDirective",
                                                [dirname]));
      CSPdebug("Skipping duplicate directive: \"" + dir + "\"");
      continue directive;
    }

    // SOURCE DIRECTIVES ////////////////////////////////////////////////
    for each(var sdi in SD) {
      if (dirname === sdi) {
        // process dirs, and enforce that 'self' is defined.
        var dv = CSPSourceList.fromString(dirvalue, aCSPR, self, true);
        if (dv) {
          // Check for unsafe-inline in style-src
          if (sdi === "style-src" && dv._allowUnsafeInline) {
             aCSPR._allowInlineStyles = true;
          } else if (sdi === "script-src") {
            // Check for unsafe-inline and unsafe-eval in script-src
            if (dv._allowUnsafeInline) {
              aCSPR._allowInlineScripts = true;
            }
            if (dv._allowUnsafeEval) {
              aCSPR._allowEval = true;
            }
          }

          aCSPR._directives[sdi] = dv;
          continue directive;
        }
      }
    }

    // REPORT URI ///////////////////////////////////////////////////////
    if (dirname === UD.REPORT_URI) {
      // might be space-separated list of URIs
      var uriStrings = dirvalue.split(/\s+/);
      var okUriStrings = [];

      for (let i in uriStrings) {
        var uri = null;
        try {
          // Relative URIs are okay, but to ensure we send the reports to the
          // right spot, the relative URIs are expanded here during parsing.
          // The resulting CSPRep instance will have only absolute URIs.
          uri = gIoService.newURI(uriStrings[i],null,selfUri);

          // if there's no host, don't do the ETLD+ check.  This will throw
          // NS_ERROR_FAILURE if the URI doesn't have a host, causing a parse
          // failure.
          uri.host;

          // Verify that each report URI is in the same etld + 1 and that the
          // scheme and port match "self" if "self" is defined, and just that
          // it's valid otherwise.
          if (self) {
            if (gETLDService.getBaseDomain(uri) !==
                gETLDService.getBaseDomain(selfUri)) {
              cspWarn(aCSPR, 
                      CSPLocalizer.getFormatStr("notETLDPlus1",
                                            [gETLDService.getBaseDomain(uri)]));
              continue;
            }
            if (!uri.schemeIs(selfUri.scheme)) {
              cspWarn(aCSPR, 
                      CSPLocalizer.getFormatStr("notSameScheme",
                                                [uri.asciiSpec]));
              continue;
            }
            if (uri.port && uri.port !== selfUri.port) {
              cspWarn(aCSPR, 
                      CSPLocalizer.getFormatStr("notSamePort",
                                                [uri.asciiSpec]));
              continue;
            }
          }
        } catch(e) {
          switch (e.result) {
            case Components.results.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS:
            case Components.results.NS_ERROR_HOST_IS_IP_ADDRESS:
              if (uri.host !== selfUri.host) {
                cspWarn(aCSPR, CSPLocalizer.getFormatStr("pageCannotSendReportsTo",
                                                         [selfUri.host, uri.host]));
                continue;
              }
              break;

            default:
              cspWarn(aCSPR, CSPLocalizer.getFormatStr("couldNotParseReportURI", 
                                                       [uriStrings[i]]));
              continue;
          }
        }
        // all verification passed: same ETLD+1, scheme, and port.
       okUriStrings.push(uri.asciiSpec);
      }
      aCSPR._directives[UD.REPORT_URI] = okUriStrings.join(' ');
      continue directive;
    }

    // POLICY URI //////////////////////////////////////////////////////////
    if (dirname === UD.POLICY_URI) {
      // POLICY_URI can only be alone
      if (aCSPR._directives.length > 0 || dirs.length > 1) {
        cspError(aCSPR, CSPLocalizer.getStr("policyURINotAlone"));
        return CSPRep.fromStringSpecCompliant("default-src 'none'");
      }
      // if we were called without a reference to the parent document request
      // we won't be able to suspend it while we fetch the policy -> fail closed
      if (!docRequest || !csp) {
        cspError(aCSPR, CSPLocalizer.getStr("noParentRequest"));
        return CSPRep.fromStringSpecCompliant("default-src 'none'");
      }

      var uri = '';
      try {
        uri = gIoService.newURI(dirvalue, null, selfUri);
      } catch(e) {
        cspError(aCSPR, CSPLocalizer.getFormatStr("policyURIParseError", [dirvalue]));
        return CSPRep.fromStringSpecCompliant("default-src 'none'");
      }

      // Verify that policy URI comes from the same origin
      if (selfUri) {
        if (selfUri.host !== uri.host){
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingHost", [uri.host]));
          return CSPRep.fromStringSpecCompliant("default-src 'none'");
        }
        if (selfUri.port !== uri.port){
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingPort", [uri.port.toString()]));
          return CSPRep.fromStringSpecCompliant("default-src 'none'");
        }
        if (selfUri.scheme !== uri.scheme){
          cspError(aCSPR, CSPLocalizer.getFormatStr("nonMatchingScheme", [uri.scheme]));
          return CSPRep.fromStringSpecCompliant("default-src 'none'");
        }
      }

      // suspend the parent document request while we fetch the policy-uri
      try {
        docRequest.suspend();
        var chan = gIoService.newChannel(uri.asciiSpec, null, null);
        // make request anonymous (no cookies, etc.) so the request for the
        // policy-uri can't be abused for CSRF
        chan.loadFlags |= Components.interfaces.nsIChannel.LOAD_ANONYMOUS;
        chan.loadGroup = docRequest.loadGroup;
        chan.asyncOpen(new CSPPolicyURIListener(uri, docRequest, csp), null);
      }
      catch (e) {
        // resume the document request and apply most restrictive policy
        docRequest.resume();
        cspError(aCSPR, CSPLocalizer.getFormatStr("errorFetchingPolicy", [e.toString()]));
        return CSPRep.fromStringSpecCompliant("default-src 'none'");
      }

      // return a fully-open policy to be intersected with the contents of the
      // policy-uri when it returns
      return CSPRep.fromStringSpecCompliant("default-src *");
    }

    // UNIDENTIFIED DIRECTIVE /////////////////////////////////////////////
    cspWarn(aCSPR, CSPLocalizer.getFormatStr("couldNotProcessUnknownDirective", [dirname]));

  } // end directive: loop

  return aCSPR;
};

CSPRep.prototype = {
  /**
   * Returns a space-separated list of all report uris defined, or 'none' if there are none.
   */
  getReportURIs:
  function() {
    if (!this._directives[CSPRep.URI_DIRECTIVES.REPORT_URI])
      return "";
    return this._directives[CSPRep.URI_DIRECTIVES.REPORT_URI];
  },

  /**
   * Compares this CSPRep instance to another.
   */
  equals:
  function(that) {
    if (this._directives.length != that._directives.length) {
      return false;
    }
    for (var i in this._directives) {
      if (!that._directives[i] || !this._directives[i].equals(that._directives[i])) {
        return false;
      }
    }
    return (this.allowsInlineScripts === that.allowsInlineScripts)
        && (this.allowsEvalInScripts === that.allowsEvalInScripts)
        && (this.allowsInlineStyles === that.allowsInlineStyles);
  },

  /**
   * Generates canonical string representation of the policy.
   */
  toString:
  function csp_toString() {
    var dirs = [];

    if (!this._specCompliant && (this._allowEval || this._allowInlineScripts)) {
      dirs.push("options" + (this._allowEval ? " eval-script" : "")
                           + (this._allowInlineScripts ? " inline-script" : ""));
    }
    for (var i in this._directives) {
      if (this._directives[i]) {
        dirs.push(i + " " + this._directives[i].toString());
      }
    }
    return dirs.join("; ");
  },

  /**
   * Determines if this policy accepts a URI.
   * @param aContext
   *        one of the SRC_DIRECTIVES defined above
   * @returns
   *        true if the policy permits the URI in given context.
   */
  permits:
  function csp_permits(aURI, aContext) {
    if (!aURI) return false;

    // GLOBALLY ALLOW "about:" SCHEME
    if (aURI instanceof String && aURI.substring(0,6) === "about:")
      return true;
    if (aURI instanceof Ci.nsIURI && aURI.scheme === "about")
      return true;

    // make sure the right directive set is used
    let DIRS = this._specCompliant ? CSPRep.SRC_DIRECTIVES_NEW : CSPRep.SRC_DIRECTIVES_OLD;

    let contextIsSrcDir = false;
    for (var i in DIRS) {
      if (DIRS[i] === aContext) {
        // for catching calls with invalid contexts (below)
        contextIsSrcDir = true;
        if (this._directives.hasOwnProperty(aContext)) {
          return this._directives[aContext].permits(aURI);
        }
        //found matching dir, can stop looking
        break;
      }
    }

    // frame-ancestors is a special case; it doesn't fall back to default-src.
    if (aContext === DIRS.FRAME_ANCESTORS)
      return true;

    // All directives that don't fall back to default-src should have an escape
    // hatch above (like frame-ancestors).
    if (!contextIsSrcDir) {
      // if this code runs, there's probably something calling permits() that
      // shouldn't be calling permits().
      CSPdebug("permits called with invalid load type: " + aContext);
      return false;
    }

    // no directives specifically matched, fall back to default-src.
    // (default-src may not be present for CSP 1.0-compliant policies, and
    // indicates no relevant directives were present and the load should be
    // permitted).
    if (this._directives.hasOwnProperty(DIRS.DEFAULT_SRC)) {
      return this._directives[DIRS.DEFAULT_SRC].permits(aURI);
    }

    // no relevant directives present -- this means for CSP 1.0 that the load
    // should be permitted (and for the old CSP, to block it).
    return this._specCompliant;
  },

  /**
   * Intersects with another CSPRep, deciding the subset policy
   * that should be enforced, and returning a new instance.
   * This assumes that either both CSPReps are specCompliant or they are both
   * not.
   * @param aCSPRep
   *        a CSPRep instance to use as "other" CSP
   * @returns
   *        a new CSPRep instance of the intersection
   */
  intersectWith:
  function cspsd_intersectWith(aCSPRep) {
    var newRep = new CSPRep();

    let DIRS = aCSPRep._specCompliant ? CSPRep.SRC_DIRECTIVES_NEW :
                                        CSPRep.SRC_DIRECTIVES_OLD;

    // one or more of the two CSPReps may not have any given directive.  In
    // these cases, we need to pick "all" or "none" based on the type of CSPRep
    // (spec compliant or not).
    let thisHasDefault = this._directives.hasOwnProperty(DIRS.DEFAULT_SRC),
        thatHasDefault = aCSPRep._directives.hasOwnProperty(DIRS.DEFAULT_SRC);
    for (var dir in DIRS) {
      let dirv = DIRS[dir];
      let thisHasDir = this._directives.hasOwnProperty(dirv),
          thatHasDir = aCSPRep._directives.hasOwnProperty(dirv);


      // if both specific src directives are absent, skip this (new policy will
      // rely on default-src)
      if (!thisHasDir && !thatHasDir) {
        continue;
      }

      // frame-ancestors is a special case; it doesn't fall back to
      // default-src, so only add it to newRep if one or both of the policies
      // have it.
      if (dirv === DIRS.FRAME_ANCESTORS) {
        if (thisHasDir && thatHasDir) {
          // both have frame-ancestors, intersect them.
          newRep._directives[dirv] =
            aCSPRep._directives[dirv].intersectWith(this._directives[dirv]);
        } else if (thisHasDir || thatHasDir) {
          // one or the other has frame-ancestors, copy it.
          newRep._directives[dirv] =
            ( thisHasDir ? this : aCSPRep )._directives[dirv].clone();
        }
      }
      else if (aCSPRep._specCompliant) {
        // CSP 1.0 doesn't require default-src, so an intersection only makes
        // sense if there is a default-src or both policies have the directive.

        if (!thisHasDir && !thisHasDefault) {
          // only aCSPRep has a relevant directive
          newRep._directives[dirv] = aCSPRep._directives[dirv].clone();
        }
        else if (!thatHasDir && !thatHasDefault) {
          // only "this" has a relevant directive
          newRep._directives[dirv] = this._directives[dirv].clone();
        }
        else {
          // both policies have a relevant directive (may be default-src)
          var isect1 = thisHasDir ?
                       this._directives[dirv] :
                       this._directives[DIRS.DEFAULT_SRC];
          var isect2 = thatHasDir ?
                       aCSPRep._directives[dirv] :
                       aCSPRep._directives[DIRS.DEFAULT_SRC];
          newRep._directives[dirv] = isect1.intersectWith(isect2);
        }
      }
      else {
        // pre-1.0 CSP requires a default-src, so we can assume it's here
        // (since the parser created one).
        var isect1 = thisHasDir ?
                     this._directives[dirv] :
                     this._directives[DIRS.DEFAULT_SRC];
        var isect2 = thatHasDir ?
                     aCSPRep._directives[dirv] :
                     aCSPRep._directives[DIRS.DEFAULT_SRC];
        newRep._directives[dirv] = isect1.intersectWith(isect2);
      }
    }

    // REPORT_URI
    var reportURIDir = CSPRep.URI_DIRECTIVES.REPORT_URI;
    if (this._directives[reportURIDir] && aCSPRep._directives[reportURIDir]) {
      newRep._directives[reportURIDir] =
        this._directives[reportURIDir].concat(aCSPRep._directives[reportURIDir]);
    }
    else if (this._directives[reportURIDir]) {
      // blank concat makes a copy of the string.
      newRep._directives[reportURIDir] = this._directives[reportURIDir].concat();
    }
    else if (aCSPRep._directives[reportURIDir]) {
      // blank concat makes a copy of the string.
      newRep._directives[reportURIDir] = aCSPRep._directives[reportURIDir].concat();
    }

    newRep._allowEval =          this.allowsEvalInScripts
                           && aCSPRep.allowsEvalInScripts;

    newRep._allowInlineScripts = this.allowsInlineScripts
                           && aCSPRep.allowsInlineScripts;

    newRep._allowInlineStyles = this.allowsInlineStyles
                           && aCSPRep.allowsInlineStyles;

    newRep._innerWindowID = this._innerWindowID ?
                              this._innerWindowID : aCSPRep._innerWindowID;

    return newRep;
  },

  /**
   * Returns true if "eval" is enabled through the "eval" keyword.
   */
  get allowsEvalInScripts () {
    return this._allowEval;
  },

  /**
   * Returns true if inline scripts are enabled through the "inline"
   * keyword.
   */
  get allowsInlineScripts () {
    return this._allowInlineScripts;
  },

  /**
   * Returns true if inline styles are enabled through the "inline-style"
   * keyword.
   */
  get allowsInlineStyles () {
    return this._allowInlineStyles;
  },

  /**
   * Sends a message to the error console and web developer console.
   * @param aFlag
   *        The nsIScriptError flag constant indicating severity
   * @param aMsg
   *        The message to send
   * @param aSource (optional)
   *        The URL of the file in which the error occurred
   * @param aScriptLine (optional)
   *        The line in the source file which the error occurred
   * @param aLineNum (optional)
   *        The number of the line where the error occurred
   */
  log:
  function cspd_log(aFlag, aMsg, aSource, aScriptLine, aLineNum) {
    var textMessage = "Content Security Policy: " + aMsg;
    var consoleMsg = Components.classes["@mozilla.org/scripterror;1"]
                               .createInstance(Ci.nsIScriptError);
    if (this._innerWindowID) {
      consoleMsg.initWithWindowID(textMessage, aSource, aScriptLine, aLineNum,
                                  0, aFlag,
                                  "CSP",
                                  this._innerWindowID);
    } else {
      consoleMsg.init(textMessage, aSource, aScriptLine, aLineNum, 0,
                      aFlag,
                      "CSP");
    }
    Components.classes["@mozilla.org/consoleservice;1"]
              .getService(Ci.nsIConsoleService).logMessage(consoleMsg);
  },

};

//////////////////////////////////////////////////////////////////////
/**
 * Class to represent a list of sources
 */
this.CSPSourceList = function CSPSourceList() {
  this._sources = [];
  this._permitAllSources = false;

  // When this is true, the source list contains 'unsafe-inline'.
  this._allowUnsafeInline = false;

  // When this is true, the source list contains 'unsafe-eval'.
  this._allowUnsafeEval = false;
}

/**
 * Factory to create a new CSPSourceList, parsed from a string.
 *
 * @param aStr
 *        string rep of a CSP Source List
 * @param aCSPRep
 *        the CSPRep to which this souce list belongs. If null, CSP errors and
 *        warnings will not be sent to the web console.
 * @param self (optional)
 *        URI or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from the source.
 * @returns
 *        an instance of CSPSourceList
 */
CSPSourceList.fromString = function(aStr, aCSPRep, self, enforceSelfChecks) {
  // source-list = *WSP [ source-expression *( 1*WSP source-expression ) *WSP ]
  //             / *WSP "'none'" *WSP

  /* If self parameter is passed, convert to CSPSource,
     unless it is already a CSPSource. */
  if (self && !(self instanceof CSPSource)) {
     self = CSPSource.create(self, aCSPRep);
  }

  var slObj = new CSPSourceList();
  slObj._CSPRep = aCSPRep;
  aStr = aStr.trim();
  // w3 specifies case insensitive equality
  if (aStr.toUpperCase() === "'NONE'") {
    slObj._permitAllSources = false;
    return slObj;
  }

  var tokens = aStr.split(/\s+/);
  for (var i in tokens) {
    if (!R_SOURCEEXP.test(tokens[i])) {
      cspWarn(aCSPRep,
              CSPLocalizer.getFormatStr("failedToParseUnrecognizedSource",
                                        [tokens[i]]));
      continue;
    }
    var src = CSPSource.create(tokens[i], aCSPRep, self, enforceSelfChecks);
    if (!src) {
      cspWarn(aCSPRep,
              CSPLocalizer.getFormatStr("failedToParseUnrecognizedSource",
                                        [tokens[i]]));
      continue;
    }

    // if a source allows unsafe-inline, set our flag to indicate this.
    if (src._allowUnsafeInline)
      slObj._allowUnsafeInline = true;

    // if a source allows unsafe-eval, set our flag to indicate this.
    if (src._allowUnsafeEval)
      slObj._allowUnsafeEval = true;

    // if a source is a *, then we can permit all sources
    if (src.permitAll) {
      slObj._permitAllSources = true;
      return slObj;
    } else {
      slObj._sources.push(src);
    }
  }

  return slObj;
};

CSPSourceList.prototype = {
  /**
   * Compares one CSPSourceList to another.
   *
   * @param that
   *        another CSPSourceList
   * @returns
   *        true if they have the same data
   */
  equals:
  function(that) {
    // special case to default-src * and 'none' to look different
    // (both have a ._sources.length of 0).
    if (that._permitAllSources != this._permitAllSources) {
      return false;
    }
    if (that._sources.length != this._sources.length) {
      return false;
    }
    // sort both arrays and compare like a zipper
    // XXX (sid): I think we can make this more efficient
    var sortfn = function(a,b) {
      return a.toString() > b.toString();
    };
    var a_sorted = this._sources.sort(sortfn);
    var b_sorted = that._sources.sort(sortfn);
    for (var i in a_sorted) {
      if (!a_sorted[i].equals(b_sorted[i])) {
        return false;
      }
    }
    return true;
  },

  /**
   * Generates canonical string representation of the Source List.
   */
  toString:
  function() {
    if (this.isNone()) {
      return "'none'";
    }
    if (this._permitAllSources) {
      return "*";
    }
    return this._sources.map(function(x) { return x.toString(); }).join(" ");
  },

  /**
   * Returns whether or not this source list represents the "'none'" special
   * case.
   */
  isNone:
  function() {
    return (!this._permitAllSources) && (this._sources.length < 1);
  },

  /**
   * Returns whether or not this source list permits all sources (*).
   */
  isAll:
  function() {
    return this._permitAllSources;
  },

  /**
   * Makes a new deep copy of this object.
   * @returns
   *      a new CSPSourceList
   */
  clone:
  function() {
    var aSL = new CSPSourceList();
    aSL._permitAllSources = this._permitAllSources;
    aSL._CSPRep = this._CSPRep;
    for (var i in this._sources) {
      aSL._sources[i] = this._sources[i].clone();
    }
    return aSL;
  },

  /**
   * Determines if this directive accepts a URI.
   * @param aURI
   *        the URI in question
   * @returns
   *        true if the URI matches a source in this source list.
   */
  permits:
  function cspsd_permits(aURI) {
    if (this.isNone())    return false;
    if (this.isAll())     return true;

    for (var i in this._sources) {
      if (this._sources[i].permits(aURI)) {
        return true;
      }
    }
    return false;
  },

  /**
   * Intersects with another CSPSourceList, deciding the subset directive
   * that should be enforced, and returning a new instance.
   * @param that
   *        the other CSPSourceList to intersect "this" with
   * @returns
   *        a new instance of a CSPSourceList representing the intersection
   */
  intersectWith:
  function cspsd_intersectWith(that) {

    var newCSPSrcList = null;

    if (!that) return this.clone();

    if (this.isNone() || that.isNone())
      newCSPSrcList = CSPSourceList.fromString("'none'", this._CSPRep);

    if (this.isAll()) newCSPSrcList = that.clone();
    if (that.isAll()) newCSPSrcList = this.clone();

    if (!newCSPSrcList) {
      // the shortcuts didn't apply, must do intersection the hard way.
      // --  find only common sources

      // XXX (sid): we should figure out a better algorithm for this.
      // This is horribly inefficient.
      var isrcs = [];
      for (var i in this._sources) {
        for (var j in that._sources) {
          var s = that._sources[j].intersectWith(this._sources[i]);
          if (s) {
            isrcs.push(s);
          }
        }
      }
      // Next, remove duplicates
      dup: for (var i = 0; i < isrcs.length; i++) {
        for (var j = 0; j < i; j++) {
          if (isrcs[i].equals(isrcs[j])) {
            isrcs.splice(i, 1);
            i--;
            continue dup;
          }
        }
      }
      newCSPSrcList = new CSPSourceList();
      newCSPSrcList._sources = isrcs;
    }

    if ((!newCSPSrcList._CSPRep) && that._CSPRep) {
      newCSPSrcList._CSPRep = that._CSPRep;
    }
    return newCSPSrcList;
  }
}

//////////////////////////////////////////////////////////////////////
/**
 * Class to model a source (scheme, host, port)
 */
this.CSPSource = function CSPSource() {
  this._scheme = undefined;
  this._port = undefined;
  this._host = undefined;

  //when set to true, this allows all source
  this._permitAll = false;

  // when set to true, this source represents 'self'
  this._isSelf = false;

  // when set to true, this source allows inline scripts or styles
  this._allowUnsafeInline = false;

  // when set to true, this source allows eval to be used
  this._allowUnsafeEval = false;
}

/**
 * General factory method to create a new source from one of the following
 * types:
 *  - nsURI
 *  - string
 *  - CSPSource (clone)
 * @param aData
 *        string, nsURI, or CSPSource
 * @param aCSPRep
 *        The CSPRep this source belongs to. If null, CSP errors and warnings
 *        will not be sent to the web console.
 * @param self (optional)
 *	  if present, string, URI, or CSPSource representing the "self" resource
 * @param enforceSelfChecks (optional)
 *	  if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from the source.
 * @returns
 *        an instance of CSPSource
 */
CSPSource.create = function(aData, aCSPRep, self, enforceSelfChecks) {
  if (typeof aData === 'string')
    return CSPSource.fromString(aData, aCSPRep, self, enforceSelfChecks);

  if (aData instanceof Ci.nsIURI) {
    // clean userpass out of the URI (not used for CSP origin checking, but
    // shows up in prePath).
    let cleanedUri = aData.cloneIgnoringRef();
    try {
      // GetUserPass throws for some protocols without userPass
      cleanedUri.userPass = '';
    } catch (ex) {}

    return CSPSource.fromURI(cleanedUri, aCSPRep, self, enforceSelfChecks);
  }

  if (aData instanceof CSPSource) {
    var ns = aData.clone();
    ns._self = CSPSource.create(self);
    return ns;
  }

  return null;
}

/**
 * Factory to create a new CSPSource, from a nsIURI.
 *
 * Don't use this if you want to wildcard ports!
 *
 * @param aURI
 *        nsIURI rep of a URI
 * @param aCSPRep
 *        The policy this source belongs to. If null, CSP errors and warnings
 *        will not be sent to the web console.
 * @param self (optional)
 *        string or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from aURI.
 * @returns
 *        an instance of CSPSource
 */
CSPSource.fromURI = function(aURI, aCSPRep, self, enforceSelfChecks) {
  if (!(aURI instanceof Ci.nsIURI)) {
    cspError(aCSPRep, CSPLocalizer.getStr("cspSourceNotURI"));
    return null;
  }

  if (!self && enforceSelfChecks) {
    cspError(aCSPRep, CSPLocalizer.getStr("selfDataNotProvided"));
    return null;
  }

  if (self && !(self instanceof CSPSource)) {
    self = CSPSource.create(self, aCSPRep, undefined, false);
  }

  var sObj = new CSPSource();
  sObj._self = self;
  sObj._CSPRep = aCSPRep;

  // PARSE
  // If 'self' is undefined, then use default port for scheme if there is one.

  // grab scheme (if there is one)
  try {
    sObj._scheme = aURI.scheme;
  } catch(e) {
    sObj._scheme = undefined;
    cspError(aCSPRep, CSPLocalizer.getFormatStr("uriWithoutScheme",
                                                [aURI.asciiSpec]));
    return null;
  }

  // grab host (if there is one)
  try {
    // if there's no host, an exception will get thrown
    // (NS_ERROR_FAILURE)
    sObj._host = CSPHost.fromString(aURI.host);
  } catch(e) {
    sObj._host = undefined;
  }

  // grab port (if there is one)
  // creating a source from an nsURI is limited in that one cannot specify "*"
  // for port.  In fact, there's no way to represent "*" differently than
  // a blank port in an nsURI, since "*" turns into -1, and so does an
  // absence of port declaration.

  // port is never inherited from self -- this gets too confusing.
  // Instead, whatever scheme is used (an explicit one or the inherited
  // one) dictates the port if no port is explicitly stated.
  // Set it to undefined here and the default port will be resolved in the
  // getter for .port.
  sObj._port = undefined;
  try {
    // if there's no port, an exception will get thrown
    // (NS_ERROR_FAILURE)
    if (aURI.port > 0) {
      sObj._port = aURI.port;
    }
  } catch(e) {
    sObj._port = undefined;
  }

  return sObj;
};

/**
 * Factory to create a new CSPSource, parsed from a string.
 *
 * @param aStr
 *        string rep of a CSP Source
 * @param aCSPRep
 *        the CSPRep this CSPSource belongs to
 * @param self (optional)
 *        string, URI, or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from aURI.
 * @returns
 *        an instance of CSPSource
 */
CSPSource.fromString = function(aStr, aCSPRep, self, enforceSelfChecks) {
  if (!aStr)
    return null;

  if (!(typeof aStr === 'string')) {
    cspError(aCSPRep, CSPLocalizer.getStr("argumentIsNotString"));
    return null;
  }

  var sObj = new CSPSource();
  sObj._self = self;
  sObj._CSPRep = aCSPRep;


  // if equal, return does match
  if (aStr === "*") {
    sObj._permitAll = true;
    return sObj;
  }

  if (!self && enforceSelfChecks) {
    cspError(aCSPRep, CSPLocalizer.getStr("selfDataNotProvided"));
    return null;
  }

  if (self && !(self instanceof CSPSource)) {
    self = CSPSource.create(self, aCSPRep, undefined, false);
  }

  // Check for scheme-source match - this only matches if the source
  // string is just a scheme with no host.
  if (R_SCHEMESRC.test(aStr)) {
    var schemeSrcMatch = R_GETSCHEME.exec(aStr);
    sObj._scheme = schemeSrcMatch[0];
    if (!sObj._host) sObj._host = CSPHost.fromString("*");
    if (!sObj._port) sObj._port = "*";
    return sObj;
  }

  // check for host-source or ext-host-source match
  if (R_HOSTSRC.test(aStr) || R_EXTHOSTSRC.test(aStr)) {
    var schemeMatch = R_GETSCHEME.exec(aStr);
    // check that the scheme isn't accidentally matching the host. There should
    // be '://' if there is a valid scheme in an (EXT)HOSTSRC
    if (!schemeMatch || aStr.indexOf("://") == -1) {
      sObj._scheme = self.scheme;
      schemeMatch = null;
    } else {
      sObj._scheme = schemeMatch[0];
    }

    // get array of matches to the R_HOST regular expression
    var hostMatch = R_HOSTSRC.exec(aStr);
    if (!hostMatch) {
      cspError(aCSPRep, CSPLocalizer.getFormatStr("couldntParseInvalidSource",
                                                  [aStr]));
      return null;
    }
    // Host regex gets scheme, so remove scheme from aStr. Add 3 for '://'
    if (schemeMatch)
      hostMatch = R_HOSTSRC.exec(aStr.substring(schemeMatch[0].length + 3));

    var portMatch = R_PORT.exec(hostMatch);

    // Host regex also gets port, so remove the port here.
    if (portMatch)
      hostMatch = R_HOSTSRC.exec(hostMatch[0].substring(0, hostMatch[0].length - portMatch[0].length));

    sObj._host = CSPHost.fromString(hostMatch[0]);
    if (!portMatch) {
      // gets the default port for the given scheme
      defPort = Services.io.getProtocolHandler(sObj._scheme).defaultPort;
      if (!defPort) {
        cspError(aCSPRep,
                 CSPLocalizer.getFormatStr("couldntParseInvalidSource",
                                           [aStr]));
        return null;
      }
      sObj._port = defPort;
    }
    else {
      // strip the ':' from the port
      sObj._port = portMatch[0].substr(1);
    }
    return sObj;
  }

  // check for 'self' (case insensitive)
  if (aStr.toUpperCase() === "'SELF'") {
    if (!self) {
      cspError(aCSPRep, CSPLocalizer.getStr("selfKeywordNoSelfData"));
      return null;
    }
    sObj._self = self.clone();
    sObj._isSelf = true;
    return sObj;
  }

  // check for 'unsafe-inline' (case insensitive)
  if (aStr.toUpperCase() === "'UNSAFE-INLINE'"){
    sObj._allowUnsafeInline = true;
    return sObj;
  }

  // check for 'unsafe-eval' (case insensitive)
  if (aStr.toUpperCase() === "'UNSAFE-EVAL'"){
    sObj._allowUnsafeEval = true;
    return sObj;
  }

  cspError(aCSPRep, CSPLocalizer.getFormatStr("couldntParseInvalidSource",
                                              [aStr]));
  return null;
};

CSPSource.validSchemeName = function(aStr) {
  // <scheme-name>       ::= <alpha><scheme-suffix>
  // <scheme-suffix>     ::= <scheme-chr>
  //                      | <scheme-suffix><scheme-chr>
  // <scheme-chr>        ::= <letter> | <digit> | "+" | "." | "-"

  return aStr.match(/^[a-zA-Z][a-zA-Z0-9+.-]*$/);
};

CSPSource.prototype = {

  get scheme () {
    if (this._isSelf && this._self)
      return this._self.scheme;
    if (!this._scheme && this._self)
      return this._self.scheme;
    return this._scheme;
  },

  get host () {
    if (this._isSelf && this._self)
      return this._self.host;
    if (!this._host && this._self)
      return this._self.host;
    return this._host;
  },

  get permitAll () {
    if (this._isSelf && this._self)
      return this._self.permitAll;
    return this._permitAll;
  },

  /**
   * If this doesn't have a nonstandard port (hard-defined), use the default
   * port for this source's scheme. Should never inherit port from 'self'.
   */
  get port () {
    if (this._isSelf && this._self)
      return this._self.port;
    if (this._port) return this._port;
    // if no port, get the default port for the scheme
    // (which may be inherited from 'self')
    if (this.scheme) {
      try {
        var port = gIoService.getProtocolHandler(this.scheme).defaultPort;
        if (port > 0) return port;
      } catch(e) {
        // if any errors happen, fail gracefully.
      }
    }

    return undefined;
  },

  /**
   * Generates canonical string representation of the Source.
   */
  toString:
  function() {
    if (this._isSelf)
      return this._self.toString();

    if (this._allowUnsafeInline)
      return "unsafe-inline";

    if (this._allowUnsafeEval)
      return "unsafe-eval";

    var s = "";
    if (this.scheme)
      s = s + this.scheme + "://";
    if (this._host)
      s = s + this._host;
    if (this.port)
      s = s + ":" + this.port;
    return s;
  },

  /**
   * Makes a new deep copy of this object.
   * @returns
   *      a new CSPSource
   */
  clone:
  function() {
    var aClone = new CSPSource();
    aClone._self = this._self ? this._self.clone() : undefined;
    aClone._scheme = this._scheme;
    aClone._port = this._port;
    aClone._host = this._host ? this._host.clone() : undefined;
    aClone._isSelf = this._isSelf;
    aClone._CSPRep = this._CSPRep;
    return aClone;
  },

  /**
   * Determines if this Source accepts a URI.
   * @param aSource
   *        the URI, or CSPSource in question
   * @returns
   *        true if the URI matches a source in this source list.
   */
  permits:
  function(aSource) {
    if (!aSource) return false;

    if (!(aSource instanceof CSPSource))
      aSource = CSPSource.create(aSource, this._CSPRep);

    // verify scheme
    if (this.scheme != aSource.scheme)
      return false;

    // port is defined in 'this' (undefined means it may not be relevant
    // to the scheme) AND this port (implicit or explicit) matches
    // aSource's port
    if (this.port && this.port !== "*" && this.port != aSource.port)
      return false;

    // host is defined in 'this' (undefined means it may not be relevant
    // to the scheme) AND this host (implicit or explicit) permits
    // aSource's host.
    if (this.host && !this.host.permits(aSource.host))
      return false;

    // all scheme, host and port matched!
    return true;
  },

  /**
   * Determines the intersection of two sources.
   * Returns a null object if intersection generates no
   * hosts that satisfy it.
   * @param that
   *        the other CSPSource to intersect "this" with
   * @returns
   *        a new instance of a CSPSource representing the intersection
   */
  intersectWith:
  function(that) {
    var newSource = new CSPSource();

    // 'self' is not part of the intersection.  Intersect the raw values from
    // the source, self must be set by someone creating this source.
    // When intersecting, we take the more specific of the two: if one scheme,
    // host or port is undefined, the other is taken.  (This is contrary to
    // when "permits" is called -- there, the value of 'self' is looked at
    // when a scheme, host or port is undefined.)

    // port
    if (!this.port)
      newSource._port = that.port;
    else if (!that.port)
      newSource._port = this.port;
    else if (this.port === "*")
      newSource._port = that.port;
    else if (that.port === "*")
      newSource._port = this.port;
    else if (that.port === this.port)
      newSource._port = this.port;
    else {
      let msg = CSPLocalizer.getFormatStr("notIntersectPort",
                                          [this.toString(), that.toString()]);
      cspError(this._CSPRep, msg);
      return null;
    }

    // scheme
    if (!this.scheme)
      newSource._scheme = that.scheme;
    else if (!that.scheme)
      newSource._scheme = this.scheme;
    if (this.scheme === "*")
      newSource._scheme = that.scheme;
    else if (that.scheme === "*")
      newSource._scheme = this.scheme;
    else if (that.scheme === this.scheme)
      newSource._scheme = this.scheme;
    else {
      var msg = CSPLocalizer.getFormatStr("notIntersectScheme",
                                          [this.toString(), that.toString()]);
      cspError(this._CSPRep, msg);
      return null;
    }

    // NOTE: Both sources must have a host, if they don't, something funny is
    // going on.  The fromString() factory method should have set the host to
    // * if there's no host specified in the input. Regardless, if a host is
    // not present either the scheme is hostless or any host should be allowed.
    // This means we can use the other source's host as the more restrictive
    // host expression, or if neither are present, we can use "*", but the
    // error should still be reported.

    // host
    if (this.host && that.host) {
      newSource._host = this.host.intersectWith(that.host);
    } else if (this.host) {
      let msg = CSPLocalizer.getFormatStr("intersectingSourceWithUndefinedHost",
                                          [that.toString()]);
      cspError(this._CSPRep, msg);
      newSource._host = this.host.clone();
    } else if (that.host) {
      let msg = CSPLocalizer.getFormatStr("intersectingSourceWithUndefinedHost",
                                          [this.toString()]);
      cspError(this._CSPRep, msg);
      newSource._host = that.host.clone();
    } else {
      let msg = CSPLocalizer.getFormatStr("intersectingSourcesWithUndefinedHosts",
                                          [this.toString(), that.toString()]);
      cspError(this._CSPRep, msg);
      newSource._host = CSPHost.fromString("*");
    }

    return newSource;
  },

  /**
   * Compares one CSPSource to another.
   *
   * @param that
   *        another CSPSource
   * @param resolveSelf (optional)
   *        if present, and 'true', implied values are obtained from 'self'
   *        instead of assumed to be "anything"
   * @returns
   *        true if they have the same data
   */
  equals:
  function(that, resolveSelf) {
    // 1. schemes match
    // 2. ports match
    // 3. either both hosts are undefined, or one equals the other.
    if (resolveSelf)
      return this.scheme === that.scheme
          && this.port   === that.port
          && (!(this.host || that.host) ||
               (this.host && this.host.equals(that.host)));

    // otherwise, compare raw (non-self-resolved values)
    return this._scheme === that._scheme
        && this._port   === that._port
        && (!(this._host || that._host) ||
              (this._host && this._host.equals(that._host)));
  },

};

//////////////////////////////////////////////////////////////////////
/**
 * Class to model a host *.x.y.
 */
this.CSPHost = function CSPHost() {
  this._segments = [];
}

/**
 * Factory to create a new CSPHost, parsed from a string.
 *
 * @param aStr
 *        string rep of a CSP Host
 * @returns
 *        an instance of CSPHost
 */
CSPHost.fromString = function(aStr) {
  if (!aStr) return null;

  // host string must be LDH with dots and stars.
  var invalidChar = aStr.match(/[^a-zA-Z0-9\-\.\*]/);
  if (invalidChar) {
    CSPdebug("Invalid character '" + invalidChar + "' in host " + aStr);
    return null;
  }

  var hObj = new CSPHost();
  hObj._segments = aStr.split(/\./);
  if (hObj._segments.length < 1)
    return null;

  // validate data in segments
  for (var i in hObj._segments) {
    var seg = hObj._segments[i];
    if (seg == "*") {
      if (i > 0) {
        // Wildcard must be FIRST
        CSPdebug("Wildcard char located at invalid position in '" + aStr + "'");
        return null;
      }
    }
    else if (seg.match(/[^a-zA-Z0-9\-]/)) {
      // Non-wildcard segment must be LDH string
      CSPdebug("Invalid segment '" + seg + "' in host value");
      return null;
    }
  }
  return hObj;
};

CSPHost.prototype = {
  /**
   * Generates canonical string representation of the Host.
   */
  toString:
  function() {
    return this._segments.join(".");
  },

  /**
   * Makes a new deep copy of this object.
   * @returns
   *      a new CSPHost
   */
  clone:
  function() {
    var aHost = new CSPHost();
    for (var i in this._segments) {
      aHost._segments[i] = this._segments[i];
    }
    return aHost;
  },

  /**
   * Returns true if this host accepts the provided host (or the other way
   * around).
   * @param aHost
   *        the FQDN in question (CSPHost or String)
   * @returns
   */
  permits:
  function(aHost) {
    if (!aHost) aHost = CSPHost.fromString("*");

    if (!(aHost instanceof CSPHost)) {
      // -- compare CSPHost to String
      aHost =  CSPHost.fromString(aHost);
    }
    var thislen = this._segments.length;
    var thatlen = aHost._segments.length;

    // don't accept a less specific host:
    //   \--> *.b.a doesn't accept b.a.
    if (thatlen < thislen) { return false; }

    // check for more specific host (and wildcard):
    //   \--> *.b.a accepts d.c.b.a.
    //   \--> c.b.a doesn't accept d.c.b.a.
    if ((thatlen > thislen) && this._segments[0] != "*") {
      return false;
    }

    // Given the wildcard condition (from above),
    // only necessary to compare elements that are present
    // in this host.  Extra tokens in aHost are ok.
    // * Compare from right to left.
    for (var i=1; i <= thislen; i++) {
      if (this._segments[thislen-i] != "*" &&
          (this._segments[thislen-i] != aHost._segments[thatlen-i])) {
        return false;
      }
    }

    // at this point, all conditions are met, so the host is allowed
    return true;
  },

  /**
   * Determines the intersection of two Hosts.
   * Basically, they must be the same, or one must have a wildcard.
   * @param that
   *        the other CSPHost to intersect "this" with
   * @returns
   *        a new instance of a CSPHost representing the intersection
   *        (or null, if they can't be intersected)
   */
  intersectWith:
  function(that) {
    if (!(this.permits(that) || that.permits(this))) {
      // host definitions cannot co-exist without a more general host
      // ... one must be a subset of the other, or intersection makes no sense.
      return null;
    }

    // pick the more specific one, if both are same length.
    if (this._segments.length == that._segments.length) {
      // *.a vs b.a : b.a
      return (this._segments[0] === "*") ? that.clone() : this.clone();
    }

    // different lengths...
    // *.b.a vs *.a : *.b.a
    // *.b.a vs d.c.b.a : d.c.b.a
    return (this._segments.length > that._segments.length) ?
            this.clone() : that.clone();
  },

  /**
   * Compares one CSPHost to another.
   *
   * @param that
   *        another CSPHost
   * @returns
   *        true if they have the same data
   */
  equals:
  function(that) {
    if (this._segments.length != that._segments.length)
      return false;

    for (var i=0; i<this._segments.length; i++) {
      if (this._segments[i] != that._segments[i])
        return false;
    }
    return true;
  }
};


//////////////////////////////////////////////////////////////////////
/**
 * Class that listens to violation report transmission and logs errors.
 */
this.CSPViolationReportListener = function CSPViolationReportListener(reportURI) {
  this._reportURI = reportURI;
}

CSPViolationReportListener.prototype = {
  _reportURI:   null,

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIStreamListener) ||
        iid.equals(Ci.nsIRequestObserver) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStopRequest:
  function(request, context, status) {
    if (!Components.isSuccessCode(status)) {
      CSPdebug("error " + status.toString(16) +
                " while sending violation report to " +
                this._reportURI);
    }
  },

  onStartRequest:
  function(request, context) { },

  onDataAvailable:
  function(request, context, inputStream, offset, count) {
    // We MUST read equal to count from the inputStream to avoid an assertion.
    var input = Components.classes['@mozilla.org/scriptableinputstream;1']
                .createInstance(Ci.nsIScriptableInputStream);

    input.init(inputStream);
    input.read(count);
  },

};

//////////////////////////////////////////////////////////////////////

function innerWindowFromRequest(docRequest) {
  let win = null;
  let loadContext = null;

  try {
    loadContext = docRequest.notificationCallbacks.getInterface(Ci.nsILoadContext);
  } catch (ex) {
    try {
      loadContext = docRequest.loadGroup.notificationCallbacks.getInterface(Ci.nsILoadContext);
    } catch (ex) {
      return null;
    }
  }

  if (loadContext) {
    win = loadContext.associatedWindow;
  }
  if (win) {
    try {
       let winUtils = win.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils);
       return winUtils.currentInnerWindowID;
    } catch (ex) {
      return null;
    }
  }
  return null;
}

function cspError(aCSPRep, aMessage) {
  if (aCSPRep) {
    aCSPRep.log(ERROR_FLAG, aMessage);
  } else {
    (new CSPRep()).log(ERROR_FLAG, aMessage);
  }
}

function cspWarn(aCSPRep, aMessage) {
  if (aCSPRep) {
    aCSPRep.log(WARN_FLAG, aMessage);
  } else {
    (new CSPRep()).log(WARN_FLAG, aMessage);
  }
}

//////////////////////////////////////////////////////////////////////

this.CSPLocalizer = {
  /**
   * Retrieve a localized string.
   *
   * @param string aName
   *        The string name you want from the CSP string bundle.
   * @return string
   *         The localized string.
   */
  getStr: function CSPLoc_getStr(aName)
  {
    let result;
    try {
      result = this.stringBundle.GetStringFromName(aName);
    }
    catch (ex) {
      Cu.reportError("Failed to get string: " + aName);
      throw ex;
    }
    return result;
  },

  /**
   * Retrieve a localized string formatted with values coming from the given
   * array.
   *
   * @param string aName
   *        The string name you want from the CSP string bundle.
   * @param array aArray
   *        The array of values you want in the formatted string.
   * @return string
   *         The formatted local string.
   */
  getFormatStr: function CSPLoc_getFormatStr(aName, aArray)
  {
    let result;
    try {
      result = this.stringBundle.formatStringFromName(aName, aArray, aArray.length);
    }
    catch (ex) {
      Cu.reportError("Failed to format string: " + aName);
      throw ex;
    }
    return result;
  },
};

XPCOMUtils.defineLazyGetter(CSPLocalizer, "stringBundle", function() {
  return Services.strings.createBundle(STRINGS_URI);
});
