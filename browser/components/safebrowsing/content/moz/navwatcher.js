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


// A NavWatcher abstracts away the mechanics of listening for progresslistener-
// based notifications. You register on it to hear these notifications instead
// of on a tabbedbrowser or the docloader service.
//
// Since it hooks the docloader service, you only need one NavWatcher
// per application (as opposed to a TabbedBrowserWatcher, of which you need
// one per tabbedbrowser, meaning one per browser window).
//
// The notifications you can register to hear are:
// 
// EVENT             DESCRIPTION
// -----             -----------
//
// docnavstart       Fires when the request for a Document has begun. This is
//                   as early a notification as you can get (roughly 
//                   equivalent to STATE_START). As a result, there is no 
//                   guarantee that when the notification fires the request 
//                   has been associated with a Document accessible in a 
//                   browser somewhere; the only guarantee is that there 
//                   _will_ be, baring some major error. You can handle
//                   such error cases gracefully by examining the nsIRequest
//                   isPending flag. See phishing-warden.js for an example.
// 
// For docnavstart the event object you'll receive will have the following
// properties:
//
//      request -- reference to the nsIRequest of the new request
//      url -- String containing the URL the request is for (from 
//             reuest.name, currently, but might be something better
//             in the future)
//
// Example:
//
// function handler(e /*event object*/) {
//   foo(e.request);
// };
// var watcher = new G_NavWatcher();
// watcher.registerListener("docnavstart", handler);
// watcher.removeListener("docnavstart", handler);
//
// TODO: should probably make both NavWatcher and TabbedBrowserWatcher
//       subclasses of EventRegistrar
//
// TODO: might make a stateful NavWatcher, one that takes care of mapping 
//       requests and urls to Documents.

/**
 * The NavWatcher abstracts listening for progresslistener-based
 * notifications.
 * 
 * @param opt_filterSpurious Boolean indicating whether to filter events
 *                           for navigations to docs about which you 
 *                           probably don't care, such as about:blank, 
 *                           chrome://, and file:// URLs.
 *
 * @constructor
 */
function G_NavWatcher(opt_filterSpurious) {
  this.debugZone = "navwatcher";
  this.filterSpurious_ = !!opt_filterSpurious;
  this.events = G_NavWatcher.events;            // Convenience pointer

  this.registrar_ = new EventRegistrar(this.events);

  var wp = Ci.nsIWebProgress;
  var wpService = Cc["@mozilla.org/docloaderservice;1"].getService(wp);
  wpService.addProgressListener(this, wp.NOTIFY_STATE_ALL);
}

// Events for which listeners can register. Future additions could include 
// things such as docnavstop or navstart (for any navigation).
G_NavWatcher.events = {
  DOCNAVSTART: "docnavstart",
};

/**
 * We implement nsIWebProgressListener
 */
G_NavWatcher.prototype.QueryInterface = function(iid) {
  if (iid.equals(Ci.nsISupports) || 
      iid.equals(Ci.nsIWebProgressListener) ||
      iid.equals(Ci.nsISupportsWeakReference))
    return this;
  throw Components.results.NS_ERROR_NO_INTERFACE;
}

/**
 * Register to receive events of a particular type
 * 
 * @param eventType String indicating the event (see 
 *                  G_NavWatcher.events)
 * 
 * @param listener Function to invoke when the event occurs. See top-
 *                 level comments for parameters.
 */
G_NavWatcher.prototype.registerListener = function(eventType, 
                                                   listener) {
  this.registrar_.registerListener(eventType, listener);
}

/**
 * Unregister a listener.
 * 
 * @param eventType String one of G_NavWatcher.events' members
 *
 * @param listener Function to remove as listener
 */
G_NavWatcher.prototype.removeListener = function(eventType, 
                                                 listener) {
  this.registrar_.removeListener(eventType, listener);
}

/**
 * Send an event to all listeners for that type.
 *
 * @param eventType String indicating the event to trigger
 * 
 * @param e Object to pass to each listener (NOT copied -- be careful)
 */
G_NavWatcher.prototype.fire = function(eventType, e) {
  this.registrar_.fire(eventType, e);
}

/**
 * Helper function to determine whether a given URL is "spurious" for some
 * definition of "spurious".
 *
 * @param url String containing the URL to check
 * 
 * @returns Boolean indicating whether Fritz thinks it's too boring to notice
 */ 
G_NavWatcher.prototype.isSpurious_ = function(url) {
  return (url == "about:blank" ||
          url == "about:config" ||  
          url.indexOf("chrome://") == 0 ||
          url.indexOf("file://") == 0 ||
          url.indexOf("jar:") == 0);
}

/**
 * We do our dirtywork on state changes.
 */
G_NavWatcher.prototype.onStateChange = function(webProgress, 
                                                request, 
                                                stateFlags, 
                                                status) {
  
  var wp = Ci.nsIWebProgressListener;

  // Debugging stuff
  //   function D(msg) {
  //     dump(msg + "\n");
  //   };
  // 
  //   function w(s) {
  //     if (stateFlags & wp[s])
  //       D(s);
  //   };
  // 
  //   D("\nState change:");
  //   try {
  //     D("URL: " + request.name);
  //   } catch (e) {};
  //   w("STATE_IS_REQUEST");
  //   w("STATE_IS_DOCUMENT");
  //   w("STATE_IS_NETWORK");
  //   w("STATE_IS_WINDOW");
  //   w("STATE_STOP");
  //   w("STATE_START");
  //   w("STATE_REDIRECTING");
  //   w("STATE_TRANSFERRING");
  //   w("STATE_NEGOTIATING");
  
  // Thanks Darin for helping with this
  if (stateFlags & wp.STATE_START &&
      stateFlags & wp.STATE_IS_REQUEST &&
      request.loadFlags & Ci.nsIChannel.LOAD_DOCUMENT_URI) {

    var url;
    try {
      url = request.name;
    } catch(e) { return; }

    if (!this.filterSpurious_ || !this.isSpurious_(url)) {

      G_Debug(this, "firing docnavstart for " + url);
      var eventObj = { 
        "request": request, 
        "url": url, 
      };
      this.fire(this.events.DOCNAVSTART, eventObj);
    }
  }
}

// We don't care about the other kinds of updates, but we need to
// implement the interface anyway.
  
/**
 * NOP
 */
G_NavWatcher.prototype.onLocationChange = function(webProgress, 
                                                   request, 
                                                   location) { }

/**
 * NOP
 */
G_NavWatcher.prototype.onProgressChange = function(webProgress, 
                                                   request, 
                                                   curSelfProgress, 
                                                   maxSelfProgress, 
                                                   curTotalProgress, 
                                                   maxTotalProgress) { }

/**
 * NOP
 */
G_NavWatcher.prototype.onSecurityChange = function(webProgress, 
                                                   request, 
                                                   state) { }

/**
 * NOP
 */
G_NavWatcher.prototype.onStatusChange = function(webProgress, 
                                                 request, 
                                                 status, 
                                                 message) { }

/**
 * NOP
 */
G_NavWatcher.prototype.onLinkIconAvailable = function(browser, aHref) { }

