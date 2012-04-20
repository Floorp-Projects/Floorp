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
 * The Original Code is the ContentSecurityPolicy module.
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Corporation
 *
 * Contributor(s):
 *   Sid Stamm <sid@mozilla.com>
 *   Brandon Sterne <bsterne@mozilla.com>
 *   Ian Melven <imelven@mozilla.com>
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
 * Content Security Policy
 *
 * Overview
 * This is a stub component that will be fleshed out to do all the fancy stuff
 * that ContentSecurityPolicy has to do.
 */

/* :::::::: Constants and Helpers ::::::::::::::: */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

const CSP_VIOLATION_TOPIC = "csp-on-violate-policy";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/CSPUtils.jsm");

/* ::::: Policy Parsing & Data structures :::::: */

function ContentSecurityPolicy() {
  CSPdebug("CSP CREATED");
  this._isInitialized = false;
  this._reportOnlyMode = false;
  this._policy = CSPRep.fromString("default-src *");

  // default options "wide open" since this policy will be intersected soon
  this._policy._allowInlineScripts = true;
  this._policy._allowEval = true;

  this._request = "";
  this._docRequest = null;
  CSPdebug("CSP POLICY INITED TO 'default-src *'");
}

/*
 * Set up mappings from nsIContentPolicy content types to CSP directives.
 */
{
  let cp = Ci.nsIContentPolicy;
  let csp = ContentSecurityPolicy;
  let cspr_sd = CSPRep.SRC_DIRECTIVES;

  csp._MAPPINGS=[];

  /* default, catch-all case */
  csp._MAPPINGS[cp.TYPE_OTHER]             =  cspr_sd.DEFAULT_SRC;

  /* self */
  csp._MAPPINGS[cp.TYPE_DOCUMENT]          =  null;

  /* shouldn't see this one */
  csp._MAPPINGS[cp.TYPE_REFRESH]           =  null;

  /* categorized content types */
  csp._MAPPINGS[cp.TYPE_SCRIPT]            = cspr_sd.SCRIPT_SRC;
  csp._MAPPINGS[cp.TYPE_IMAGE]             = cspr_sd.IMG_SRC;
  csp._MAPPINGS[cp.TYPE_STYLESHEET]        = cspr_sd.STYLE_SRC;
  csp._MAPPINGS[cp.TYPE_OBJECT]            = cspr_sd.OBJECT_SRC;
  csp._MAPPINGS[cp.TYPE_OBJECT_SUBREQUEST] = cspr_sd.OBJECT_SRC;
  csp._MAPPINGS[cp.TYPE_SUBDOCUMENT]       = cspr_sd.FRAME_SRC;
  csp._MAPPINGS[cp.TYPE_MEDIA]             = cspr_sd.MEDIA_SRC;
  csp._MAPPINGS[cp.TYPE_FONT]              = cspr_sd.FONT_SRC;
  csp._MAPPINGS[cp.TYPE_XMLHTTPREQUEST]    = cspr_sd.XHR_SRC;
  csp._MAPPINGS[cp.TYPE_WEBSOCKET]         = cspr_sd.XHR_SRC;


  /* These must go through the catch-all */
  csp._MAPPINGS[cp.TYPE_XBL]               = cspr_sd.DEFAULT_SRC;
  csp._MAPPINGS[cp.TYPE_PING]              = cspr_sd.DEFAULT_SRC;
  csp._MAPPINGS[cp.TYPE_DTD]               = cspr_sd.DEFAULT_SRC;
}

ContentSecurityPolicy.prototype = {
  classID:          Components.ID("{AB36A2BF-CB32-4AA6-AB41-6B4E4444A221}"),
  QueryInterface:   XPCOMUtils.generateQI([Ci.nsIContentSecurityPolicy]),

  get isInitialized() {
    return this._isInitialized;
  },

  set isInitialized (foo) {
    this._isInitialized = foo;
  },

  get policy () {
    return this._policy.toString();
  },

  get allowsInlineScript() {
    return this._reportOnlyMode || this._policy.allowsInlineScripts;
  },

  get allowsEval() {
    return this._reportOnlyMode || this._policy.allowsEvalInScripts;
  },

  /**
   * Log policy violation on the Error Console and send a report if a report-uri
   * is present in the policy
   *
   * @param aViolationType
   *     one of the VIOLATION_TYPE_* constants, e.g. inline-script or eval
   * @param aSourceFile
   *     name of the source file containing the violation (if available)
   * @param aContentSample
   *     sample of the violating content (to aid debugging)
   * @param aLineNum
   *     source line number of the violation (if available)
   */
  logViolationDetails:
  function(aViolationType, aSourceFile, aScriptSample, aLineNum) {
    // allowsInlineScript and allowsEval both return true when report-only mode
    // is enabled, resulting in a call to this function. Therefore we need to
    // check that the policy was in fact violated before logging any violations
    switch (aViolationType) {
    case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_SCRIPT:
      if (!this._policy.allowsInlineScripts)
        this._asyncReportViolation('self','inline script base restriction',
                                   'violated base restriction: Inline Scripts will not execute',
                                   aSourceFile, aScriptSample, aLineNum);
      break;
    case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL:
      if (!this._policy.allowsEvalInScripts)
        this._asyncReportViolation('self','eval script base restriction',
                                   'violated base restriction: Code will not be created from strings',
                                   aSourceFile, aScriptSample, aLineNum);
      break;
    }
  },

  set reportOnlyMode(val) {
    this._reportOnlyMode = val;
  },

  get reportOnlyMode () {
    return this._reportOnlyMode;
  },

  /*
  // Having a setter is a bad idea... opens up the policy to "loosening"
  // Instead, use "refinePolicy."
  set policy (aStr) {
    this._policy = CSPRep.fromString(aStr);
  },
  */

  /**
   * Given an nsIHttpChannel, fill out the appropriate data.
   */
  scanRequestData:
  function(aChannel) {
    if (!aChannel)
      return;
    // grab the request line
    var internalChannel = null;
    try {
      internalChannel = aChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    } catch (e) {
      CSPdebug("No nsIHttpChannelInternal for " + aChannel.URI.asciiSpec);
    }

    this._request = aChannel.requestMethod + " " + aChannel.URI.asciiSpec;
    this._docRequest = aChannel;

    // We will only be able to provide the HTTP version information if aChannel
    // implements nsIHttpChannelInternal
    if (internalChannel) {
      var reqMaj = {};
      var reqMin = {};
      var reqVersion = internalChannel.getRequestVersion(reqMaj, reqMin);
      this._request += " HTTP/" + reqMaj.value + "." + reqMin.value;
    }
  },

/* ........ Methods .............. */

  /**
   * Given a new policy, intersects the currently enforced policy with the new
   * one and stores the result.  The effect is a "tightening" or refinement of
   * an old policy.  This is called any time a new policy is encountered and
   * the effective policy has to be refined.
   */
  refinePolicy:
  function csp_refinePolicy(aPolicy, selfURI) {
    CSPdebug("REFINE POLICY: " + aPolicy);
    CSPdebug("         SELF: " + selfURI.asciiSpec);
    // For nested schemes such as view-source: make sure we are taking the
    // innermost URI to use as 'self' since that's where we will extract the
    // scheme, host and port from
    if (selfURI instanceof Ci.nsINestedURI) {
      CSPdebug("        INNER: " + selfURI.innermostURI.asciiSpec);
      selfURI = selfURI.innermostURI;
    }

    // stay uninitialized until policy merging is done
    this._isInitialized = false;

    // If there is a policy-uri, fetch the policy, then re-call this function.
    // (1) parse and create a CSPRep object
    // Note that we pass the full URI since when it's parsed as 'self' to construct a 
    // CSPSource only the scheme, host, and port are kept. 
    var newpolicy = CSPRep.fromString(aPolicy,
				      selfURI,
                                      this._docRequest,
                                      this);

    // (2) Intersect the currently installed CSPRep object with the new one
    var intersect = this._policy.intersectWith(newpolicy);
 
    // (3) Save the result
    this._policy = intersect;
    this._isInitialized = true;
  },

  /**
   * Generates and sends a violation report to the specified report URIs.
   */
  sendReports:
  function(blockedUri, violatedDirective, aSourceFile, aScriptSample, aLineNum) {
    var uriString = this._policy.getReportURIs();
    var uris = uriString.split(/\s+/);
    if (uris.length > 0) {
      // Generate report to send composed of
      // {
      //   csp-report: {
      //     request: "GET /index.html HTTP/1.1",
      //     blocked-uri: "...",
      //     violated-directive: "..."
      //   }
      // }
      var report = {
        'csp-report': {
          'request': this._request,
          'blocked-uri': (blockedUri instanceof Ci.nsIURI ?
                          blockedUri.asciiSpec : blockedUri),
          'violated-directive': violatedDirective
        }
      }
      // extra report fields for script errors (if available)
      if (aSourceFile)
        report["csp-report"]["source-file"] = aSourceFile;
      if (aScriptSample)
        report["csp-report"]["script-sample"] = aScriptSample;
      if (aLineNum)
        report["csp-report"]["line-number"] = aLineNum;

      CSPdebug("Constructed violation report:\n" + JSON.stringify(report));

      CSPWarning("Directive \"" + violatedDirective + "\" violated"
               + (blockedUri['asciiSpec'] ? " by " + blockedUri.asciiSpec : ""),
                 (aSourceFile) ? aSourceFile : null,
                 (aScriptSample) ? decodeURIComponent(aScriptSample) : null,
                 (aLineNum) ? aLineNum : null);

      // For each URI in the report list, send out a report.
      // We make the assumption that all of the URIs are absolute URIs; this
      // should be taken care of in CSPRep.fromString (where it converts any
      // relative URIs into absolute ones based on "self").
      for (let i in uris) {
        if (uris[i] === "")
          continue;

        var failure = function(aEvt) {  
          if (req.readyState == 4 && req.status != 200) {
            CSPError("Failed to send report to " + uris[i]);
          }  
        };  
        var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]  
                    .createInstance(Ci.nsIXMLHttpRequest);  

        try {
          req.open("POST", uris[i], true);
          req.setRequestHeader('Content-Type', 'application/json');
          req.upload.addEventListener("error", failure, false);
          req.upload.addEventListener("abort", failure, false);

          // we need to set an nsIChannelEventSink on the XHR object
          // so we can tell it to not follow redirects when posting the reports
          req.channel.notificationCallbacks = new CSPReportRedirectSink();

          req.send(JSON.stringify(report));
          CSPdebug("Sent violation report to " + uris[i]);
        } catch(e) {
          // it's possible that the URI was invalid, just log a
          // warning and skip over that.
          CSPWarning("Tried to send report to invalid URI: \"" + uris[i] + "\"");
        }
      }
    }
  },

  /**
   * Exposed Method to analyze docShell for approved frame ancestry.
   * Also sends violation reports if necessary.
   * @param docShell
   *    the docShell for this policy's resource.
   * @return
   *    true if the frame ancestry is allowed by this policy.
   */
  permitsAncestry:
  function(docShell) {
    if (!docShell) { return false; }
    CSPdebug(" in permitsAncestry(), docShell = " + docShell);

    // walk up this docShell tree until we hit chrome
    var dst = docShell.QueryInterface(Ci.nsIInterfaceRequestor)
                      .getInterface(Ci.nsIDocShellTreeItem);

    // collect ancestors and make sure they're allowed.
    var ancestors = [];
    while (dst.parent) {
      dst = dst.parent;
      let it = dst.QueryInterface(Ci.nsIInterfaceRequestor)
                  .getInterface(Ci.nsIWebNavigation);
      if (it.currentURI) {
        if (it.currentURI.scheme === "chrome") {
          break;
        }
        let ancestor = it.currentURI;
        CSPdebug(" found frame ancestor " + ancestor.asciiSpec);
        ancestors.push(ancestor);
      }
    } 

    // scan the discovered ancestors
    let cspContext = CSPRep.SRC_DIRECTIVES.FRAME_ANCESTORS;
    for (let i in ancestors) {
      let ancestor = ancestors[i].prePath;
      if (!this._policy.permits(ancestor, cspContext)) {
        // report the frame-ancestor violation
        let directive = this._policy._directives[cspContext];
        let violatedPolicy = (directive._isImplicit
                                ? 'default-src' : 'frame-ancestors ')
                                + directive.toString();

        this._asyncReportViolation(ancestors[i], violatedPolicy);

        // need to lie if we are testing in report-only mode
        return this._reportOnlyMode;
      }
    }
    return true;
  },

  /**
   * Delegate method called by the service when sub-elements of the protected
   * document are being loaded.  Given a bit of information about the request,
   * decides whether or not the policy is satisfied.
   */
  shouldLoad:
  function csp_shouldLoad(aContentType, 
                          aContentLocation, 
                          aRequestOrigin, 
                          aContext, 
                          aMimeTypeGuess, 
                          aExtra) {

    // don't filter chrome stuff
    if (aContentLocation.scheme === 'chrome' ||
        aContentLocation.scheme === 'resource') {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    // interpret the context, and then pass off to the decision structure
    CSPdebug("shouldLoad location = " + aContentLocation.asciiSpec);
    CSPdebug("shouldLoad content type = " + aContentType);
    var cspContext = ContentSecurityPolicy._MAPPINGS[aContentType];

    // if the mapping is null, there's no policy, let it through.
    if (!cspContext) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    // otherwise, honor the translation
    // var source = aContentLocation.scheme + "://" + aContentLocation.hostPort; 
    var res = this._policy.permits(aContentLocation, cspContext)
              ? Ci.nsIContentPolicy.ACCEPT 
              : Ci.nsIContentPolicy.REJECT_SERVER;

    // frame-ancestors is taken care of early on (as this document is loaded)

    // If the result is *NOT* ACCEPT, then send report
    if (res != Ci.nsIContentPolicy.ACCEPT) { 
      CSPdebug("blocking request for " + aContentLocation.asciiSpec);
      try {
        let directive = this._policy._directives[cspContext];
        let violatedPolicy = (directive._isImplicit
                                ? 'default-src' : cspContext)
                                + ' ' + directive.toString();
        this._asyncReportViolation(aContentLocation, violatedPolicy);
      } catch(e) {
        CSPdebug('---------------- ERROR: ' + e);
      }
    }

    return (this._reportOnlyMode ? Ci.nsIContentPolicy.ACCEPT : res);
  },
  
  shouldProcess:
  function csp_shouldProcess(aContentType,
                             aContentLocation,
                             aRequestOrigin,
                             aContext,
                             aMimeType,
                             aExtra) {
    // frame-ancestors check is done outside the ContentPolicy
    var res = Ci.nsIContentPolicy.ACCEPT;
    CSPdebug("shouldProcess aContext=" + aContext);
    return res;
  },

  /**
   * Asynchronously notifies any nsIObservers listening to the CSP violation
   * topic that a violation occurred.  Also triggers report sending.  All
   * asynchronous on the main thread.
   *
   * @param blockedContentSource
   *        Either a CSP Source (like 'self', as string) or nsIURI: the source
   *        of the violation.
   * @param violatedDirective
   *        the directive that was violated (string).
   * @param observerSubject
   *        optional, subject sent to the nsIObservers listening to the CSP
   *        violation topic.
   * @param aSourceFile
   *        name of the file containing the inline script violation
   * @param aScriptSample
   *        a sample of the violating inline script
   * @param aLineNum
   *        source line number of the violation (if available)
   */
  _asyncReportViolation:
  function(blockedContentSource, violatedDirective, observerSubject,
           aSourceFile, aScriptSample, aLineNum) {
    // if optional observerSubject isn't specified, default to the source of
    // the violation.
    if (!observerSubject)
      observerSubject = blockedContentSource;

    // gotta wrap things that aren't nsISupports, since it's sent out to
    // observers as such.  Objects that are not nsISupports are converted to
    // strings and then wrapped into a nsISupportsCString.
    if (!(observerSubject instanceof Ci.nsISupports)) {
      let d = observerSubject;
      observerSubject = Cc["@mozilla.org/supports-cstring;1"]
                          .createInstance(Ci.nsISupportsCString);
      observerSubject.data = d;
    }

    var reportSender = this;
    Services.tm.mainThread.dispatch(
      function() {
        Services.obs.notifyObservers(observerSubject,
                                     CSP_VIOLATION_TOPIC,
                                     violatedDirective);
        reportSender.sendReports(blockedContentSource, violatedDirective,
                                 aSourceFile, aScriptSample, aLineNum);
      }, Ci.nsIThread.DISPATCH_NORMAL);
  },
};

// The POST of the violation report (if it happens) should not follow
// redirects, per the spec. hence, we implement an nsIChannelEventSink
// with an object so we can tell XHR to abort if a redirect happens.
function CSPReportRedirectSink() {
}

CSPReportRedirectSink.prototype = {
  QueryInterface: function requestor_qi(iid) {
    if (iid.equals(Ci.nsISupports) ||
        iid.equals(Ci.nsIInterfaceRequestor) ||
        iid.equals(Ci.nsIChannelEventSink))
      return this;
    throw Cr.NS_ERROR_NO_INTERFACE;
  },

  // nsIInterfaceRequestor
  getInterface: function requestor_gi(iid) {
    if (iid.equals(Ci.nsIChannelEventSink))
      return this;

    throw Components.results.NS_ERROR_NO_INTERFACE;
  },

  // nsIChannelEventSink
  asyncOnChannelRedirect: function channel_redirect(oldChannel, newChannel,
                                                    flags, callback) {
    CSPWarning("Post of violation report to " + oldChannel.URI.asciiSpec +
               " failed, as a redirect occurred");

    // cancel the old channel so XHR failure callback happens
    oldChannel.cancel(Cr.NS_ERROR_ABORT);

    // notify an observer that we have blocked the report POST due to a redirect,
    // used in testing, do this async since we're in an async call now to begin with
    Services.tm.mainThread.dispatch(
      function() {
        observerSubject = Cc["@mozilla.org/supports-cstring;1"]
                             .createInstance(Ci.nsISupportsCString);
        observerSubject.data = oldChannel.URI.asciiSpec;

        Services.obs.notifyObservers(observerSubject,
                                     CSP_VIOLATION_TOPIC,
                                     "denied redirect while sending violation report");
      }, Ci.nsIThread.DISPATCH_NORMAL);

    // throw to stop the redirect happening
    throw Cr.NS_BINDING_REDIRECTED;
  }
};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentSecurityPolicy]);
