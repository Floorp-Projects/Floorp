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
 * The Original Code is the Content Security Policy data structures.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 *
 * Contributor(s):
 *   Sid Stamm <sid@mozilla.com>
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

/**
 * Content Security Policy Utilities
 * 
 * Overview
 * This contains a set of classes and utilities for CSP.  It is in this
 * separate file for testing purposes.
 */

// Module stuff
var EXPORTED_SYMBOLS = ["CSPRep", "CSPSourceList", "CSPSource", 
                        "CSPHost", "CSPWarning", "CSPError", "CSPdebug"];


// these are not exported
var gIoService = Components.classes["@mozilla.org/network/io-service;1"]
                 .getService(Components.interfaces.nsIIOService);

var gETLDService = Components.classes["@mozilla.org/network/effective-tld-service;1"]
                   .getService(Components.interfaces.nsIEffectiveTLDService);

var gPrefObserver = {
  get debugEnabled () {
    if (!this._branch)
      this._initialize();
    return this._debugEnabled;
  },

  _initialize: function() {
    var prefSvc = Components.classes["@mozilla.org/preferences-service;1"]
                    .getService(Components.interfaces.nsIPrefService);
    this._branch = prefSvc.getBranch("security.csp.");
    this._branch.addObserver("", this, false);
    this._debugEnabled = this._branch.getBoolPref("debug");
  },

  unregister: function() {
    if(!this._branch) return;
    this._branch.removeObserver("", this);
  },

  observe: function(aSubject, aTopic, aData) {
    if(aTopic != "nsPref:changed") return;
    if(aData === "debug")
      this._debugEnabled = this._branch.getBoolPref("debug");
  },

};


function CSPWarning(aMsg, aSource, aScriptSample, aLineNum) {
  var textMessage = 'CSP WARN:  ' + aMsg + "\n";

  var consoleMsg = Components.classes["@mozilla.org/scripterror;1"]
                    .createInstance(Components.interfaces.nsIScriptError);
  consoleMsg.init(textMessage, aSource, aScriptSample, aLineNum, 0,
                  Components.interfaces.nsIScriptError.warningFlag,
                  "Content Security Policy");
  Components.classes["@mozilla.org/consoleservice;1"]
                    .getService(Components.interfaces.nsIConsoleService)
                    .logMessage(consoleMsg);
}

function CSPError(aMsg) {
  var textMessage = 'CSP ERROR:  ' + aMsg + "\n";

  var consoleMsg = Components.classes["@mozilla.org/scripterror;1"]
                    .createInstance(Components.interfaces.nsIScriptError);
  consoleMsg.init(textMessage, null, null, 0, 0,
                  Components.interfaces.nsIScriptError.errorFlag,
                  "Content Security Policy");
  Components.classes["@mozilla.org/consoleservice;1"]
                    .getService(Components.interfaces.nsIConsoleService)
                    .logMessage(consoleMsg);
}

function CSPdebug(aMsg) {
  if (!gPrefObserver.debugEnabled) return;

  aMsg = 'CSP debug: ' + aMsg + "\n";
  Components.classes["@mozilla.org/consoleservice;1"]
                    .getService(Components.interfaces.nsIConsoleService)
                    .logStringMessage(aMsg);
}

// Callback to resume a request once the policy-uri has been fetched
function CSPPolicyURIListener(policyURI, docRequest, csp) {
  this._policyURI = policyURI;    // location of remote policy
  this._docRequest = docRequest;  // the parent document request
  this._csp = csp;                // parent document's CSP
  this._policy = "";              // contents fetched from policyURI
  this._wrapper = null;           // nsIScriptableInputStream
  this._docURI = docRequest.QueryInterface(Components.interfaces.nsIChannel)
                 .URI;    // parent document URI (to be used as 'self')
}

CSPPolicyURIListener.prototype = {

  QueryInterface: function(iid) {
    if (iid.equals(Components.interfaces.nsIStreamListener) ||
        iid.equals(Components.interfaces.nsIRequestObserver) ||
        iid.equals(Components.interfaces.nsISupports))
      return this;
    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  onStartRequest:
  function(request, context) {},

  onDataAvailable:
  function(request, context, inputStream, offset, count) {
    if (this._wrapper == null) {
      this._wrapper = Components.classes["@mozilla.org/scriptableinputstream;1"]
                      .createInstance(Components.interfaces.nsIScriptableInputStream);
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
      this._csp.refinePolicy(this._policy, this._docURI, this._docRequest);
    }
    else {
      // problem fetching policy so fail closed
      this._csp.refinePolicy("allow 'none'", this._docURI, this._docRequest);
      this._csp.refinePolicy("default-src 'none'", this._docURI, this._docRequest);
    }
    // resume the parent document request
    this._docRequest.resume();
  }
};

//:::::::::::::::::::::::: CLASSES ::::::::::::::::::::::::::// 

/**
 * Class that represents a parsed policy structure.
 */
function CSPRep() {
  // this gets set to true when the policy is done parsing, or when a
  // URI-borne policy has finished loading.
  this._isInitialized = false;

  this._allowEval = false;
  this._allowInlineScripts = false;

  // don't auto-populate _directives, so it is easier to find bugs
  this._directives = {};
}

CSPRep.SRC_DIRECTIVES = {
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

CSPRep.URI_DIRECTIVES = {
  REPORT_URI:       "report-uri", /* list of URIs */
  POLICY_URI:       "policy-uri"  /* single URI */
};

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
  var SD = CSPRep.SRC_DIRECTIVES;
  var UD = CSPRep.URI_DIRECTIVES;
  var aCSPR = new CSPRep();
  aCSPR._originalText = aStr;

  var selfUri = null;
  if (self instanceof Components.interfaces.nsIURI)
    selfUri = self.clone();

  var dirs = aStr.split(";");

  directive:
  for each(var dir in dirs) {
    dir = dir.trim();
    if (dir.length < 1) continue;

    var dirname = dir.split(/\s+/)[0];
    var dirvalue = dir.substring(dirname.length).trim();

    // OPTIONS DIRECTIVE ////////////////////////////////////////////////
    if (dirname === CSPRep.OPTIONS_DIRECTIVE) {
      // grab value tokens and interpret them
      var options = dirvalue.split(/\s+/);
      for each (var opt in options) {
        if (opt === "inline-script")
          aCSPR._allowInlineScripts = true;
        else if (opt === "eval-script")
          aCSPR._allowEval = true;
        else
          CSPWarning("don't understand option '" + opt + "'.  Ignoring it.");
      }
      continue directive;
    }

    // ALLOW DIRECTIVE //////////////////////////////////////////////////
    // parse "allow" as equivalent to "default-src", at least until the spec
    // stabilizes, at which time we can stop parsing "allow"
    if (dirname === CSPRep.ALLOW_DIRECTIVE) {
      var dv = CSPSourceList.fromString(dirvalue, self, true);
      if (dv) {
        aCSPR._directives[SD.DEFAULT_SRC] = dv;
        continue directive;
      }
    }

    // SOURCE DIRECTIVES ////////////////////////////////////////////////
    for each(var sdi in SD) {
      if (dirname === sdi) {
        // process dirs, and enforce that 'self' is defined.
        var dv = CSPSourceList.fromString(dirvalue, self, true);
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
              CSPWarning("can't use report URI from non-matching eTLD+1: "
                         + gETLDService.getBaseDomain(uri));
              continue;
            }
            if (!uri.schemeIs(selfUri.scheme)) {
              CSPWarning("can't use report URI with different scheme from "
                         + "originating document: " + uri.asciiSpec);
              continue;
            }
            if (uri.port && uri.port !== selfUri.port) {
              CSPWarning("can't use report URI with different port from "
                         + "originating document: " + uri.asciiSpec);
              continue;
            }
          }
        } catch(e) {
          switch (e.result) {
            case Components.results.NS_ERROR_INSUFFICIENT_DOMAIN_LEVELS:
            case Components.results.NS_ERROR_HOST_IS_IP_ADDRESS:
              if (uri.host !== selfUri.host) {
                CSPWarning("page on " + selfUri.host
                           + " cannot send reports to " + uri.host);
                continue;
              }
              break;

            default:
              CSPWarning("couldn't parse report URI: " + uriStrings[i]);
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
        CSPError("policy-uri directive can only appear alone");
        return CSPRep.fromString("default-src 'none'");
      }
      // if we were called without a reference to the parent document request
      // we won't be able to suspend it while we fetch the policy -> fail closed
      if (!docRequest || !csp) {
        CSPError("The policy-uri cannot be fetched without a parent request and a CSP.");
        return CSPRep.fromString("default-src 'none'");
      }

      var uri = '';
      try {
        uri = gIoService.newURI(dirvalue, null, selfUri);
      } catch(e) {
        CSPError("could not parse URI in policy URI: " + dirvalue);
        return CSPRep.fromString("default-src 'none'");
      }

      // Verify that policy URI comes from the same origin
      if (selfUri) {
        if (selfUri.host !== uri.host){
          CSPError("can't fetch policy uri from non-matching hostname: " + uri.host);
          return CSPRep.fromString("default-src 'none'");
        }
        if (selfUri.port !== uri.port){
          CSPError("can't fetch policy uri from non-matching port: " + uri.port);
          return CSPRep.fromString("default-src 'none'");
        }
        if (selfUri.scheme !== uri.scheme){
          CSPError("can't fetch policy uri from non-matching scheme: " + uri.scheme);
          return CSPRep.fromString("default-src 'none'");
        }
      }

      // suspend the parent document request while we fetch the policy-uri
      try {
        docRequest.suspend();
        var chan = gIoService.newChannel(uri.asciiSpec, null, null);
        // make request anonymous (no cookies, etc.) so the request for the
        // policy-uri can't be abused for CSRF
        chan.loadFlags |= Components.interfaces.nsIChannel.LOAD_ANONYMOUS;
        chan.asyncOpen(new CSPPolicyURIListener(uri, docRequest, csp), null);
      }
      catch (e) {
        // resume the document request and apply most restrictive policy
        docRequest.resume();
        CSPError("Error fetching policy-uri: " + e);
        return CSPRep.fromString("default-src 'none'");
      }

      // return a fully-open policy to be intersected with the contents of the
      // policy-uri when it returns
      return CSPRep.fromString("default-src *");
    }

    // UNIDENTIFIED DIRECTIVE /////////////////////////////////////////////
    CSPWarning("Couldn't process unknown directive '" + dirname + "'");

  } // end directive: loop

  // if makeExplicit fails for any reason, default to default-src 'none'.  This
  // includes the case where "default-src" is not present.
  if (aCSPR.makeExplicit())
    return aCSPR;
  return CSPRep.fromString("default-src 'none'", self);
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
        && (this.allowsEvalInScripts === that.allowsEvalInScripts);
  },

  /**
   * Generates canonical string representation of the policy.
   */
  toString:
  function csp_toString() {
    var dirs = [];

    if (this._allowEval || this._allowInlineScripts) {
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
    if (aURI instanceof Components.interfaces.nsIURI && aURI.scheme === "about")
      return true;

    // make sure the context is valid
    for (var i in CSPRep.SRC_DIRECTIVES) {
      if (CSPRep.SRC_DIRECTIVES[i] === aContext) {
        return this._directives[aContext].permits(aURI);
      }
    }
    return false;
  },

  /**
   * Intersects with another CSPRep, deciding the subset policy 
   * that should be enforced, and returning a new instance.
   * @param aCSPRep
   *        a CSPRep instance to use as "other" CSP
   * @returns
   *        a new CSPRep instance of the intersection
   */
  intersectWith:
  function cspsd_intersectWith(aCSPRep) {
    var newRep = new CSPRep();

    for (var dir in CSPRep.SRC_DIRECTIVES) {
      var dirv = CSPRep.SRC_DIRECTIVES[dir];
      newRep._directives[dirv] = this._directives[dirv]
               .intersectWith(aCSPRep._directives[dirv]);
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

    for (var dir in CSPRep.SRC_DIRECTIVES) {
      var dirv = CSPRep.SRC_DIRECTIVES[dir];
      newRep._directives[dirv] = this._directives[dirv]
               .intersectWith(aCSPRep._directives[dirv]);
    }

    newRep._allowEval =          this.allowsEvalInScripts
                           && aCSPRep.allowsEvalInScripts;

    newRep._allowInlineScripts = this.allowsInlineScripts 
                           && aCSPRep.allowsInlineScripts;

    return newRep;
  },

  /**
   * Copies default source list to each unspecified directive.
   * @returns
   *      true  if the makeExplicit succeeds
   *      false if it fails (for some weird reason)
   */
  makeExplicit:
  function cspsd_makeExplicit() {
    var SD = CSPRep.SRC_DIRECTIVES;
    var defaultSrcDir = this._directives[SD.DEFAULT_SRC];
    if (!defaultSrcDir) {
      CSPWarning("'allow' or 'default-src' directive required but not present.  Reverting to \"default-src 'none'\"");
      return false;
    }

    for (var dir in SD) {
      var dirv = SD[dir];
      if (dirv === SD.DEFAULT_SRC) continue;
      if (!this._directives[dirv]) {
        // implicit directive, make explicit.
        // All but frame-ancestors directive inherit from 'allow' (bug 555068)
        if (dirv === SD.FRAME_ANCESTORS)
          this._directives[dirv] = CSPSourceList.fromString("*");
        else
          this._directives[dirv] = defaultSrcDir.clone();
        this._directives[dirv]._isImplicit = true;
      }
    }
    this._isInitialized = true;
    return true;
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
};

//////////////////////////////////////////////////////////////////////
/**
 * Class to represent a list of sources 
 */
function CSPSourceList() {
  this._sources = [];
  this._permitAllSources = false;

  // Set to true when this list is created using "makeExplicit()"
  // It's useful to know this when reporting the directive that was violated.
  this._isImplicit = false;
}

/**
 * Factory to create a new CSPSourceList, parsed from a string.
 *
 * @param aStr
 *        string rep of a CSP Source List
 * @param self (optional)
 *        URI or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from the source.
 * @returns
 *        an instance of CSPSourceList 
 */
CSPSourceList.fromString = function(aStr, self, enforceSelfChecks) {
  // Source list is:
  //    <host-dir-value> ::= <source-list>
  //                       | "'none'"
  //    <source-list>    ::= <source>
  //                       | <source-list>" "<source>

  /* If self parameter is passed, convert to CSPSource,
     unless it is already a CSPSource. */
  if(self && !(self instanceof CSPSource)) {
     self = CSPSource.create(self);
  }

  var slObj = new CSPSourceList();
  if (aStr === "'none'")
    return slObj;

  if (aStr === "*") {
    slObj._permitAllSources = true;
    return slObj;
  }

  var tokens = aStr.split(/\s+/);
  for (var i in tokens) {
    if (tokens[i] === "") continue;
    var src = CSPSource.create(tokens[i], self, enforceSelfChecks);
    if (!src) {
      CSPWarning("Failed to parse unrecoginzied source " + tokens[i]);
      continue;
    }
    slObj._sources.push(src);
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

    if (this.isNone() || that.isNone())
      newCSPSrcList = CSPSourceList.fromString("'none'");

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

    // if either was explicit, so is this.
    newCSPSrcList._isImplicit = this._isImplicit && that._isImplicit;

    return newCSPSrcList;
  }
}

//////////////////////////////////////////////////////////////////////
/**
 * Class to model a source (scheme, host, port)
 */
function CSPSource() {
  this._scheme = undefined;
  this._port = undefined;
  this._host = undefined;

  // when set to true, this source represents 'self'
  this._isSelf = false;
}

/**
 * General factory method to create a new source from one of the following
 * types:
 *  - nsURI
 *  - string
 *  - CSPSource (clone)
 * @param aData 
 *        string, nsURI, or CSPSource
 * @param self (optional)
 *	  if present, string, URI, or CSPSource representing the "self" resource 
 * @param enforceSelfChecks (optional)
 *	  if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from the source.
 * @returns
 *        an instance of CSPSource
 */
CSPSource.create = function(aData, self, enforceSelfChecks) {
  if (typeof aData === 'string')
    return CSPSource.fromString(aData, self, enforceSelfChecks);

  if (aData instanceof Components.interfaces.nsIURI)
    return CSPSource.fromURI(aData, self, enforceSelfChecks);

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
 * @param self (optional)
 *        string or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from aURI.
 * @returns
 *        an instance of CSPSource 
 */
CSPSource.fromURI = function(aURI, self, enforceSelfChecks) {
  if (!(aURI instanceof Components.interfaces.nsIURI)){
    CSPError("Provided argument is not an nsIURI");
    return null;
  }

  if (!self && enforceSelfChecks) {
    CSPError("Can't use 'self' if self data is not provided");
    return null;
  }

  if (self && !(self instanceof CSPSource)) {
    self = CSPSource.create(self, undefined, false);
  }

  var sObj = new CSPSource();
  sObj._self = self;

  // PARSE
  // If 'self' is undefined, then use default port for scheme if there is one.

  // grab scheme (if there is one)
  try {
    sObj._scheme = aURI.scheme;
  } catch(e) {
    sObj._scheme = undefined;
    CSPError("can't parse a URI without a scheme: " + aURI.asciiSpec);
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
 * @param self (optional)
 *        string, URI, or CSPSource representing the "self" source
 * @param enforceSelfChecks (optional)
 *        if present, and "true", will check to be sure "self" has the
 *        appropriate values to inherit when they are omitted from aURI.
 * @returns
 *        an instance of CSPSource 
 */
CSPSource.fromString = function(aStr, self, enforceSelfChecks) {
  if (!aStr)
    return null;

  if (!(typeof aStr === 'string')) {
    CSPError("Provided argument is not a string");
    return null;
  }

  if (!self && enforceSelfChecks) {
    CSPError("Can't use 'self' if self data is not provided");
    return null;
  }

  if (self && !(self instanceof CSPSource)) {
    self = CSPSource.create(self, undefined, false);
  }

  var sObj = new CSPSource();
  sObj._self = self;

  // take care of 'self' keyword
  if (aStr === "'self'") {
    if (!self) {
      CSPError("self keyword used, but no self data specified");
      return null;
    }
    sObj._self = self.clone();
    sObj._isSelf = true;
    return sObj;
  }

  // We could just create a URI and then send this off to fromURI, but
  // there's no way to leave out the scheme or wildcard the port in an nsURI.
  // That has to be supported here.

  // split it up
  var chunks = aStr.split(":");

  // If there is only one chunk, it's gotta be a host.
  if (chunks.length == 1) {
    sObj._host = CSPHost.fromString(chunks[0]);
    if (!sObj._host) {
      CSPError("Couldn't parse invalid source " + aStr);
      return null;
    }

    // enforce 'self' inheritance
    if (enforceSelfChecks) {
      // note: the non _scheme accessor checks sObj._self
      if (!sObj.scheme || !sObj.port) {
        CSPError("Can't create host-only source " + aStr + " without 'self' data");
        return null;
      }
    }
    return sObj;
  }

  // If there are two chunks, it's either scheme://host or host:port
  //   ... but scheme://host can have an empty host.
  //   ... and host:port can have an empty host
  if (chunks.length == 2) {

    // is the last bit a port?
    if (chunks[1] === "*" || chunks[1].match(/^\d+$/)) {
      sObj._port = chunks[1];
      // then the previous chunk *must* be a host or empty.
      if (chunks[0] !== "") {
        sObj._host = CSPHost.fromString(chunks[0]);
        if (!sObj._host) {
          CSPError("Couldn't parse invalid source " + aStr);
          return null;
        }
      }
      // enforce 'self' inheritance 
      // (scheme:host requires port, host:port does too.  Wildcard support is
      // only available if the scheme and host are wildcarded)
      if (enforceSelfChecks) {
        // note: the non _scheme accessor checks sObj._self
        if (!sObj.scheme || !sObj.host || !sObj.port) {
          CSPError("Can't create source " + aStr + " without 'self' data");
          return null;
        }
      }
    }
    // is the first bit a scheme?
    else if (CSPSource.validSchemeName(chunks[0])) {
      sObj._scheme = chunks[0];
      // then the second bit *must* be a host or empty
      if (chunks[1] === "") {
        // Allow scheme-only sources!  These default to wildcard host/port,
        // especially since host and port don't always matter.
        // Example: "javascript:" and "data:" 
        if (!sObj._host) sObj._host = CSPHost.fromString("*");
        if (!sObj._port) sObj._port = "*";
      } else {
        // some host was defined.
        // ... remove <= 3 leading slashes (from the scheme) and parse
        var cleanHost = chunks[1].replace(/^\/{0,3}/,"");
        // ... and parse
        sObj._host = CSPHost.fromString(cleanHost);
        if (!sObj._host) {
          CSPError("Couldn't parse invalid host " + cleanHost);
          return null;
        }
      }

      // enforce 'self' inheritance (scheme-only should be scheme:*:* now, and
      // if there was a host provided it should be scheme:host:selfport
      if (enforceSelfChecks) {
        // note: the non _scheme accessor checks sObj._self
        if (!sObj.scheme || !sObj.host || !sObj.port) {
          CSPError("Can't create source " + aStr + " without 'self' data");
          return null;
        }
      }
    }
    else  {
      // AAAH!  Don't know what to do!  No valid scheme or port!
      CSPError("Couldn't parse invalid source " + aStr);
      return null;
    }

    return sObj;
  }

  // If there are three chunks, we got 'em all!
  if (!CSPSource.validSchemeName(chunks[0])) {
    CSPError("Couldn't parse scheme in " + aStr);
    return null;
  }
  sObj._scheme = chunks[0];
  if (!(chunks[2] === "*" || chunks[2].match(/^\d+$/))) {
    CSPError("Couldn't parse port in " + aStr);
    return null;
  }

  sObj._port = chunks[2];

  // ... remove <= 3 leading slashes (from the scheme) and parse
  var cleanHost = chunks[1].replace(/^\/{0,3}/,"");
  sObj._host = CSPHost.fromString(cleanHost);

  return sObj._host ? sObj : null;
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
      return this.permits(CSPSource.create(aSource));

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
    if (!this._port)
      newSource._port = that._port;
    else if (!that._port)
      newSource._port = this._port;
    else if (this._port === "*") 
      newSource._port = that._port;
    else if (that._port === "*")
      newSource._port = this._port;
    else if (that._port === this._port)
      newSource._port = this._port;
    else {
      CSPError("Could not intersect " + this + " with " + that
               + " due to port problems.");
      return null;
    }

    // scheme
    if (!this._scheme)
      newSource._scheme = that._scheme;
    else if (!that._scheme)
      newSource._scheme = this._scheme;
    if (this._scheme === "*")
      newSource._scheme = that._scheme;
    else if (that._scheme === "*")
      newSource._scheme = this._scheme;
    else if (that._scheme === this._scheme)
      newSource._scheme = this._scheme;
    else {
      CSPError("Could not intersect " + this + " with " + that
               + " due to scheme problems.");
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
    if (this._host && that._host) {
      newSource._host = this._host.intersectWith(that._host);
    } else if (this._host) {
      CSPError("intersecting source with undefined host: " + that.toString());
      newSource._host = this._host.clone();
    } else if (that._host) {
      CSPError("intersecting source with undefined host: " + this.toString());
      newSource._host = that._host.clone();
    } else {
      CSPError("intersecting two sources with undefined hosts: " +
               this.toString() + " and " + that.toString());
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
function CSPHost() {
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
      return this.permits(CSPHost.fromString(aHost));
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
