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
Cu.import("resource://gre/modules/CSPUtils.jsm");

/* ::::: Policy Parsing & Data structures :::::: */

function ContentSecurityPolicy() {
  CSPdebug("CSP CREATED");
  this._isInitialized = false;
  this._enforcedPolicy = null;
  this._reportOnlyPolicy = null;
  this._requestHeaders = [];
  this._request = "";

  this._observerService = Cc['@mozilla.org/observer-service;1']
                            .getService(Ci.nsIObserverService);
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
  csp._MAPPINGS[cp.TYPE_OTHER]             =  cspr_sd.ALLOW;

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


  /* These must go through the catch-all */
  csp._MAPPINGS[cp.TYPE_XBL]               = cspr_sd.ALLOW;
  csp._MAPPINGS[cp.TYPE_PING]              = cspr_sd.ALLOW;
  csp._MAPPINGS[cp.TYPE_DTD]               = cspr_sd.ALLOW;
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

  /**
   * Decides whether or not inline scripts are allowed and sends out violation
   * reports if they're not.  Also, if there's a report-only policy, violation
   * reports are sent when the policy disallows them, but this returns true.
   */
  get allowsInlineScript() {
    // gotta wrap the violation string, since it's sent out to observers as
    // an nsISupports.
    var violation = Cc["@mozilla.org/supports-cstring;1"]
                      .createInstance(Ci.nsISupportsCString);
    violation.data = 'base restriction: no inline scripts';

    // first, trigger reports if the report-only policy disallows inline
    // scripts (but do not decide whether or not to block the script).
    if (this._reportOnlyPolicy && !this._reportOnlyPolicy.allowsInlineScripts) {
      this._observerService.notifyObservers(
                              violation,
                              CSP_VIOLATION_TOPIC,
                              'inline script base restriction');
      this._sendReports(this._reportOnlyPolicy, 'self', violation.data);
    }

    // second, verify with real policy and trigger automatic report to go out
    // when inline scripts are disabled.
    if (this._enforcedPolicy && !this._enforcedPolicy.allowsInlineScripts) {
      this._observerService.notifyObservers(
                              violation,
                              CSP_VIOLATION_TOPIC,
                              'inline script base restriction');
      this._sendReports(this._enforcedPolicy, 'self', violation.data);
      return false;
    }

    // finally, if we get here, inline scripts should be allowed
    return true;
  },

  /**
   * Decides whether or not eval() is allowed and sends out violation
   * reports if not.  Also, if there's a report-only policy, violation
   * reports are sent when the policy disallows eval, but this returns true.
   */
  get allowsEval() {
    // gotta wrap the violation string, since it's sent out to observers as
    // an nsISupports.
    var violation = Cc["@mozilla.org/supports-cstring;1"]
                             .createInstance(Ci.nsISupportsCString);
    violation.data = 'base restriction: no eval-like function calls';

    // first, trigger reports if the report-only policy disallows eval
    // (but do not decide whether or not to block the script).
    if (this._reportOnlyPolicy && !this._reportOnlyPolicy.allowsEvalInScripts) {
      this._observerService.notifyObservers(
                              violation,
                              CSP_VIOLATION_TOPIC,
                              'eval script base restriction');
      this._sendReports(this._reportOnlyPolicy, 'self', violation.data);
    }

    // trigger automatic report to go out when eval and friends are disabled.
    if (this._enforcedPolicy && !this._enforcedPolicy.allowsEvalInScripts) {
      this._observerService.notifyObservers(
                              violation,
                              CSP_VIOLATION_TOPIC,
                              'eval script base restriction');
      this._sendReports(this._enforcedPolicy, 'self', violation.data);
      return false;
    }

    return true;
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
    var internalChannel = aChannel.QueryInterface(Ci.nsIHttpChannelInternal);
    var reqMaj = {};
    var reqMin = {};
    var reqVersion = internalChannel.getRequestVersion(reqMaj, reqMin);
    this._request = aChannel.requestMethod + " " 
                  + aChannel.URI.asciiSpec
                  + " HTTP/" + reqMaj.value + "." + reqMin.value;

    // grab the request headers
    var self = this;
    aChannel.visitRequestHeaders({
      visitHeader: function(aHeader, aValue) {
        self._requestHeaders.push(aHeader + ": " + aValue);
      }});
  },

/* ........ Methods .............. */

  function csp_refinePolicyInternal(policyToRefine, aPolicy, selfURI) {
    CSPdebug("     REFINING: " + policyToRefine);
    CSPdebug("REFINE POLICY: " + aPolicy);
    CSPdebug("         SELF: " + selfURI.asciiSpec);

    // stay uninitialized until policy merging is done
    this._isInitialized = false;

    // If there is a policy-uri, fetch the policy, then re-call this function.
    // (1) parse and create a CSPRep object
    var newpolicy = CSPRep.fromString(aPolicy,
                                      selfURI.scheme + "://" + selfURI.hostPort);

    // (2) Intersect the currently installed CSPRep object with the new one
    if (!this[policyToRefine])
      this[policyToRefine] = newpolicy;
    else
      this[policyToRefine] = this._policy.intersectWith(newpolicy);

    // (3) Finished
    this._isInitialized = true;
  },

  /**
   * Given a new policy, intersects the currently enforced policy with the new
   * one and stores the result.  The effect is a "tightening" or refinement of
   * an old policy.  This is called any time a new policy is encountered and
   * the effective policy has to be refined.
   */
  refineEnforcedPolicy:
  function csp_refineEnforcedPolicy(aPolicy, selfURI) {
    return this._refinePolicyInternal("_enforcedPolicy", aPolicy, selfURI);
  },

  /**
   * Same, but for report-only policy.
   */
  refineReportOnlyPolicy:
  function csp_refineReportOnlyPolicy(aPolicy, selfURI) {
    return this._refinePolicyInternal("_reportOnlyPolicy", aPolicy, selfURI);
  },

  /**
   * Generates and sends a violation report to the specified report URIs.
   */
  _sendReports:
  function(policyViolated, blockedUri, violatedDirective) {
    var uriString = policyViolated.getReportURIs();
    var uris = uriString.split(/\s+/);
    if (uris.length > 0) {
      // Generate report to send composed of
      // {
      //   csp-report: {
      //     request: "GET /index.html HTTP/1.1",
      //     request-headers: "Host: example.com
      //                       User-Agent: ...
      //                       ...",
      //     blocked-uri: "...",
      //     violated-directive: "..."
      //   }
      // }
      var strHeaders = "";
      for (let i in this._requestHeaders) {
        strHeaders += this._requestHeaders[i] + "\n";
      }
      var report = {
        'csp-report': {
          'request': this._request,
          'request-headers': strHeaders,
          'blocked-uri': (blockedUri instanceof Ci.nsIURI ?
                          blockedUri.asciiSpec : blockedUri),
          'violated-directive': violatedDirective
        }
      }
      CSPdebug("Constructed violation report:\n" + JSON.stringify(report));

      // For each URI in the report list, send out a report.
      for (let i in uris) {
        if (uris[i] === "")
          continue;

        var failure = function(aEvt) {  
          if (req.readyState == 4 && req.status != 200) {
            CSPError("Failed to send report to " + reportURI);
          }  
        };  
        var req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]  
                    .createInstance(Ci.nsIXMLHttpRequest);  

        try {
          req.open("POST", uris[i], true);
          req.setRequestHeader('Content-Type', 'application/json');
          req.upload.addEventListener("error", failure, false);
          req.upload.addEventListener("abort", failure, false);
          //req.channel.loadFlags |= Ci.nsIRequest.LOAD_BYPASS_CACHE;
 
          // make request anonymous
          // This prevents sending cookies with the request,
          // in case the policy URI is injected, it can't be
          // abused for CSRF.
          req.channel.loadFlags |= Ci.nsIChannel.LOAD_ANONYMOUS;

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

      // first, check the report-only policy.
      if (this._reportOnlyPolicy) {
        let directive = this._reportOnlyPolicy._directives[cspContext];
        let violatedPolicy = (directive._isImplicit
                              ? 'allow' : 'frame-ancestors ')
                              + directive.toString();
        if (!this._reportOnlyPolicy.permits(ancestor, cspContext)) {
          this._observerService.notifyObservers(
                                  ancestors[i],
                                  CSP_VIOLATION_TOPIC,
                                  violatedPolicy);
          this._sendReports(this._enforcedPolicy, ancestors[i].asciiSpec, violatedPolicy);
        }
      }

      // second, check the enforced policy.
      if (this._enforcedPolicy) {
        let directive = this._enforcedPolicy._directives[cspContext];
        let violatedPolicy = (directive._isImplicit
                              ? 'allow' : 'frame-ancestors ')
                              + directive.toString();
        if(!this._enforcedPolicy.permits(ancestor, cspContext)) {
          this._observerService.notifyObservers(
                                  ancestors[i],
                                  CSP_VIOLATION_TOPIC,
                                  violatedPolicy);
          this._sendReports(this._enforcedPolicy, ancestors[i].asciiSpec, violatedPolicy);
          return false;
        }
      }
    }

    // all ancestors allowed (or only violated a report-only policy)
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
    if (aContentLocation.scheme === 'chrome') {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    // interpret the context, and then pass off to the decision structure
    CSPdebug("shouldLoad location = " + aContentLocation.asciiSpec);
    CSPdebug("shouldLoad content type = " + aContentType);
    var cspContext = ContentSecurityPolicy._MAPPINGS[aContentType];
    // CSPdebug("shouldLoad CSP directive =" + cspContext);

    // if the mapping is null, there's no policy, let it through.
    if (!cspContext) {
      return Ci.nsIContentPolicy.ACCEPT;
    }

    // frame-ancestors is already taken care of early on (as this document is
    // loaded)

    // first, run it by the report-only policy
    if (this._reportOnlyPolicy) {
      var ro_res = this._reportOnlyPolicy.permits(aContentLocation, cspContext)
                    ? Ci.nsIContentPolicy.ACCEPT
                    : Ci.nsIContentPolicy.REJECT_SERVER;

      if (ro_res != Ci.nsIContentPolicy.ACCEPT) {
        try {
          let directive = this._reportOnlyPolicy._directives[cspContext];
          let violatedPolicy = (directive._isImplicit
                                  ? 'allow' : cspContext)
                                  + ' ' + directive.toString();
          this._observerService.notifyObservers(
                                  aContentLocation,
                                  CSP_VIOLATION_TOPIC,
                                  violatedPolicy);
          this._sendReports(this._reportOnlyPolicy, aContentLocation, violatedPolicy);
        } catch(e) {
          CSPdebug('---------------- ERROR: ' + e);
        }
      }
    }

    // also run it by the enforced policy
    if (this._enforcedPolicy) {
      var en_res = this._enforcedPolicy.permits(aContentLocation, cspContext)
                    ? Ci.nsIContentPolicy.ACCEPT
                    : Ci.nsIContentPolicy.REJECT_SERVER;

      if (en_res != Ci.nsIContentPolicy.ACCEPT) {
        CSPdebug("blocking request for " + aContentLocation.asciiSpec);
        try {
          let directive = this._enforcedPolicy._directives[cspContext];
          let violatedPolicy = (directive._isImplicit
                                  ? 'allow' : cspContext)
                                  + ' ' + directive.toString();
          this._observerService.notifyObservers(
                                  aContentLocation,
                                  CSP_VIOLATION_TOPIC,
                                  violatedPolicy);
          this._sendReports(this._enforcedPolicy, aContentLocation, violatedPolicy);
        } catch(e) {
          CSPdebug('---------------- ERROR: ' + e);
        }
        return en_res;
      }
    }

    return Ci.nsIContentPolicy.ACCEPT;
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

};

var NSGetFactory = XPCOMUtils.generateNSGetFactory([ContentSecurityPolicy]);
