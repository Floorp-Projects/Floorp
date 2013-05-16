/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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

// Needed to support CSP 1.0 spec and our original CSP implementation - should
// be removed when our original implementation is deprecated.
const CSP_TYPE_XMLHTTPREQUEST_SPEC_COMPLIANT = "csp_type_xmlhttprequest_spec_compliant";
const CSP_TYPE_WEBSOCKET_SPEC_COMPLIANT = "csp_type_websocket_spec_compliant";

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
  this._requestOrigin = "";
  this._requestPrincipal = "";
  this._referrer = "";
  this._docRequest = null;
  CSPdebug("CSP POLICY INITED TO 'default-src *'");

  this._cache = { };
}

/*
 * Set up mappings from nsIContentPolicy content types to CSP directives.
 */
{
  let cp = Ci.nsIContentPolicy;
  let csp = ContentSecurityPolicy;
  let cspr_sd_old = CSPRep.SRC_DIRECTIVES_OLD;
  let cspr_sd_new = CSPRep.SRC_DIRECTIVES_NEW;

  csp._MAPPINGS=[];

  /* default, catch-all case */
  // This is the same in old and new CSP so use the new mapping.
  csp._MAPPINGS[cp.TYPE_OTHER]             =  cspr_sd_new.DEFAULT_SRC;

  /* self */
  csp._MAPPINGS[cp.TYPE_DOCUMENT]          =  null;

  /* shouldn't see this one */
  csp._MAPPINGS[cp.TYPE_REFRESH]           =  null;

  /* categorized content types */
  // These are the same in old and new CSP's so just use the new mappings.
  csp._MAPPINGS[cp.TYPE_SCRIPT]            = cspr_sd_new.SCRIPT_SRC;
  csp._MAPPINGS[cp.TYPE_IMAGE]             = cspr_sd_new.IMG_SRC;
  csp._MAPPINGS[cp.TYPE_STYLESHEET]        = cspr_sd_new.STYLE_SRC;
  csp._MAPPINGS[cp.TYPE_OBJECT]            = cspr_sd_new.OBJECT_SRC;
  csp._MAPPINGS[cp.TYPE_OBJECT_SUBREQUEST] = cspr_sd_new.OBJECT_SRC;
  csp._MAPPINGS[cp.TYPE_SUBDOCUMENT]       = cspr_sd_new.FRAME_SRC;
  csp._MAPPINGS[cp.TYPE_MEDIA]             = cspr_sd_new.MEDIA_SRC;
  csp._MAPPINGS[cp.TYPE_FONT]              = cspr_sd_new.FONT_SRC;

  /* Our original CSP implementation's mappings for XHR and websocket
   * These should be changed to be = cspr_sd.CONNECT_SRC when we remove
   * the original implementation - NOTE: order in this array is important !!!
   */
  csp._MAPPINGS[cp.TYPE_XMLHTTPREQUEST]    = cspr_sd_old.XHR_SRC;
  csp._MAPPINGS[cp.TYPE_WEBSOCKET]         = cspr_sd_old.XHR_SRC;

  /* CSP cannot block CSP reports */
  csp._MAPPINGS[cp.TYPE_CSP_REPORT]        = null;

  /* These must go through the catch-all */
  csp._MAPPINGS[cp.TYPE_XBL]               = cspr_sd_new.DEFAULT_SRC;
  csp._MAPPINGS[cp.TYPE_PING]              = cspr_sd_new.DEFAULT_SRC;
  csp._MAPPINGS[cp.TYPE_DTD]               = cspr_sd_new.DEFAULT_SRC;

  /* CSP 1.0 spec compliant mappings for XHR and websocket */
  // The directive name for XHR, websocket, and EventSource is different
  // in the 1.0 spec than in our original implementation, these mappings
  // address this. These won't be needed when we deprecate our original
  // implementation.
  csp._MAPPINGS[CSP_TYPE_XMLHTTPREQUEST_SPEC_COMPLIANT]    = cspr_sd_new.CONNECT_SRC;
  csp._MAPPINGS[CSP_TYPE_WEBSOCKET_SPEC_COMPLIANT]         = cspr_sd_new.CONNECT_SRC;
  // TODO : EventSource will be here and also will use connect-src
  // after we fix Bug 802872 - CSP should restrict EventSource using the connect-src
  // directive. For background see Bug 667490 - EventSource should use the same
  // nsIContentPolicy type as XHR (which is fixed)
}

ContentSecurityPolicy.prototype = {
  classID:          Components.ID("{d1680bb4-1ac0-4772-9437-1188375e44f2}"),
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

  getAllowsInlineScript: function(shouldReportViolation) {
    // report it?
    shouldReportViolation.value = !this._policy.allowsInlineScripts;

    // allow it to execute?
    return this._reportOnlyMode || this._policy.allowsInlineScripts;
  },

  getAllowsEval: function(shouldReportViolation) {
    // report it?
    shouldReportViolation.value = !this._policy.allowsEvalInScripts;

    // allow it to execute?
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
        this._asyncReportViolation('self',null,'inline script base restriction',
                                   'violated base restriction: Inline Scripts will not execute',
                                   aSourceFile, aScriptSample, aLineNum);
      break;
    case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL:
      if (!this._policy.allowsEvalInScripts)
        this._asyncReportViolation('self',null,'eval script base restriction',
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

    // Save the docRequest for fetching a policy-uri
    this._docRequest = aChannel;

    // save the document URI (minus <fragment>) and referrer for reporting
    let uri = aChannel.URI.cloneIgnoringRef();
    try { // GetUserPass throws for some protocols without userPass
      uri.userPass = '';
    } catch (ex) {}
    this._request = uri.asciiSpec;
    this._requestOrigin = uri;

    //store a reference to the principal, that can later be used in shouldLoad
    this._requestPrincipal = Components.classes["@mozilla.org/scriptsecuritymanager;1"].
    getService(Components.interfaces.nsIScriptSecurityManager).getChannelPrincipal(aChannel);

    if (aChannel.referrer) {
      let referrer = aChannel.referrer.cloneIgnoringRef();
      try { // GetUserPass throws for some protocols without userPass
        referrer.userPass = '';
      } catch (ex) {}
      this._referrer = referrer.asciiSpec;
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
  function csp_refinePolicy(aPolicy, selfURI, aSpecCompliant) {
    CSPdebug("REFINE POLICY: " + aPolicy);
    CSPdebug("         SELF: " + selfURI.asciiSpec);
    CSPdebug("CSP 1.0 COMPLIANT : " + aSpecCompliant);
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

    // If we want to be CSP 1.0 spec compliant, use the new parser.
    // The old one will be deprecated in the future and will be
    // removed at that time.
    var newpolicy;
    if (aSpecCompliant) {
      newpolicy = CSPRep.fromStringSpecCompliant(aPolicy,
                                                 selfURI,
                                                 this._docRequest,
                                                 this);
    } else {
      newpolicy = CSPRep.fromString(aPolicy,
                                    selfURI,
                                    this._docRequest,
                                    this);
    }

    // (2) Intersect the currently installed CSPRep object with the new one
    var intersect = this._policy.intersectWith(newpolicy);

    // (3) Save the result
    this._policy = intersect;

    this._policy._specCompliant = !!aSpecCompliant;

    this._isInitialized = true;
    this._cache = {};
  },

  /**
   * Generates and sends a violation report to the specified report URIs.
   */
  sendReports:
  function(blockedUri, originalUri, violatedDirective,
           aSourceFile, aScriptSample, aLineNum) {
    var uriString = this._policy.getReportURIs();
    var uris = uriString.split(/\s+/);
    if (uris.length > 0) {
      // see if we need to sanitize the blocked-uri
      let blocked = '';
      if (originalUri) {
        // We've redirected, only report the blocked origin
        let clone = blockedUri.clone();
        clone.path = '';
        blocked = clone.asciiSpec;
      }
      else if (blockedUri instanceof Ci.nsIURI) {
        blocked = blockedUri.cloneIgnoringRef().asciiSpec;
      }
      else {
        // blockedUri is a string for eval/inline-script violations
        blocked = blockedUri;
      }

      // Generate report to send composed of
      // {
      //   csp-report: {
      //     document-uri: "http://example.com/file.html?params",
      //     referrer: "...",
      //     blocked-uri: "...",
      //     violated-directive: "..."
      //   }
      // }
      var report = {
        'csp-report': {
          'document-uri': this._request,
          'referrer': this._referrer,
          'blocked-uri': blocked,
          'violated-directive': violatedDirective
        }
      }

      // extra report fields for script errors (if available)
      if (originalUri)
        report["csp-report"]["original-uri"] = originalUri.cloneIgnoringRef().asciiSpec;
      if (aSourceFile)
        report["csp-report"]["source-file"] = aSourceFile;
      if (aScriptSample)
        report["csp-report"]["script-sample"] = aScriptSample;
      if (aLineNum)
        report["csp-report"]["line-number"] = aLineNum;

      var reportString = JSON.stringify(report);
      CSPdebug("Constructed violation report:\n" + reportString);

      var violationMessage = null;
      if (blockedUri["asciiSpec"]) {
         violationMessage = CSPLocalizer.getFormatStr("directiveViolatedWithURI", [violatedDirective, blockedUri.asciiSpec]);
      } else {
         violationMessage = CSPLocalizer.getFormatStr("directiveViolated", [violatedDirective]);
      }
      this._policy.warn(violationMessage,
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

        try {
          var chan = Services.io.newChannel(uris[i], null, null);
          if (!chan) {
            CSPdebug("Error creating channel for " + uris[i]);
            continue;
          }

          var content = Cc["@mozilla.org/io/string-input-stream;1"]
                          .createInstance(Ci.nsIStringInputStream);
          content.data = reportString + "\n\n";

          // make sure this is an anonymous request (no cookies) so in case the
          // policy URI is injected, it can't be abused for CSRF.
          chan.loadFlags |= Ci.nsIChannel.LOAD_ANONYMOUS;

          // we need to set an nsIChannelEventSink on the channel object
          // so we can tell it to not follow redirects when posting the reports
          chan.notificationCallbacks = new CSPReportRedirectSink(this._policy);
          if (this._docRequest) {
            chan.loadGroup = this._docRequest.loadGroup;
          }

          chan.QueryInterface(Ci.nsIUploadChannel)
              .setUploadStream(content, "application/json", content.available());

          try {
            // if this is an HTTP channel, set the request method to post
            chan.QueryInterface(Ci.nsIHttpChannel);
            chan.requestMethod = "POST";
          } catch(e) {} // throws only if chan is not an nsIHttpChannel.

          // check with the content policy service to see if we're allowed to
          // send this request.
          try {
            var contentPolicy = Cc["@mozilla.org/layout/content-policy;1"]
                                  .getService(Ci.nsIContentPolicy);
            if (contentPolicy.shouldLoad(Ci.nsIContentPolicy.TYPE_CSP_REPORT,
                                         chan.URI, this._requestOrigin,
                                         null, null, null, this._requestPrincipal)
                != Ci.nsIContentPolicy.ACCEPT) {
              continue; // skip unauthorized URIs
            }
          } catch(e) {
            continue; // refuse to load if we can't do a security check.
          }

          //send data (and set up error notifications)
          chan.asyncOpen(new CSPViolationReportListener(uris[i]), null);
          CSPdebug("Sent violation report to " + uris[i]);
        } catch(e) {
          // it's possible that the URI was invalid, just log a
          // warning and skip over that.
          this._policy.warn(CSPLocalizer.getFormatStr("triedToSendReport", [uris[i]]));
          this._policy.warn(CSPLocalizer.getFormatStr("errorWas", [e.toString()]));
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
        // delete any userpass
        let ancestor = it.currentURI.cloneIgnoringRef();
        try { // GetUserPass throws for some protocols without userPass
          ancestor.userPass = '';
        } catch (ex) {}

        CSPdebug(" found frame ancestor " + ancestor.asciiSpec);
        ancestors.push(ancestor);
      }
    }

    // scan the discovered ancestors
    // frame-ancestors is the same in both old and new source directives,
    // so don't need to differentiate here.
    let cspContext = CSPRep.SRC_DIRECTIVES_NEW.FRAME_ANCESTORS;
    for (let i in ancestors) {
      let ancestor = ancestors[i];
      if (!this._policy.permits(ancestor, cspContext)) {
        // report the frame-ancestor violation
        let directive = this._policy._directives[cspContext];
        let violatedPolicy = (directive._isImplicit
                                ? 'default-src' : 'frame-ancestors ')
                                + directive.toString();

        this._asyncReportViolation(ancestors[i], null, violatedPolicy);

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
                          aOriginalUri) {
    let key = aContentLocation.spec + "!" + aContentType;
    if (this._cache[key]) {
      return this._cache[key];
    }

#ifndef MOZ_B2G
    // Try to remove as much as possible from the hot path on b2g.
    CSPdebug("shouldLoad location = " + aContentLocation.asciiSpec);
    CSPdebug("shouldLoad content type = " + aContentType);
#endif
    // interpret the context, and then pass off to the decision structure
    var cspContext = ContentSecurityPolicy._MAPPINGS[aContentType];

    // The mapping for XHR and websockets is different between our original
    // implementation and the 1.0 spec, we handle this here.
    var cspContext;

    let cp = Ci.nsIContentPolicy;

#ifndef MOZ_B2G
    CSPdebug("policy is " + (this._policy._specCompliant ?
                             "1.0 compliant" : "pre-1.0"));
#endif

    if (aContentType == cp.TYPE_XMLHTTPREQUEST && this._policy._specCompliant) {
      cspContext = ContentSecurityPolicy._MAPPINGS[CSP_TYPE_XMLHTTPREQUEST_SPEC_COMPLIANT];
    } else if (aContentType == cp.TYPE_WEBSOCKET && this._policy._specCompliant) {
      cspContext = ContentSecurityPolicy._MAPPINGS[CSP_TYPE_WEBSOCKET_SPEC_COMPLIANT];
    } else {
      cspContext = ContentSecurityPolicy._MAPPINGS[aContentType];
    }

    CSPdebug("shouldLoad cspContext = " + cspContext);

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
        this._asyncReportViolation(aContentLocation, aOriginalUri, violatedPolicy);
      } catch(e) {
        CSPdebug('---------------- ERROR: ' + e);
      }
    }

    let ret = this._cache[key] =
      (this._reportOnlyMode ? Ci.nsIContentPolicy.ACCEPT : res);
    return ret;
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
   * @param originalUri
   *        The original URI if the blocked content is a redirect, else null
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
  function(blockedContentSource, originalUri, violatedDirective, observerSubject,
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
        reportSender.sendReports(blockedContentSource, originalUri,
                                 violatedDirective,
                                 aSourceFile, aScriptSample, aLineNum);
      }, Ci.nsIThread.DISPATCH_NORMAL);
  },
};

// The POST of the violation report (if it happens) should not follow
// redirects, per the spec. hence, we implement an nsIChannelEventSink
// with an object so we can tell XHR to abort if a redirect happens.
function CSPReportRedirectSink(policy) {
  this._policy = policy;
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
    this._policy.warn(CSPLocalizer.getFormatStr("reportPostRedirect", [oldChannel.URI.asciiSpec]));

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

this.NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentSecurityPolicy]);
