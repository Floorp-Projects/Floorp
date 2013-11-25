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

const WARN_FLAG = Ci.nsIScriptError.warningFlag;
const ERROR_FLAG = Ci.nsIScriptError.ERROR_FLAG;

const INLINE_STYLE_VIOLATION_OBSERVER_SUBJECT = 'violated base restriction: Inline Stylesheets will not apply';
const INLINE_SCRIPT_VIOLATION_OBSERVER_SUBJECT = 'violated base restriction: Inline Scripts will not execute';
const EVAL_VIOLATION_OBSERVER_SUBJECT = 'violated base restriction: Code will not be created from strings';
const SCRIPT_NONCE_VIOLATION_OBSERVER_SUBJECT = 'Inline Script had invalid nonce'
const STYLE_NONCE_VIOLATION_OBSERVER_SUBJECT = 'Inline Style had invalid nonce'

// The cutoff length of content location in creating CSP cache key.
const CSP_CACHE_URI_CUTOFF_SIZE = 512;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/CSPUtils.jsm");

/* ::::: Policy Parsing & Data structures :::::: */

function ContentSecurityPolicy() {
  CSPdebug("CSP CREATED");
  this._isInitialized = false;

  this._policies = [];

  this._request = "";
  this._requestOrigin = "";
  this._weakRequestPrincipal =  { get : function() { return null; } };
  this._referrer = "";
  this._weakDocRequest = { get : function() { return null; } };
  CSPdebug("CSP object initialized, no policies to enforce yet");

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
  csp._MAPPINGS[cp.TYPE_XSLT]              = cspr_sd_new.SCRIPT_SRC;

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

  _getPolicyInternal: function(index) {
    if (index < 0 || index >= this._policies.length) {
      throw Cr.NS_ERROR_FAILURE;
    }
    return this._policies[index];
  },

  _buildViolatedDirectiveString:
  function(aDirectiveName, aPolicy) {
    var SD = CSPRep.SRC_DIRECTIVES_NEW;
    var cspContext = (SD[aDirectiveName] in aPolicy._directives) ? SD[aDirectiveName] : SD.DEFAULT_SRC;
    var directive = aPolicy._directives[cspContext];
    return cspContext + ' ' + directive.toString();
  },

  /**
   * Returns policy string representing the policy at "index".
   */
  getPolicy: function(index) {
    return this._getPolicyInternal(index).toString();
  },

  /**
   * Returns count of policies.
   */
  get numPolicies() {
    return this._policies.length;
  },

  getAllowsInlineScript: function(shouldReportViolations) {
    // report it? (for each policy, is it violated?)
    shouldReportViolations.value = this._policies.some(function(a) { return !a.allowsInlineScripts; });

    // allow it to execute?  (Do all the policies allow it to execute)?
    return this._policies.every(function(a) {
      return a._reportOnlyMode || a.allowsInlineScripts;
    });
  },

  getAllowsEval: function(shouldReportViolations) {
    // report it? (for each policy, is it violated?)
    shouldReportViolations.value = this._policies.some(function(a) { return !a.allowsEvalInScripts; });

    // allow it to execute?  (Do all the policies allow it to execute)?
    return this._policies.every(function(a) {
      return a._reportOnlyMode || a.allowsEvalInScripts;
    });
  },

  getAllowsInlineStyle: function(shouldReportViolations) {
    // report it? (for each policy, is it violated?)
    shouldReportViolations.value = this._policies.some(function(a) { return !a.allowsInlineStyles; });

    // allow it to execute?  (Do all the policies allow it to execute)?
    return this._policies.every(function(a) {
      return a._reportOnlyMode || a.allowsInlineStyles;
    });
  },

  getAllowsNonce: function(aNonce, aContentType, shouldReportViolation) {
    if (!CSPPrefObserver.experimentalEnabled)
      return false;

    if (!(aContentType == Ci.nsIContentPolicy.TYPE_SCRIPT ||
          aContentType == Ci.nsIContentPolicy.TYPE_STYLESHEET)) {
      CSPdebug("Nonce check requested for an invalid content type (not script or style): " + aContentType);
      return false;
    }
    let ct = ContentSecurityPolicy._MAPPINGS[aContentType];

    // allow it to execute?
    let policyAllowsNonce = [ policy.permits(null, ct, aNonce) for (policy of this._policies) ];

    shouldReportViolation.value = policyAllowsNonce.some(function(a) { return !a; });

    // allow it to execute?  (Do all the policies allow it to execute)?
    return this._policies.every(function(policy, i) {
      return policy._reportOnlyMode || policyAllowsNonce[i];
    });
  },

  /**
   * For each policy, log any violation on the Error Console and send a report
   * if a report-uri is present in the policy
   *
   * @param aViolationType
   *     one of the VIOLATION_TYPE_* constants, e.g. inline-script or eval
   * @param aSourceFile
   *     name of the source file containing the violation (if available)
   * @param aContentSample
   *     sample of the violating content (to aid debugging)
   * @param aLineNum
   *     source line number of the violation (if available)
   * @param aNonce
   *     (optional) If this is a nonce violation, include the nonce should we
   *     can recheck to determine which policies were violated and send the
   *     appropriate reports.
   */
  logViolationDetails:
  function(aViolationType, aSourceFile, aScriptSample, aLineNum, aNonce) {
    for (let policyIndex=0; policyIndex < this._policies.length; policyIndex++) {
      let policy = this._policies[policyIndex];

      // call-sites to the eval/inline checks recieve two return values: allows
      // and violates.  Policies that are report-only allow the
      // loads/compilations but violations should still be reported.  Not all
      // policies in this nsIContentSecurityPolicy instance will be violated,
      // which is why we must check again here.
      switch (aViolationType) {
      case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_STYLE:
        if (!policy.allowsInlineStyles) {
          var violatedDirective = this._buildViolatedDirectiveString('STYLE_SRC', policy);
          this._asyncReportViolation('self', null, violatedDirective, policyIndex,
                                    INLINE_STYLE_VIOLATION_OBSERVER_SUBJECT,
                                    aSourceFile, aScriptSample, aLineNum);
        }
        break;
      case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_INLINE_SCRIPT:
        if (!policy.allowsInlineScripts)    {
          var violatedDirective = this._buildViolatedDirectiveString('SCRIPT_SRC', policy);
          this._asyncReportViolation('self', null, violatedDirective, policyIndex,
                                    INLINE_SCRIPT_VIOLATION_OBSERVER_SUBJECT,
                                    aSourceFile, aScriptSample, aLineNum);
          }
        break;
      case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_EVAL:
        if (!policy.allowsEvalInScripts) {
          var violatedDirective = this._buildViolatedDirectiveString('SCRIPT_SRC', policy);
          this._asyncReportViolation('self', null, violatedDirective, policyIndex,
                                    EVAL_VIOLATION_OBSERVER_SUBJECT,
                                    aSourceFile, aScriptSample, aLineNum);
        }
        break;
      case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_NONCE_SCRIPT:
        let scriptType = ContentSecurityPolicy._MAPPINGS[Ci.nsIContentPolicy.TYPE_SCRIPT];
        if (!policy.permits(null, scriptType, aNonce)) {
          var violatedDirective = this._buildViolatedDirectiveString('SCRIPT_SRC', policy);
          this._asyncReportViolation('self', null, violatedDirective, policyIndex,
                                     SCRIPT_NONCE_VIOLATION_OBSERVER_SUBJECT,
                                     aSourceFile, aScriptSample, aLineNum);
        }
        break;
      case Ci.nsIContentSecurityPolicy.VIOLATION_TYPE_NONCE_STYLE:
        let styleType = ContentSecurityPolicy._MAPPINGS[Ci.nsIContentPolicy.TYPE_STYLE];
        if (!policy.permits(null, styleType, aNonce)) {
          var violatedDirective = this._buildViolatedDirectiveString('STYLE_SRC', policy);
          this._asyncReportViolation('self', null, violatedDirective, policyIndex,
                                     STYLE_NONCE_VIOLATION_OBSERVER_SUBJECT,
                                     aSourceFile, aScriptSample, aLineNum);
        }
        break;
      }
    }
  },

  /**
   * Given an nsIHttpChannel, fill out the appropriate data.
   */
  scanRequestData:
  function(aChannel) {
    if (!aChannel)
      return;

    // Save the docRequest for fetching a policy-uri
    this._weakDocRequest = Cu.getWeakReference(aChannel);

    // save the document URI (minus <fragment>) and referrer for reporting
    let uri = aChannel.URI.cloneIgnoringRef();
    try { // GetUserPass throws for some protocols without userPass
      uri.userPass = '';
    } catch (ex) {}
    this._request = uri.asciiSpec;
    this._requestOrigin = uri;

    //store a reference to the principal, that can later be used in shouldLoad
    this._weakRequestPrincipal = Cu.getWeakReference(Cc["@mozilla.org/scriptsecuritymanager;1"]
                                                       .getService(Ci.nsIScriptSecurityManager)
                                                       .getChannelPrincipal(aChannel));

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
   * Adds a new policy to our list of policies for this CSP context.
   * @returns the count of policies.
   */
  appendPolicy:
  function csp_appendPolicy(aPolicy, selfURI, aReportOnly, aSpecCompliant) {
#ifndef MOZ_B2G
    CSPdebug("APPENDING POLICY: " + aPolicy);
    CSPdebug("            SELF: " + selfURI.asciiSpec);
    CSPdebug("CSP 1.0 COMPLIANT : " + aSpecCompliant);
#endif

    // For nested schemes such as view-source: make sure we are taking the
    // innermost URI to use as 'self' since that's where we will extract the
    // scheme, host and port from
    if (selfURI instanceof Ci.nsINestedURI) {
#ifndef MOZ_B2G
      CSPdebug("        INNER: " + selfURI.innermostURI.asciiSpec);
#endif
      selfURI = selfURI.innermostURI;
    }

    // stay uninitialized until policy setup is done
    var newpolicy;

    // If there is a policy-uri, fetch the policy, then re-call this function.
    // (1) parse and create a CSPRep object
    // Note that we pass the full URI since when it's parsed as 'self' to construct a
    // CSPSource only the scheme, host, and port are kept.

    // If we want to be CSP 1.0 spec compliant, use the new parser.
    // The old one will be deprecated in the future and will be
    // removed at that time.
    if (aSpecCompliant) {
      newpolicy = CSPRep.fromStringSpecCompliant(aPolicy,
                                                 selfURI,
                                                 aReportOnly,
                                                 this._weakDocRequest.get(),
                                                 this);
    } else {
      newpolicy = CSPRep.fromString(aPolicy,
                                    selfURI,
                                    aReportOnly,
                                    this._weakDocRequest.get(),
                                    this);
    }

    newpolicy._specCompliant = !!aSpecCompliant;
    newpolicy._isInitialized = true;
    this._policies.push(newpolicy);
    this._cache = {}; // reset cache since effective policy changes
  },

  /**
   * Removes a policy from the array of policies.
   */
  removePolicy:
  function csp_removePolicy(index) {
    if (index < 0 || index >= this._policies.length) {
      CSPdebug("Cannot remove policy " + index + "; not enough policies.");
      return;
    }
    this._policies.splice(index, 1);
    this._cache = {}; // reset cache since effective policy changes
  },

  /**
   * Generates and sends a violation report to the specified report URIs.
   */
  sendReports:
  function(blockedUri, originalUri, violatedDirective,
           violatedPolicyIndex, aSourceFile,
           aScriptSample, aLineNum) {

    let policy = this._getPolicyInternal(violatedPolicyIndex);
    if (!policy) {
      CSPdebug("ERROR in SendReports: policy " + violatedPolicyIndex + " is not defined.");
      return;
    }

    var uriString = policy.getReportURIs();
    var uris = uriString.split(/\s+/);
    if (uris.length > 0) {
      // see if we need to sanitize the blocked-uri
      let blocked = '';
      if (originalUri) {
        // We've redirected, only report the blocked origin
        try {
          let clone = blockedUri.clone();
          clone.path = '';
          blocked = clone.asciiSpec;
        } catch(e) {
          CSPdebug(".... blockedUri can't be cloned: " + blockedUri);
        }
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
          chan.notificationCallbacks = new CSPReportRedirectSink(policy);
          if (this._weakDocRequest.get()) {
            chan.loadGroup = this._weakDocRequest.get().loadGroup;
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
                                         null, null, null, this._weakRequestPrincipal.get())
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
          policy.log(WARN_FLAG, CSPLocalizer.getFormatStr("triedToSendReport", [uris[i]]));
          policy.log(WARN_FLAG, CSPLocalizer.getFormatStr("errorWas", [e.toString()]));
        }
      }
    }
  },

  /**
   * Logs a meaningful CSP warning to the developer console.
   */
  logToConsole:
  function(blockedUri, originalUri, violatedDirective, aViolatedPolicyIndex,
           aSourceFile, aScriptSample, aLineNum, aObserverSubject) {
     let policy = this._policies[aViolatedPolicyIndex];
     switch(aObserverSubject.data) {
      case INLINE_STYLE_VIOLATION_OBSERVER_SUBJECT:
        violatedDirective = CSPLocalizer.getStr("inlineStyleBlocked");
        break;
      case INLINE_SCRIPT_VIOLATION_OBSERVER_SUBJECT:
        violatedDirective = CSPLocalizer.getStr("inlineScriptBlocked");
        break;
      case EVAL_VIOLATION_OBSERVER_SUBJECT:
        violatedDirective = CSPLocalizer.getStr("scriptFromStringBlocked");
        break;
    }
    var violationMessage = null;
    if (blockedUri["asciiSpec"]) {
      violationMessage = CSPLocalizer.getFormatStr("CSPViolationWithURI", [violatedDirective, blockedUri.asciiSpec]);
    } else {
      violationMessage = CSPLocalizer.getFormatStr("CSPViolation", [violatedDirective]);
    }
    policy.log(WARN_FLAG, violationMessage,
               (aSourceFile) ? aSourceFile : null,
               (aScriptSample) ? decodeURIComponent(aScriptSample) : null,
               (aLineNum) ? aLineNum : null);
  },

/**
   * Exposed Method to analyze docShell for approved frame ancestry.
   * NOTE: Also sends violation reports if necessary.
   * @param docShell
   *    the docShell for this policy's resource.
   * @return
   *    true if the frame ancestry is allowed by this policy and the load
   *    should progress.
   */
  permitsAncestry:
  function(docShell) {
    // Cannot shortcut checking all the policies since violation reports have
    // to be triggered if any policy wants it.
    var permitted = true;
    for (let i = 0; i < this._policies.length; i++) {
      if (!this._permitsAncestryInternal(docShell, this._policies[i], i)) {
        permitted = false;
      }
    }
    return permitted;
  },

  _permitsAncestryInternal:
  function(docShell, policy, policyIndex) {
    if (!docShell) { return false; }

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

#ifndef MOZ_B2G
        CSPdebug(" found frame ancestor " + ancestor.asciiSpec);
#endif
        ancestors.push(ancestor);
      }
    }

    // scan the discovered ancestors
    // frame-ancestors is the same in both old and new source directives,
    // so don't need to differentiate here.
    let cspContext = CSPRep.SRC_DIRECTIVES_NEW.FRAME_ANCESTORS;
    for (let i in ancestors) {
      let ancestor = ancestors[i];
      if (!policy.permits(ancestor, cspContext)) {
        // report the frame-ancestor violation
        let directive = policy._directives[cspContext];
        let violatedPolicy = 'frame-ancestors ' + directive.toString();

        this._asyncReportViolation(ancestors[i], null, violatedPolicy,
                                   policyIndex);

        // need to lie if we are testing in report-only mode
        return policy._reportOnlyMode;
      }
    }
    return true;
  },

  /**
   * Creates a cache key from content location and content type.
   */
  _createCacheKey:
  function (aContentLocation, aContentType) {
    if (aContentType != Ci.nsIContentPolicy.TYPE_SCRIPT &&
        aContentLocation.scheme == "data") {
      // For non-script data: URI, use ("data:", aContentType) as the cache key.
      return aContentLocation.scheme + ":" + aContentType;
    }

    let uri = aContentLocation.spec;
    if (uri.length > CSP_CACHE_URI_CUTOFF_SIZE) {
      // Don't cache for a URI longer than the cutoff size.
      return null;
    }
    return uri + "!" + aContentType;
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
    let key = this._createCacheKey(aContentLocation, aContentType);
    if (key && this._cache[key]) {
      return this._cache[key];
    }

#ifndef MOZ_B2G
    // Try to remove as much as possible from the hot path on b2g.
    CSPdebug("shouldLoad location = " + aContentLocation.asciiSpec);
    CSPdebug("shouldLoad content type = " + aContentType);
#endif

    // The mapping for XHR and websockets is different between our original
    // implementation and the 1.0 spec, we handle this here.
    var cspContext;

    let cp = Ci.nsIContentPolicy;

    // Infer if this is a preload for elements that use nonce-source. Since,
    // for preloads, aContext is the document and not the element associated
    // with the resource, we cannot determine the nonce. See Bug 612921 and
    // Bug 855326.
    var possiblePreloadNonceConflict =
      (aContentType == cp.TYPE_SCRIPT || aContentType == cp.TYPE_STYLESHEET) &&
      aContext instanceof Ci.nsIDOMHTMLDocument;

    // iterate through all the _policies and send reports where a policy is
    // violated.  After the check, determine the overall effect (blocked or
    // loaded?) and cache it.
    let policyAllowsLoadArray = [];
    for (let policyIndex=0; policyIndex < this._policies.length; policyIndex++) {
      let policy = this._policies[policyIndex];

#ifndef MOZ_B2G
      CSPdebug("policy is " + (policy._specCompliant ?
                              "1.0 compliant" : "pre-1.0"));
      CSPdebug("policy is " + (policy._reportOnlyMode ?
                              "report-only" : "blocking"));
#endif

      if (aContentType == cp.TYPE_XMLHTTPREQUEST && this._policies[policyIndex]._specCompliant) {
        cspContext = ContentSecurityPolicy._MAPPINGS[CSP_TYPE_XMLHTTPREQUEST_SPEC_COMPLIANT];
      } else if (aContentType == cp.TYPE_WEBSOCKET && this._policies[policyIndex]._specCompliant) {
        cspContext = ContentSecurityPolicy._MAPPINGS[CSP_TYPE_WEBSOCKET_SPEC_COMPLIANT];
      } else {
        cspContext = ContentSecurityPolicy._MAPPINGS[aContentType];
      }

#ifndef MOZ_B2G
      CSPdebug("shouldLoad cspContext = " + cspContext);
#endif

      // if the mapping is null, there's no policy, let it through.
      if (!cspContext) {
        return Ci.nsIContentPolicy.ACCEPT;
      }

      // otherwise, honor the translation
      // var source = aContentLocation.scheme + "://" + aContentLocation.hostPort;
      let context = CSPPrefObserver.experimentalEnabled ? aContext : null;
      var res = policy.permits(aContentLocation, cspContext, context) ?
                cp.ACCEPT : cp.REJECT_SERVER;
      // record whether the thing should be blocked or just reported.
      policyAllowsLoadArray.push(res == cp.ACCEPT || policy._reportOnlyMode);

      // frame-ancestors is taken care of early on (as this document is loaded)

      // If the result is *NOT* ACCEPT, then send report
      // Do not send report if this is a nonce-source preload - the decision may
      // be wrong and will incorrectly fail the unit tests.
      if (res != Ci.nsIContentPolicy.ACCEPT && !possiblePreloadNonceConflict) {
        CSPdebug("blocking request for " + aContentLocation.asciiSpec);
        try {
          let directive = "unknown directive",
              violatedPolicy = "unknown policy";

          // The policy might not explicitly declare each source directive (so
          // the cspContext may be implicit).  If so, we have to report
          // violations as appropriate: specific or the default-src directive.
          if (policy._directives.hasOwnProperty(cspContext)) {
            directive = policy._directives[cspContext];
            violatedPolicy = cspContext + ' ' + directive.toString();
          } else if (policy._directives.hasOwnProperty("default-src")) {
            directive = policy._directives["default-src"];
            violatedPolicy = "default-src " + directive.toString();
          } else {
            violatedPolicy = "unknown directive";
            CSPdebug('ERROR in blocking content: ' +
                    'CSP is not sure which part of the policy caused this block');
          }

          this._asyncReportViolation(aContentLocation, aOriginalUri,
                                     violatedPolicy, policyIndex);
        } catch(e) {
          CSPdebug('---------------- ERROR: ' + e);
        }
      }
    } // end for-each loop over policies

    // the ultimate decision is based on whether any policies want to reject
    // the load.  The array keeps track of whether the policies allowed the
    // loads. If any doesn't, we'll reject the load (and cache the result).
    let ret = (policyAllowsLoadArray.some(function(a,b) { return !a; }) ?
               cp.REJECT_SERVER : cp.ACCEPT);

    // Do not cache the result if this is a nonce-source preload
    if (key && !possiblePreloadNonceConflict) {
      this._cache[key] = ret;
    }
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
   * @param aBlockedContentSource
   *        Either a CSP Source (like 'self', as string) or nsIURI: the source
   *        of the violation.
   * @param aOriginalUri
   *        The original URI if the blocked content is a redirect, else null
   * @param aViolatedDirective
   *        the directive that was violated (string).
   * @param aViolatedPolicyIndex
   *        the index of the policy that was violated (so we know where to send
   *        the reports).
   * @param aObserverSubject
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
  function(aBlockedContentSource, aOriginalUri, aViolatedDirective,
           aViolatedPolicyIndex, aObserverSubject,
           aSourceFile, aScriptSample, aLineNum) {
    // if optional observerSubject isn't specified, default to the source of
    // the violation.
    if (!aObserverSubject)
      aObserverSubject = aBlockedContentSource;

    // gotta wrap things that aren't nsISupports, since it's sent out to
    // observers as such.  Objects that are not nsISupports are converted to
    // strings and then wrapped into a nsISupportsCString.
    if (!(aObserverSubject instanceof Ci.nsISupports)) {
      let d = aObserverSubject;
      aObserverSubject = Cc["@mozilla.org/supports-cstring;1"]
                          .createInstance(Ci.nsISupportsCString);
      aObserverSubject.data = d;
    }

    var reportSender = this;
    Services.tm.mainThread.dispatch(
      function() {
        Services.obs.notifyObservers(aObserverSubject,
                                     CSP_VIOLATION_TOPIC,
                                     aViolatedDirective);
        reportSender.sendReports(aBlockedContentSource, aOriginalUri,
                                 aViolatedDirective, aViolatedPolicyIndex,
                                 aSourceFile, aScriptSample, aLineNum);
        reportSender.logToConsole(aBlockedContentSource, aOriginalUri,
                                  aViolatedDirective, aViolatedPolicyIndex,
                                  aSourceFile, aScriptSample,
                                  aLineNum, aObserverSubject);

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
    this._policy.log(WARN_FLAG, CSPLocalizer.getFormatStr("reportPostRedirect", [oldChannel.URI.asciiSpec]));

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
