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


// This file implements a G_TabbedBrowserWatcher, an object
// encapsulating and abstracting the mechanics of working with tabs
// and the documents within them. The watcher provides notification of
// various DOM-related events (a document loaded, a document unloaded,
// tab was created/destroyed, user switched tabs, etc.) as well as
// commonly required methods (get me the current tab, find the tab to
// which this document belongs, etc.).
//
// This class does not do progresslistener-based notifications; for that,
// use the NavWatcher.
//
// Note: I use "browser" and "tab" interchangeably.
//
// This class adds a level of indirection to event registration. You
// initialize it with a tabbedbrowser, and then register to hear
// events from it instead of from the tabbedbrowser or browser itself.
// Your handlers are invoked with a custom object as an argument (see
// below). This object contains useful information such as a reference
// to the browser in which the event is happening and whether the
// event is occurring on the top-level document in that browser.
//
// The events you can register to hear are:
//
// EVENT             DESCRIPTION
// -----             -----------
// load              Fires when a page is shown in the browser window and
//                   this page wasn't in the bfcache
//
// unload            Fires when a page is nav'd away from in the browser window
//                   and the page isn't going into the bfcache
//
// pageshow          Fires when a page is shown in the browser window, whether
//                   it came from bfcache or not. (There is a "persisted"
//                   property we can get from the event object if we'd like.
//                   It indicates whether the page was loaded from bfcache.
//                   If false then we known load recently fired).
//
// pagehide          Fires when a page is nav'd away from in the browser,
//                   whether its going into the bfcache or not. (There is
//                   a persisted property here as well that we're not
//                   propagating -- when its true the page is going into
//                   the bfcache, else it's not, and unload will shortly
//                   fire).
//
// domcontentloaded  BROKEN BROKEN BROKEN BROKEN BROKEN BROKEN BROKEN BROKEN
//                   Fires when a doc's DOM is ready, but its externally linked
//                   content hasn't necessarily loaded. This event is
//                   currently broken: it doesn't fire when using the
//                   forward/back buttons in conjunction with the bfcache.
//                   Bryner is working on a fix.
//
// tabload           Fires when a new tab has been created (but doesn't
//                   necessarily have content loaded in it)
//
// tabunload         Fires when a tab is being destroyed (and might have had
//                   the content it contains destroyed)
//
// tabswitch         Fires when the user switches tabs
//
// tabmove           Fires when a user drags a tab to a different position
//
//
// For pageshow, pagehide, load, unload, and domcontentloaded, the event
// object you'll receive has the following properties:
//
//      doc -- reference to Document on which the event fired
//      browser -- the browser in which the document resides
//      isTop -- boolean indicating if it is the top-level document
//      inSelected -- boolean indicating if it is in the currently selected tab
//
// For tabload and unload it has:
//
//      browser -- reference to the browser that is loading or closing
//
// For tabswitch it has:
//
//      fromBrowser -- reference to the browser user is switching from
//      toBrowser -- reference to the browser user is switching to
//
// For tabmove it has:
//
//      tab -- the tab that was moved (Note that this is the actual
//             tab widget that holds the document title, not the
//             browser object.  We use this because it's the target
//             of the DOMNodeInserted event.)
//      fromIndex -- the tab index before the move
//      toIndex -- the tab index after the move
//
//
// The order of events is:                   tabload
//                                           domcontentloaded
//                                           load
//                                           pageshow
//                                           --  --
//                                           pagehide
//                                           unload
//                                           tabunload
//
// Example:
//
// function handler(e /*event object*/) {
//   foo(e.browser);
// };
// var watcher = new G_TabbedBrowserWatcher(document.getElementById(gBrowser));
// watcher.registerListener("load", handler);      // register for events
// watcher.removeListener("load", handler);        // unregister
//
//
// TODO, BUGS, ISSUES, COMPLICATIONS:
//
// The only major problem is in closing tabs:
//
//     + When you close a tab, the reference to the Document you get in the
//       unload event is undefined. We pass this along. This could easily
//       be fixed by not listening for unload at all, but instead inferring
//       it from the information in pagehide, and then firing our own "fake"
//       unload after firing pagehide.
//
//     + There's no docshell during the pagehide event, so we can't determine
//       if the document is top-level. We pass undefined in this case.
//
//     + Though the browser reference during tabunload will be valid, its
//       members most likely have already been torn down. Use it in an
//       objectsafemap to keep state if you need its members.
//
//     + The event listener DOMNodeInserted has the potential to cause
//       performance problems if there are too many events fired.  It
//       should be ok, since we inserted it as far as possible into
//       the xul tree.
//
//
// TODO: need better enforcement of unique names. Two tabbedbrowserwatchers
//       with the same name will clobber each other because they use that
//       name to mark browsers they've seen.
// 
// TODO: the functions that iterate of windows and documents badly need
//       to be made cleaner. Right now we have multiple implementations
//       that essentially do the same thing :(
//
// But good enough for government work.

/**
 * Encapsulates tab-related information. You can use the
 * G_TabbedBrowserWatcher to watch for events on tabs as well as to
 * retrieve tab-related data (such as what tab is currently showing).
 * It receives many event notifications from G_BrowserWatchers it
 * attaches to newly opening tabs.
 *
 * @param tabBrowser A reference to the tabbed browser you wish to watch.
 *
 * @param name String containing a probabilistically unique name. Used to
 *             ensure that each tabbedbrowserwatcher can uniquely mark
 *             browser it has "seen."
 *
 * @param opt_filterAboutBlank Boolean indicating whether to filter events
 *                             for about:blank. These events are often
 *                             spurious since about:blank is the default
 *                             page for an empty browser.
 *
 * @constructor
 */

function G_TabbedBrowserWatcher(tabBrowser, name, opt_filterAboutBlank) {
  this.debugZone = "tabbedbrowserwatcher";
  this.registrar_ = new EventRegistrar(G_TabbedBrowserWatcher.events);
  this.tabBrowser_ = tabBrowser;
  this.filterAboutBlank_ = !!opt_filterAboutBlank;
  this.events = G_TabbedBrowserWatcher.events;      // Convenience pointer

  // We need some way to tell if we've seen a browser before, so we
  // set a property on it with a probabilistically unique string. The
  // string is a combination of a static string and one passed in by
  // the caller.
  G_Assert(this, typeof name == "string" && !!name,
           "Need a probabilistically unique name");
  this.name_ = name;
  this.mark_ = G_TabbedBrowserWatcher.mark_ + "-" + this.name_;

  this.tabbox_ = this.getTabBrowser().mTabBox;

  // We watch for events occuring in previously unseen browsers at
  // this (the tabbedbrowser) level, and attach listeners to the new
  // browsers when we see them. In order to do this properly, we need
  // to watch for the earliest event we're interested in
  // (DOMContentLoaded), because otherwise we'd miss this event in newly
  // opening browsers. For example, if instead we hooked load here,
  // DOMContentLoaded would already have passed by the time we noticed
  // there was a new browser.

  this.onDOMContentLoadedClosure_ = BindToObject(this.onDOMContentLoaded, this)
  this.tabbox_.addEventListener("DOMContentLoaded",
                                this.onDOMContentLoadedClosure_, true);

  // We watch for DOM nodes inserted under the tabbox so we can detect when
  // a user drags a tab to a new location.
  this.onDOMNodeInsertedClosure_ = BindToObject(this.onDOMNodeInserted, this);
  this.tabbox_.addEventListener("DOMNodeInserted",
                                this.onDOMNodeInsertedClosure_, true);

  // There's no tabswitch event in Firefox, so we fake it by watching
  // for selects on the tabbox.
  this.onTabSwitchClosure_ = BindToObject(this.onTabSwitch, this);
  this.tabbox_.addEventListener("select",
                                this.onTabSwitchClosure_, true);

  // Used to determine when the user has switched tabs
  this.lastTab_ = this.getCurrentBrowser();

  // Ensure we hook a G_BrowserWatcher to all tabs that are open at startup
  this.detectNewTabs_();
}

// Events for which listeners can register
G_TabbedBrowserWatcher.events = {
   DOMCONTENTLOADED: "domcontentloaded",
   PAGESHOW: "pageshow",
   PAGEHIDE: "pagehide",
   LOAD: "load",
   UNLOAD: "unload",
   TABLOAD: "tabload",
   TABUNLOAD: "tabunload",
   TABSWITCH: "tabswitch",
   TABMOVE: "tabmove",
   };

// We mark new tabs as we see them
G_TabbedBrowserWatcher.mark_ = "watcher-marked";

/**
 * Remove all the event handlers and clean up circular refs.
 */
G_TabbedBrowserWatcher.prototype.shutdown = function() {
  G_Debug(this, "Removing event listeners");
  if (this.tabbox_) {
    this.tabbox_.removeEventListener("DOMContentLoaded",
                                     this.onDOMContentLoadedClosure_, true);
    this.tabbox_.removeEventListener("DOMNodeInserted",
                                     this.onDOMNodeInsertedClosure_, true);
    this.tabbox_.removeEventListener("select",
                                     this.onTabSwitchClosure_, true);
    // Break circular ref so we can be gc'ed.
    this.tabbox_ = null;
  }
  // Break circular ref so we can be gc'ed.
  if (this.lastTab_) {
    this.lastTab_ = null;
  }

  if (this.tabBrowser_) {
    this.tabBrowser_ = null;
  }
}

/**
 * Check to see if we've seen a browser before
 *
 * @param browser Browser to check
 * @returns Boolean indicating if we've attached a BrowserWatcher to this
 *          browser
 */
G_TabbedBrowserWatcher.prototype.isInstrumented_ = function(browser) {
  return !!browser[this.mark_];
}

/**
 * Attaches a BrowserWatcher to a browser and marks it as seen
 *
 * @param browser Browser to which to attach a G_BrowserWatcher
 */
G_TabbedBrowserWatcher.prototype.instrumentBrowser_ = function(browser) {
  G_Assert(this, !this.isInstrumented_(browser),
           "Browser already instrumented!");

  // The browserwatcher will hook itself into the browser and its parent (us)
  new G_BrowserWatcher(this, browser);
  browser[this.mark_] = true;
}

/**
 * Attach BrowserWatchers to all open, unseen tabs
 */
G_TabbedBrowserWatcher.prototype.detectNewTabs_ = function() {
  var tb = this.getTabBrowser();

  for (var i = 0; i < tb.browsers.length; ++i)
    this.maybeFireTabLoad(tb.browsers[i]);
}

/**
 * Register to receive events of a particular type
 *
 * @param eventType String indicating the event (see
 *                  G_TabbedBrowserWatcher.events)
 * @param listener Function to invoke when the event occurs. See top-
 *                 level comments for parameters.
 */
G_TabbedBrowserWatcher.prototype.registerListener = function(eventType,
                                                             listener) {
  this.registrar_.registerListener(eventType, listener);
}

/**
 * Unregister a listener.
 *
 * @param eventType String one of G_TabbedBrowserWatcher.events' members
 * @param listener Function to remove as listener
 */
G_TabbedBrowserWatcher.prototype.removeListener = function(eventType,
                                                           listener) {
  this.registrar_.removeListener(eventType, listener);
}

/**
 * Send an event to all listeners for that type.
 *
 * @param eventType String indicating the event to trigger
 * @param e Object to pass to each listener (NOT copied -- be careful)
 */
G_TabbedBrowserWatcher.prototype.fire = function(eventType, e) {
  this.registrar_.fire(eventType, e);
}

/**
 * Convenience function to send a document-related event. We use this
 * convenience function because the event constructing logic and
 * parameters are the same for all these events. (Document-related
 * events are load, unload, pagehide, pageshow, and domcontentloaded).
 *
 * @param eventType String indicating the type of event to fire (one of
 *                  the document-related events)
 *
 * @param doc Reference to the HTMLDocument the event is occuring to
 *
 * @param browser Reference to the browser in which the document is contained
 */
G_TabbedBrowserWatcher.prototype.fireDocEvent_ = function(eventType,
                                                          doc,
                                                          browser) {
  // If we've already shutdown, don't bother firing any events.
  if (!this.tabBrowser_) {
    G_Debug(this, "Firing event after shutdown: " + eventType);
    return;
  }

  try {
    // Could be that the browser's contentDocument has already been torn
    // down. If so, this throws, and we can't tell without keeping more
    // state whether doc was the top frame.
    var isTop = (doc == browser.contentDocument);
  } catch(e) {
    var isTop = undefined;
  }

  var inSelected = (browser == this.getCurrentBrowser());

  var location = doc ? doc.location.href : undefined;

  // Only send notifications for about:config's if we're supposed to
  if (!this.filterAboutBlank_ || location != "about:blank") {

    G_Debug(this, "firing " + eventType + " for " + location +
            (isTop ? " (isTop)" : "") + (inSelected ? " (inSelected)" : ""));
    this.fire(eventType, { "doc": doc,
                           "isTop": isTop,
                           "inSelected": inSelected,
                           "browser": browser});
  }
}

/**
 * Invoked on a browser to ensure we've seen it before. If we haven't,
 * the browser is instrumented and the tabload event is fired.
 *
 * @param browser Reference to the browser to check
 */
G_TabbedBrowserWatcher.prototype.maybeFireTabLoad = function(browser) {
  if (!this.isInstrumented_(browser)) {            // Is it a new browser?
    this.instrumentBrowser_(browser);              // Add a G_BrowserWatcher
    G_Debug(this, "firing tabload");
    // And shoot notification
    this.fire(this.events.TABLOAD, { "browser": browser });
  }
}

/**
 * Invoked when the document content has loaded for a document. Externally
 * linked in content might not yet have loaded.
 *
 * @param e Event object
 */
G_TabbedBrowserWatcher.prototype.onDOMContentLoaded = function(e) {
  G_Debug(this, "onDOMContentLoaded for a " + e.target);

  var doc = e.target;
  var browser = this.getBrowserFromDocument(doc);

  if (!browser) {
    G_Debug(this, "domcontentloaded: no browser for " + doc.location.href);
    return;
  }

  this.maybeFireTabLoad(browser);
  G_Debug(this, "DOMContentLoaded broken for forward/back buttons.");
  this.fireDocEvent_(this.events.DOMCONTENTLOADED, doc, browser);
}

/**
 * Invoked when a new xul node is inserted under the tabbox.  We use this
 * to detect tab moves.
 *
 * @param e Event object
 */
G_TabbedBrowserWatcher.prototype.onDOMNodeInserted = function(e) {
  G_Debug(this, "onDOMNodeInserted for a " + e.target +
          " related: " + e.relatedNode);

  // Ignore the node insertion if it isn't a tab
  if (e.target.localName != "tab") {
    return;
  }

  // If the tab was just inserted (it's a new tab, not a moved tab), the
  // pos value will be undefined.
  if (!isDef(e.target._tPos)) {
    return;
  }

  // Get the target tab's old position
  var fromPos = e.target._tPos;

  // Get the target tab's new position.
  // Would like to avoid a linear search through the tabs but I'm not sure
  // how to get around this.
  var toPos;
  for (var i = 0; i < e.relatedNode.childNodes.length; i++) {
    var child = e.relatedNode.childNodes[i];
    if (child == e.target) {
      toPos = i;
      break;
    }
  }

  G_Debug(this, "firing tabmove");
  this.fire(this.events.TABMOVE, { "tab": e.target,
                                   "fromIndex": fromPos,
                                   "toIndex": toPos } );
}

/**
 * Invoked when the user might have switched tabs
 *
 * @param e Event object
 */
G_TabbedBrowserWatcher.prototype.onTabSwitch = function(e) {
  // Filter spurious events
  // The event target is usually tabs but can be tabpanels when tabs were opened
  // programatically via tabbrowser.addTab().
  if (e.target == null || 
      (e.target.localName != "tabs" && e.target.localName != "tabpanels"))
    return;

  var fromBrowser = this.lastTab_;
  var toBrowser = this.getCurrentBrowser();

  if (fromBrowser != toBrowser) {
    this.lastTab_ = toBrowser;
    G_Debug(this, "firing tabswitch");
    this.fire(this.events.TABSWITCH, { "fromBrowser": fromBrowser,
                                       "toBrowser": toBrowser });
  }
}

// Utility functions

/**
 * Returns a reference to the tabbed browser this G_TabbedBrowserWatcher
 * was initialized with.
 */
G_TabbedBrowserWatcher.prototype.getTabBrowser = function() {
  return this.tabBrowser_;
}

/**
 * Returns a reference to the currently selected tab.
 */
G_TabbedBrowserWatcher.prototype.getCurrentBrowser = function() {
  return this.getTabBrowser().selectedBrowser;
}

/**
 * Returns a reference to the top window in the currently selected tab.
 */
G_TabbedBrowserWatcher.prototype.getCurrentWindow = function() {
  return this.getCurrentBrowser().contentWindow;
}

/**
 * Find the browser corresponding to a Document
 *
 * @param doc Document we want the browser for
 * @returns Reference to the browser in which the given document is found
 *          or null if not found
 */
G_TabbedBrowserWatcher.prototype.getBrowserFromDocument = function(doc) {
  // Could instead get the top window of the browser in which the doc
  // is found via doc.defaultView.top, but sometimes the document
  // isn't in a browser at all (it's being unloaded, for example), so
  // defaultView won't be valid.

  // Helper: return true if doc is a sub-document of win
  function docInWindow(doc, win) {
    if (win.document == doc)
      return true;

    if (win.frames)
      for (var i = 0; i < win.frames.length; i++)
        if (docInWindow(doc, win.frames[i]))
          return true;

    return false;
  }

  var browsers = this.getTabBrowser().browsers;
  for (var i = 0; i < browsers.length; i++)
    if (docInWindow(doc, browsers[i].contentWindow))
      return browsers[i];

  return null;
}

/**
 * Find the Document that has the given URL loaded. Returns on the
 * _first_ such document found, so be careful.
 *
 * TODO make doc/window searches more elegant, and don't use inner functions
 *
 * @param url String indicating the URL we're searching for
 * @param opt_browser Optional reference to a browser. If given, the
 *                    search will be confined to only this browser.
 * @returns Reference to the Document with that URL or null if not found
 */
G_TabbedBrowserWatcher.prototype.getDocumentFromURL = function(url,
                                                               opt_browser) {

  // Helper function: return the Document in win that has location of url
  function docWithURL(win, url) {
    if (win.document.location.href == url)
      return win.document;

    if (win.frames)
      for (var i = 0; i < win.frames.length; i++) {
        var rv = docWithURL(win.frames[i], url);
  	if (rv)
          return rv;
      }

    return null;
  }

  if (opt_browser)
    return docWithURL(opt_browser.contentWindow, url);

  var browsers = this.getTabBrowser().browsers;
  for (var i=0; i < browsers.length; i++) {
    var rv = docWithURL(browsers[i].contentWindow, url);
    if (rv)
      return rv;
  }

  return null;
}

/**
 * Find the all Documents that have the given URL loaded.
 *
 * TODO make doc/window searches more elegant, and don't use inner functions
 *
 * @param url String indicating the URL we're searching for
 *
 * @param opt_browser Optional reference to a browser. If given, the
 *                    search will be confined to only this browser.
 *
 * @returns Array of Documents with the given URL (zero length if none found)
 */
G_TabbedBrowserWatcher.prototype.getDocumentsFromURL = function(url,
                                                                opt_browser) {

  var docs = [];

  // Helper function: add Docs in win with the location of url
  function getDocsWithURL(win, url) {
    if (win.document.location.href == url)
      docs.push(win.document);

    if (win.frames)
      for (var i = 0; i < win.frames.length; i++)
        getDocsWithURL(win.frames[i], url);
  }

  if (opt_browser)
    return getDocsWithURL(opt_browser.contentWindow, url);

  var browsers = this.getTabBrowser().browsers;
  for (var i=0; i < browsers.length; i++)
    getDocsWithURL(browsers[i].contentWindow, url);

  return docs;
}


/**
 * Finds the browser in which a Window resides.
 *
 * @param sub Window to find
 * @returns Reference to the browser in which sub resides, else null
 *          if not found
 */
G_TabbedBrowserWatcher.prototype.getBrowserFromWindow = function(sub) {

  // Helpfer function: return true if sub is a sub-window of win
  function containsSubWindow(sub, win) {
    if (win == sub)
      return true;

    if (win.frames)
      for (var i=0; i < win.frames.length; i++)
        if (containsSubWindow(sub, win.frames[i]))
          return true;

    return false;
  }

  var browsers = this.getTabBrowser().browsers;
  for (var i=0; i < browsers.length; i++)
    if (containsSubWindow(sub, browsers[i].contentWindow))
      return browsers[i];

  return null;
}

/**
 * Finds the XUL <tab> tag corresponding to a given browser.
 *
 * @param tabBrowser Reference to the tabbed browser in which browser lives
 * @param browser Reference to the browser we wish to find the tab of
 * @returns Reference to the browser's tab element, or null
 * @static
 */
G_TabbedBrowserWatcher.getTabElementFromBrowser = function(tabBrowser,
                                                           browser) {

  for (var i=0; i < tabBrowser.browsers.length; i++)
    if (tabBrowser.browsers[i] == browser)
      return tabBrowser.mTabContainer.childNodes[i];

  return null;
}

if (G_GDEBUG) {
  G_debugService.loggifier.loggify(G_TabbedBrowserWatcher.prototype);
}


/**
 * The G_TabbedBrowserWatcher delegates watching most events in browsers
 * to this object. It calls into its parent (the G_TabbedBrowserWatcher)
 * to signal events and is garbage collected when the browser goes away
 * because we don't hold a reference to it.
 *
 * @constructor
 * @param tabbedBrowserWatcher The high-level watcher through which we
 *                             should send notifications
 * @param browser The browser to which we should attach
 */
function G_BrowserWatcher(tabbedBrowserWatcher, browser) {
  this.debugZone = "browserwatcher";
  this.parent_ = tabbedBrowserWatcher;
  this.browser_ = browser;

  G_Debug(this, "new G_BrowserWatcher");
  // Now register to hear most of the doc-related events
  this.onPageShowClosure_ = BindToObject(this.onPageShow, this);
  this.browser_.addEventListener("pageshow", this.onPageShowClosure_, true);

  this.onPageHideClosure_ = BindToObject(this.onPageHide, this);
  this.browser_.addEventListener("pagehide", this.onPageHideClosure_, true);

  this.onLoadClosure_ = BindToObject(this.onLoad, this);
  this.browser_.addEventListener("load", this.onLoadClosure_, true);

  this.onUnloadClosure_ = BindToObject(this.onUnload, this);
  this.browser_.addEventListener("unload", this.onUnloadClosure_, true);
}

/**
 * Invoked when pageshow fires
 *
 * @param e Event object passed in by event system
 */
G_BrowserWatcher.prototype.onPageShow = function(e) {
  G_Debug(this, "onPageShow for " + ((e.target) ? (e.target) : ("undefined")));

  if (e.target && e.target.nodeName == "#document") {
    var doc = e.target;
    this.parent_.fireDocEvent_(this.parent_.events.PAGESHOW,
                               doc,
                               this.browser_);
  }
}

/**
 * Invoked when load fires
 *
 * @param e Event object passed in by event system
 */
G_BrowserWatcher.prototype.onLoad = function(e) {
  G_Debug(this, "onLoad for a " + e.target);

  if (e.target.nodeName != "#document")
    return;

  var doc = e.target;
  this.parent_.fireDocEvent_(this.parent_.events.LOAD, doc, this.browser_);
}

/**
 * Invoked when unload fires
 *
 * @param e Event object passed in by event system
 */
G_BrowserWatcher.prototype.onUnload = function(e) {
  G_Debug(this, "onUnload for " + ((e.target) ? (e.target) : ("undefined")));

  var doc = e.target;

  // We get spurious unloads for non-docs :(
  if (doc && doc.nodeName == "#document")
    this.parent_.fireDocEvent_("unload", doc, this.browser_);


  if (!doc) {                // This is a closing tab
    G_Debug(this, "firing tabunload for a " + this.browser_ + "(" +
            this.browser_.nodename + ")");

    // fire tabunload event
    this.parent_.fire(this.parent_.events.TABUNLOAD,
                      { "browser": this.browser_ });
    // unregister event listeners
    this.browser_.removeEventListener("pageshow", this.onPageShowClosure_, true);
    this.browser_.removeEventListener("pagehide", this.onPageHideClosure_, true);
    this.browser_.removeEventListener("load", this.onLoadClosure_, true);
    this.browser_.removeEventListener("unload", this.onUnloadClosure_, true);
    this.parent_ = null;
    this.browser_ = null;
    G_Debug(this, "Removing event listeners");
  }
}

/**
 * Invoked when pagehide fires
 *
 * @param e Event object passed in by event system
 */
G_BrowserWatcher.prototype.onPageHide = function(e) {
  G_Debug(this, "onPageHide for a " + e.target + "(" +
          e.target.nodeName + ")");

  if (e.target.nodeName != "#document")   // Ignore non-documents
    return;

  var doc = e.target;
  this.parent_.fireDocEvent_(this.parent_.events.PAGEHIDE, doc, this.browser_);
}
