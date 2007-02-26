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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Fritz Schneider <fritz@google.com> (original author)
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


// The warden checks request to see if they are for phishy pages. It
// does so by either querying a remote server with the URL (advanced
// protectoin mode) or querying our locally stored blacklists (privacy
// mode).
// 
// When the warden notices a problem, it queries all browser views
// (each of which corresopnds to an open browser window) to see
// whether one of them can handle it. A browser view can handle a
// problem if its browser window has an HTMLDocument loaded with the
// given URL and that Document hasn't already been flagged as a
// problem. For every problematic URL we notice loading, at most one
// Document is flagged as problematic. Otherwise you can get into
// trouble if multiple concurrent phishy pages load with the same URL.
//
// Since we check URLs very early in the request cycle (in a progress
// listener), the URL might not yet be associated with a Document when
// we determine that it is phishy. So the the warden retries finding
// a browser view to handle the problem until one can, or until it
// determines it should give up (see complicated logic below).
//
// The warden has displayers that the browser view uses to render
// different kinds of warnings (e.g., one that's shown before a page
// loads as opposed to one that's shown after the page has already
// loaded).
//
// Note: There is a single warden for the whole application.
//
// TODO better way to expose displayers/views to browser view

const kPhishWardenEnabledPref = "browser.safebrowsing.enabled";
const kPhishWardenRemoteLookups = "browser.safebrowsing.remoteLookups";

// We have hardcoded URLs that we let people navigate to in order to 
// check out the warning.
const kTestUrls = {
  "http://www.google.com/tools/firefox/safebrowsing/phish-o-rama.html": true,
  "http://www.mozilla.org/projects/bonecho/anti-phishing/its-a-trap.html": true,
  "http://www.mozilla.com/firefox/its-a-trap.html": true,
}

/**
 * Abtracts the checking of user/browser actions for signs of
 * phishing. 
 *
 * @param progressListener nsIDocNavStartProgressListener
 * @constructor
 */
function PROT_PhishingWarden(progressListener) {
  PROT_ListWarden.call(this);

  this.debugZone = "phishwarden";
  this.testing_ = false;
  this.browserViews_ = [];

  // Use this to query preferences
  this.prefs_ = new G_Preferences();

  // Only one displayer so far; perhaps we'll have others in the future
  this.displayers_ = {
    "afterload": PROT_PhishMsgDisplayer,
  };

  // We use this dude to do lookups on our remote server
  this.fetcher_ = new PROT_TRFetcher();

  // We need to know whether we're enabled and whether we're in advanced
  // mode, so reflect the appropriate preferences into our state.

  // Read state: should we be checking remote preferences?
  this.checkRemote_ = this.prefs_.getPref(kPhishWardenRemoteLookups, null);
  
  // true if we should use whitelists to suppress remote lookups
  this.checkWhitelists_ = false;

  // Get notifications when the remote check preference changes
  var checkRemotePrefObserver = BindToObject(this.onCheckRemotePrefChanged,
                                             this);
  this.prefs_.addObserver(kPhishWardenRemoteLookups, checkRemotePrefObserver);

  // Global preference to enable the phishing warden
  this.phishWardenEnabled_ = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  // Get notifications when the phishing warden enabled pref changes
  var phishWardenPrefObserver = 
    BindToObject(this.onPhishWardenEnabledPrefChanged, this);
  this.prefs_.addObserver(kPhishWardenEnabledPref, phishWardenPrefObserver);
  
  // Get notifications when the data provider pref changes
  var dataProviderPrefObserver =
    BindToObject(this.onDataProviderPrefChanged, this);
  this.prefs_.addObserver(kDataProviderIdPref, dataProviderPrefObserver);

  // hook up our browser listener
  this.progressListener_ = progressListener;
  this.progressListener_.callback = this;
  this.progressListener_.enabled = this.phishWardenEnabled_;
  // ms to wait after a request has started before firing JS callback
  this.progressListener_.delay = 1500;

  // object to keep track of request errors if we're in remote check mode
  this.requestBackoff_ = new RequestBackoff(3 /* num errors */,
                                   10*60*1000 /* error time, 10min */,
                                   10*60*1000 /* backoff interval, 10min */,
                                   6*60*60*1000 /* max backoff, 6hr */);

  G_Debug(this, "phishWarden initialized");
}

PROT_PhishingWarden.inherits(PROT_ListWarden);

/**
 * We implement nsIWebProgressListener
 */
PROT_PhishingWarden.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || 
      iid.equals(Ci.nsIWebProgressListener) ||
      iid.equals(Ci.nsISupportsWeakReference))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Cleanup on shutdown.
 */
PROT_PhishingWarden.prototype.shutdown = function() {
  this.progressListener_.callback = null;
  this.progressListener_ = null;
  this.listManager_ = null;
}

/**
 * When a preference (either advanced features or the phishwarden
 * enabled) changes, we might have to start or stop asking for updates. 
 * 
 * This is a little tricky; we start or stop management only when we
 * have complete information we can use to determine whether we
 * should.  It could be the case that one pref or the other isn't set
 * yet (e.g., they haven't opted in/out of advanced features). So do
 * nothing unless we have both pref values -- we get notifications for
 * both, so eventually we will start correctly.
 */ 
PROT_PhishingWarden.prototype.maybeToggleUpdateChecking = function() {
  if (this.testing_)
    return;

  var phishWardenEnabled = this.prefs_.getPref(kPhishWardenEnabledPref, null);

  this.checkRemote_ = this.prefs_.getPref(kPhishWardenRemoteLookups, null);

  G_Debug(this, "Maybe toggling update checking. " +
          "Warden enabled? " + phishWardenEnabled + " || " +
          "Check remote? " + this.checkRemote_);

  // Do nothing unless both prefs are set.  They can be null (unset), true, or
  // false.
  if (phishWardenEnabled === null || this.checkRemote_ === null)
    return;

  // We update and save to disk all tables if we don't have remote checking
  // enabled.
  if (phishWardenEnabled === true) {
    // If anti-phishing is enabled, we always download the local files to
    // use in case remote lookups fail.
    this.enableBlacklistTableUpdates();
    this.enableWhitelistTableUpdates();

    if (this.checkRemote_ === true) {
      // Remote lookup mode
      // We check to see if the local list update host is the same as the
      // remote lookup host.  If they are the same, then we don't bother
      // to do a remote url check if the url is in the whitelist.
      var ioService = Cc["@mozilla.org/network/io-service;1"]
                     .getService(Ci.nsIIOService);
      var updateHost = '';
      var lookupHost = '';
      try {
        var url = ioService.newURI(gDataProvider.getUpdateURL(),
                                         null, null);
        updateHost = url.asciiHost;
      } catch (e) { }
      try {
        var url = ioService.newURI(gDataProvider.getLookupURL(),
                                         null, null);
        lookupHost = url.asciiHost;
      } catch (e) { }

      if (updateHost && lookupHost && updateHost == lookupHost) {
        // The data provider for local lists and remote lookups is the
        // same, enable whitelist lookup suppression.
        this.checkWhitelists_ = true;
      } else {
        // hosts don't match, don't use whitelist suppression
        this.checkWhitelists_ = false;
      }
    }
  } else {
    // Anti-phishing is off, disable table updates
    this.disableBlacklistTableUpdates();
    this.disableWhitelistTableUpdates();
  }
}

/**
 * Controllers register their browser views with us
 *
 * @param view Reference to a browser view 
 */
PROT_PhishingWarden.prototype.addBrowserView = function(view) {
  G_Debug(this, "New browser view registered.");
  this.browserViews_.push(view);
}

/**
 * Controllers unregister their views when their window closes
 *
 * @param view Reference to a browser view 
 */
PROT_PhishingWarden.prototype.removeBrowserView = function(view) {
  for (var i = 0; i < this.browserViews_.length; i++)
    if (this.browserViews_[i] === view) {
      G_Debug(this, "Browser view unregistered.");
      this.browserViews_.splice(i, 1);
      return;
    }
  G_Assert(this, false, "Tried to unregister non-existent browser view!");
}

/**
 * Deal with a user changing the pref that says whether we should check
 * the remote server (i.e., whether we're in advanced mode)
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should check remote server
 */
PROT_PhishingWarden.prototype.onCheckRemotePrefChanged = function(prefName) {
  this.checkRemote_ = this.prefs_.getBoolPrefOrDefault(prefName,
                                                       this.checkRemote_);
  this.requestBackoff_.reset();
  this.maybeToggleUpdateChecking();
}

/**
 * Deal with a user changing the pref that says whether we should 
 * enable the phishing warden (i.e., that SafeBrowsing is active)
 *
 * @param prefName Name of the pref holding the value indicating whether
 *                 we should enable the phishing warden
 */
PROT_PhishingWarden.prototype.onPhishWardenEnabledPrefChanged = function(
                                                                    prefName) {
  this.phishWardenEnabled_ = 
    this.prefs_.getBoolPrefOrDefault(prefName, this.phishWardenEnabled_);
  this.requestBackoff_.reset();
  this.maybeToggleUpdateChecking();
  this.progressListener_.enabled = this.phishWardenEnabled_;
}

/**
 * Event fired when the user changes data providers.
 */
PROT_PhishingWarden.prototype.onDataProviderPrefChanged = function(prefName) {
  // We want to reset request backoff state since it's a different provider.
  this.requestBackoff_.reset();

  // If we have a new data provider and we're doing remote lookups, then
  // we may want to use whitelist lookup suppression or change which
  // tables are being downloaded.
  if (this.checkRemote_) {
    this.maybeToggleUpdateChecking();
  }
}

/**
 * A request for a Document has been initiated somewhere. Check it!
 *
 * @param request
 * @param url
 */ 
PROT_PhishingWarden.prototype.onDocNavStart = function(request, url) {
  G_Debug(this, "checkRemote: " +
          (this.checkRemote_ ? "yes" : "no"));

  // If we're on a test page, trigger the warning.
  // XXX Do we still need a test url or should each provider just put
  // it in their local list?
  if (this.isBlacklistTestURL(url)) {
    this.houstonWeHaveAProblem_(request);
    return;
  }

  // Make a remote lookup check if the pref is selected and if we haven't
  // triggered server backoff.  Otherwise, make a local check.
  if (this.checkRemote_ && this.requestBackoff_.canMakeRequest()) {
    // If we can use whitelists to suppress remote lookups, do so.
    if (this.checkWhitelists_) {
      var maybeRemoteCheck = BindToObject(this.maybeMakeRemoteCheck_,
                                          this,
                                          url,
                                          request);
      this.isWhiteURL(url, maybeRemoteCheck);
    } else {
      // Do a remote lookup (don't check whitelists)
      this.fetcher_.get(url,
                        BindToObject(this.onTRFetchComplete,
                                     this,
                                     url,
                                     request));
    }
  } else {
    // Check the local lists for a match.
    var evilCallback = BindToObject(this.localListMatch_,
                                    this,
                                    url,
                                    request);
    this.isEvilURL(url, evilCallback);
  }
}

/** 
 * Callback from whitelist check when remote lookups is on.
 * @param url String url to lookup
 * @param request nsIRequest object
 * @param status int enum from callback (PROT_ListWarden.IN_BLACKLIST,
 *    PROT_ListWarden.IN_WHITELIST, PROT_ListWarden.NOT_FOUND)
 */
PROT_PhishingWarden.prototype.maybeMakeRemoteCheck_ = function(url, request, status) {
  if (PROT_ListWarden.IN_WHITELIST == status)
    return;

  G_Debug(this, "Local whitelist lookup failed");
  this.fetcher_.get(url,
                    BindToObject(this.onTRFetchComplete,
                                 this,
                                 url,
                                 request));
}

/**
 * Invoked with the result of a lookupserver request.
 *
 * @param url String the URL we looked up
 * @param request The nsIRequest in which we're interested
 * @param trValues Object holding name/value pairs parsed from the
 *                 lookupserver's response
 * @param status Number HTTP status code or NS_ERROR_NOT_AVAILABLE if there's
 *               an HTTP error
 */
PROT_PhishingWarden.prototype.onTRFetchComplete = function(url,
                                                           request,
                                                           trValues,
                                                           status) {
  // Did the remote http request succeed?  If not, we fall back on
  // local lists.
  if (status == Components.results.NS_ERROR_NOT_AVAILABLE ||
      this.requestBackoff_.isErrorStatus_(status)) {
    this.requestBackoff_.noteServerResponse(status);

    G_Debug(this, "remote check failed, using local lists instead");
    var evilCallback = BindToObject(this.localListMatch_,
                                    this,
                                    url,
                                    request);
    this.isEvilURL(url, evilCallback);
  } else {
    var callback = BindToObject(this.houstonWeHaveAProblem_, this, request);
    this.checkRemoteData(callback, trValues);
  }
}

/**
 * One of our Check* methods found a problem with a request. Why do we
 * need to keep the nsIRequest (instead of just passing in the URL)? 
 * Because we need to know when to stop looking for the URL it's
 * fetching, and to know this we need the nsIRequest.isPending flag.
 *
 * @param request nsIRequest that is problematic
 */
PROT_PhishingWarden.prototype.houstonWeHaveAProblem_ = function(request) {

  // We have a problem request that might or might not be associated
  // with a Document that's currently in a browser. If it is, we 
  // want that Document. If it's not, we want to give it a chance to 
  // be loaded. See below for complete details.

  if (this.maybeLocateProblem_(request))       // Cases 1 and 2 (see below)
    return;

  // OK, so the request isn't associated with any currently accessible
  // Document, and we want to give it the chance to be. We don't want
  // to retry forever (e.g., what if the Document was already displayed
  // and navigated away from?), so we'll use nsIRequest.isPending to help
  // us decide what to do.
  //
  // A complication arises because there is a lag between when a
  // request transitions from pending to not-pending and when it's
  // associated with a Document in a browser. The transition from
  // pending to not occurs just before the notification corresponding
  // to NavWatcher.DOCNAVSTART (see NavWatcher), but the association
  // occurs afterwards. Unfortunately, we're probably in DOCNAVSTART.
  // 
  // Diagnosis by Darin:
  // ---------------------------------------------------------------------------
  // Here's a summary of what happens:
  //
  //   RestorePresentation() {
  //     Dispatch_OnStateChange(dummy_request, STATE_START)
  //     PostCompletionEvent()
  //   }
  //
  //   CompletionEvent() {
  //     ReallyRestorePresentation()
  //     Dispatch_OnStateChange(dummy_request, STATE_STOP)
  //   }
  //
  // So, now your code receives that initial OnStateChange event and sees
  // that the dummy_request is not pending and not loaded in any window.
  // So, you put a timeout(0) event in the queue.  Then, the CompletionEvent
  // is added to the queue.  The stack unwinds....
  //
  // Your timeout runs, and you find that the dummy_request is still not
  // pending and not loaded in any window.  Then the CompletionEvent
  // runs, and it hooks up the cached presentation.
  // 
  // https://bugzilla.mozilla.org/show_bug.cgi?id=319527
  // ---------------------------------------------------------------------------
  //
  // So the logic is:
  //
  //         request     found an unhandled          
  //  case   pending?    doc with the url?         action
  //  ----------------------------------------------------------------
  //   1      yes             yes           Use that doc (handled above)
  //   2      no              yes           Use that doc (handled above)
  //   3      yes             no            Retry
  //   4      no              no            Retry twice (case described above)
  //
  // We don't get into trouble with Docs with the same URL "stealing" the 
  // warning because there is exactly one warning signaled per nav to 
  // a problem URL, and each Doc can be marked as problematic at most once.

  if (request.isPending()) {        // Case 3

    G_Debug(this, "Can't find problem Doc; Req pending. Retrying.");
    new G_Alarm(BindToObject(this.houstonWeHaveAProblem_, 
                             this, 
                             request), 
                200 /*ms*/);

  } else {                          // Case 4

    G_Debug(this, 
            "Can't find problem Doc; Req completed. Retrying at most twice.");
    new G_ConditionalAlarm(BindToObject(this.maybeLocateProblem_, 
                                        this, 
                                        request),
                           0 /* next event loop */, 
                           true /* repeat */, 
                           2 /* at most twice */);
  }
}

/**
 * Query all browser views we know about and offer them the chance to
 * handle the problematic request.
 *
 * @param request nsIRequest that is problematic
 * 
 * @returns Boolean indicating if someone decided to handle it
 */
PROT_PhishingWarden.prototype.maybeLocateProblem_ = function(request) {
  G_Debug(this, "Trying to find the problem.");

  G_Debug(this, this.browserViews_.length + " browser views to check.");
  for (var i = 0; i < this.browserViews_.length; i++) {
    if (this.browserViews_[i].tryToHandleProblemRequest(this, request)) {
      G_Debug(this, "Found browser view willing to handle problem!");
      return true;
    }
    G_Debug(this, "wrong browser view");
  }
  return false;
}

/**
 * Indicates if this URL is one of the possible blacklist test URLs.
 * These test URLs should always be considered as phishy.
 *
 * @param url URL to check 
 * @return A boolean indicating whether this is one of our blacklist
 *         test URLs
 */
PROT_PhishingWarden.prototype.isBlacklistTestURL = function(url) {
  // Explicitly check for URL so we don't get JS warnings in strict mode.
  if (kTestUrls[url])
    return true;
  return false;
}

/**
 * Callback for found local blacklist match.  First we report that we have
 * a blacklist hit, then we bring up the warning dialog.
 * @param status Number enum from callback (PROT_ListWarden.IN_BLACKLIST,
 *    PROT_ListWarden.IN_WHITELIST, PROT_ListWarden.NOT_FOUND)
 */
PROT_PhishingWarden.prototype.localListMatch_ = function(url, request, status) {
  if (PROT_ListWarden.IN_BLACKLIST != status)
    return;

  // Maybe send a report
  (new PROT_Reporter).report("phishblhit", url);
  this.houstonWeHaveAProblem_(request);
}

/**
 * Examine data fetched from a lookup server for evidence of a
 * phishing problem. 
 *
 * @param callback Function to invoke if there is a problem. 
 * @param trValues Object containing name/value pairs the server returned
 */
PROT_PhishingWarden.prototype.checkRemoteData = function(callback, 
                                                         trValues) {

  if (!trValues) {
    G_Debug(this, "Didn't get TR values from the server.");
    return;
  }
  
  G_Debug(this, "Page has phishiness " + trValues["phishy"]);

  if (trValues["phishy"] == 1) {     // It's on our blacklist 
    G_Debug(this, "Remote blacklist hit");
    callback(this);
  } else {
    G_Debug(this, "Remote blacklist miss");
  }
}

#ifdef 0
// Some unittests (e.g., paste into JS shell)
var warden = safebrowsing.phishWarden;
function expectLocalCheck() {
  warden.isEvilURL = function() {
    dump("checkurl: ok\n");
  }
  warden.checkRemoteData = function() {
    throw "unexpected remote check";
  }
}
function expectRemoteCheck() {
  warden.isEvilURL = function() {
    throw "unexpected local check";
  }
  warden.checkRemoteData = function() {
    dump("checkremote: ok\n");
  }
}

warden.requestBackoff_.reset();

// START TESTS
expectRemoteCheck();
warden.onTRFetchComplete(null, null, null, 200);

// HTTP 5xx should fallback on local check
expectLocalCheck();
warden.onTRFetchComplete(null, null, null, 500);
warden.onTRFetchComplete(null, null, null, 502);

// Only two errors have occurred, so we continue to try remote lookups.
if (!warden.requestBackoff_.canMakeRequest()) throw "expected ok";

// NS_ERROR_NOT_AVAILABLE also triggers a local check, but it doesn't
// count as a remote lookup error.  We don't know /why/ it failed (e.g.,
// user may just be in offline mode).
warden.onTRFetchComplete(null, null, null,
                         Components.results.NS_ERROR_NOT_AVAILABLE);
if (!warden.requestBackoff_.canMakeRequest()) throw "expected ok";

// HTTP 302, 303, 307 should also trigger an error.  This is our
// third error so we should now be in backoff mode.
expectLocalCheck();
warden.onTRFetchComplete(null, null, null, 303);

if (warden.requestBackoff_.canMakeRequest()) throw "expected failed";

#endif
