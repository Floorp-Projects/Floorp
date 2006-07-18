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


// Implementation of the warning message we show users when we
// notice navigation to a phishing page after it has loaded. The
// browser view encapsulates all the hide/show logic, so the displayer
// doesn't need to know when to display itself, only how.
//
// Displayers implement the following interface:
//
// start() -- fired to initialize the displayer (to make it active). When
//            called, this displayer starts listening for and responding to
//            events. At most one displayer per tab should be active at a 
//            time, and start() should be called at most once.
// declineAction() -- fired when the user declines the warning.
// acceptAction() -- fired when the user declines the warning
// explicitShow() -- fired when the user wants to see the warning again
// browserSelected() -- the browser is the top tab
// browserUnselected() -- the browser is no long the top tab
// done() -- clean up. May be called once (even if the displayer wasn't
//           activated).
//
// At the moment, all displayers share access to the same xul in 
// safebrowsing-overlay.xul. Hence the need for at most one displayer
// to be active per tab at a time.

/**
 * Factory that knows how to create a displayer appropriate to the
 * user's platform. We use a clunky canvas-based displayer for all
 * platforms until such time as we can overlay semi-transparent
 * areas over browser content.
 *
 * See the base object for a description of the constructor args
 *
 * @constructor
 */
function PROT_PhishMsgDisplayer(msgDesc, browser, doc, url) {

  // TODO: Change this to return a PhishMsgDisplayerTransp on windows
  // (and maybe other platforms) when Firefox 2.0 hits.

  return new PROT_PhishMsgDisplayerCanvas(msgDesc, browser, doc, url);
}


/**
 * Base class that implements most of the plumbing required to hide
 * and show a phishing warning. Subclasses implement the actual
 * showMessage and hideMessage methods. 
 *
 * This class is not meant to be instantiated directly.
 *
 * @param msgDesc String describing the kind of warning this is
 * @param browser Reference to the browser over which we display the msg
 * @param doc Reference to the document in which browser is found
 * @param url String containing url of the problem document
 * @constructor
 */
function PROT_PhishMsgDisplayerBase(msgDesc, browser, doc, url) {
  this.debugZone = "phishdisplayer";
  this.msgDesc_ = msgDesc;                                // currently unused
  this.browser_ = browser;
  this.doc_ = doc;
  this.url_ = url;

  // We'll need to manipulate the XUL in safebrowsing-overlay.xul
  this.messageId_ = "safebrowsing-palm-message";
  this.messageTailId_ = "safebrowsing-palm-message-tail-container";
  this.messageContentId_ = "safebrowsing-palm-message-content";
  this.extendedMessageId_ = "safebrowsing-palm-extended-message";
  this.showmoreLinkId_ = "safebrowsing-palm-showmore-link";
  this.urlbarIconId_ = "safebrowsing-urlbar-icon";
  this.refElementId_ = this.urlbarIconId_;

  // We use this to report user actions to the server
  this.reporter_ = new PROT_Reporter();

  // The active UI elements in our warning send these commands; bind them
  // to their handlers but don't register the commands until we start
  // (because another displayer might be active)
  this.commandHandlers_ = {
    "safebrowsing-palm-showmore":
      BindToObject(this.showMore_, this),
  };

  this.windowWatcher_ = 
    Components.classes["@mozilla.org/embedcomp/window-watcher;1"]
    .getService(Components.interfaces.nsIWindowWatcher);
}

/**
 * @returns The default background color of the browser
 */
PROT_PhishMsgDisplayerBase.prototype.getBackgroundColor_ = function() {
  var pref = Components.classes["@mozilla.org/preferences-service;1"].
             getService(Components.interfaces.nsIPrefBranch);
  return pref.getCharPref("browser.display.background_color");
}

/**
 * Fired when the user declines our warning. Report it!
 */
PROT_PhishMsgDisplayerBase.prototype.declineAction = function() {
  G_Debug(this, "User declined warning.");
  G_Assert(this, this.started_, "Decline on a non-active displayer?");
  this.reporter_.report("phishdecline", this.url_);

  this.messageShouldShow_ = false;
  if (this.messageShowing_)
    this.hideMessage_();
}

/**
 * Fired when the user accepts our warning
 */
PROT_PhishMsgDisplayerBase.prototype.acceptAction = function() {
  G_Assert(this, this.started_, "Accept on an unstarted displayer?");
  G_Assert(this, this.done_, "Accept on a finished displayer?");
  G_Debug(this, "User accepted warning.");
  this.reporter_.report("phishaccept", this.url_);

  var url = this.getMeOutOfHereUrl_();
  this.browser_.loadURI(url);
}

/**
 * Get the url for "Get me out of here."  This is the user's home page if
 * one is set, ow, about:blank.
 * @return String url
 */
PROT_PhishMsgDisplayerBase.prototype.getMeOutOfHereUrl_ = function() {
  // Try to get their homepage from prefs.
  var prefs = Cc["@mozilla.org/preferences-service;1"]
              .getService(Ci.nsIPrefService).getBranch(null);

  var url = "about:blank";
  try {
    url = prefs.getComplexValue("browser.startup.homepage",
                                Ci.nsIPrefLocalizedString).data;
  } catch(e) {
    G_Debug(this, "Couldn't get homepage pref: " + e);
  }
  
  return url;
}

/**
 * Invoked when the browser is resized
 */
PROT_PhishMsgDisplayerBase.prototype.onBrowserResized_ = function(event) {
  G_Debug(this, "Got resize for " + event.target);

  if (this.messageShowing_) {
    this.hideMessage_(); 
    this.showMessage_();
  }
}

/**
 * Invoked by the browser view when our browser is switched to
 */
PROT_PhishMsgDisplayerBase.prototype.browserSelected = function() {
  G_Assert(this, this.started_, "Displayer selected before being started???");

  // If messageshowing hasn't been set, then this is the first time this
  // problematic browser tab has been on top, so do our setup and show
  // the warning.
  if (this.messageShowing_ === undefined) {
    this.messageShouldShow_ = true;
  }

  this.hideLockIcon_();        // Comes back when we are unselected or unloaded
  this.addWarningInUrlbar_();  // Goes away when we are unselected or unloaded

  // messageShouldShow might be false if the user dismissed the warning, 
  // switched tabs, and then switched back. We're still active, but don't
  // want to show the warning again. The user can cause it to show by
  // clicking our icon in the urlbar.
  if (this.messageShouldShow_)
    this.showMessage_();
}

/**
 * Invoked to display the warning message explicitly, for example if the user
 * clicked the url warning icon.
 */
PROT_PhishMsgDisplayerBase.prototype.explicitShow = function() {
  this.messageShouldShow_ = true;
  if (!this.messageShowing_)
    this.showMessage_();
}

/** 
 * Invoked by the browser view when our browser is switched away from
 */
PROT_PhishMsgDisplayerBase.prototype.browserUnselected = function() {
  this.removeWarningInUrlbar_();
  this.unhideLockIcon_();
  if (this.messageShowing_)
    this.hideMessage_();
}

/**
 * Invoked to make this displayer active. The displayer will now start
 * responding to notifications such as commands and resize events. We
 * can't do this in the constructor because there might be many 
 * displayers instantiated waiting in the problem queue for a particular
 * browser (e.g., a page has multiple framed problem pages), and we
 * don't want them all responding to commands!
 *
 * Invoked zero (the page we're a warning for was nav'd away from
 * before it reaches the head of the problem queue) or one (we're
 * displaying this warning) times by the browser view.
 */
PROT_PhishMsgDisplayerBase.prototype.start = function() {
  G_Assert(this, this.started_ == undefined, "Displayer started twice?");
  this.started_ = true;

  this.commandController_ = new PROT_CommandController(this.commandHandlers_);
  this.doc_.defaultView.controllers.appendController(this.commandController_);

  // Add an event listener for when the browser resizes (e.g., user
  // shows/hides the sidebar).
  this.resizeHandler_ = BindToObject(this.onBrowserResized_, this);
  this.browser_.addEventListener("resize",
                                 this.resizeHandler_, 
                                 false);
}

/**
 * @returns Boolean indicating whether this displayer is currently
 *          active
 */
PROT_PhishMsgDisplayerBase.prototype.isActive = function() {
  return !!this.started_;
}

/**
 * Invoked by the browser view to clean up after the user is done 
 * interacting with the message. Should be called once by the browser
 * view. 
 */
PROT_PhishMsgDisplayerBase.prototype.done = function() {
  G_Assert(this, !this.done_, "Called done more than once?");
  this.done_ = true;

  // If the Document we're showing the warning for was nav'd away from
  // before we had a chance to get started, we have nothing to do.
  if (this.started_) {

    // If we were started, we must be the current problem, so these things
    // must be showing
    this.removeWarningInUrlbar_();
    this.unhideLockIcon_();

    // Could be though that they've closed the warning dialog
    if (this.messageShowing_)
      this.hideMessage_();

    if (this.resizeHandler_) {
      this.browser_.removeEventListener("resize", 
                                        this.resizeHandler_, 
                                        false);
      this.resizeHandler_ = null;
    }
    
    var win = this.doc_.defaultView;
    win.controllers.removeController(this.commandController_);
    this.commandController_ = null;
  }
}

/**
 * Helper function to remove a substring from inside a string.
 *
 * @param orig String to remove substring from
 * 
 * @param toRemove String to remove (if it is present)
 *
 * @returns String with the substring removed
 */
PROT_PhishMsgDisplayerBase.prototype.removeIfExists_ = function(orig,
                                                                toRemove) {
  var pos = orig.indexOf(toRemove);
  if (pos != -1)
    orig = orig.substring(0, pos) + orig.substring(pos + toRemove.length);

  return orig;
}

/**
 * We don't want to confuse users if they land on a phishy page that uses
 * SSL, so ensure that the lock icon never shows when we're showing our 
 * warning.
 */
PROT_PhishMsgDisplayerBase.prototype.hideLockIcon_ = function() {
  var lockIcon = this.doc_.getElementById("lock-icon");
  if (!lockIcon)
    return;
  lockIcon.hidden = true;
}

/**
 * Ensure they can see it after our warning is finished.
 */
PROT_PhishMsgDisplayerBase.prototype.unhideLockIcon_ = function() {
  var lockIcon = this.doc_.getElementById("lock-icon");
  if (!lockIcon)
    return;
  lockIcon.hidden = false;
}

/**
 * This method makes our warning icon visible in the location bar. It will
 * be removed only when the problematic document is navigated awy from 
 * (i.e., when done() is called), and not when the warning is dismissed.
 */
PROT_PhishMsgDisplayerBase.prototype.addWarningInUrlbar_ = function() {
  var urlbarIcon = this.doc_.getElementById(this.urlbarIconId_);
  if (!urlbarIcon)
    return;
  urlbarIcon.setAttribute('level', 'warn');
}

/**
 * Hides our urlbar icon
 */
PROT_PhishMsgDisplayerBase.prototype.removeWarningInUrlbar_ = function() {
  var urlbarIcon = this.doc_.getElementById(this.urlbarIconId_);
  if (!urlbarIcon)
    return;
  urlbarIcon.setAttribute('level', 'safe');
}

/**
 * VIRTUAL -- Displays the warning message
 */
PROT_PhishMsgDisplayerBase.prototype.showMessage_ = function() { };

/**
 * VIRTUAL -- Hide the warning message from the user.
 */
PROT_PhishMsgDisplayerBase.prototype.hideMessage_ = function() { };

/**
 * Reposition the message relative to refElement in the parent window
 *
 * @param message Reference to the element to position
 * @param tail Reference to the message tail
 * @param refElement Reference to element relative to which we position
 *                   ourselves
 */
PROT_PhishMsgDisplayerBase.prototype.adjustLocation_ = function(message,
                                                                tail,
                                                                refElement) {
  var refX = refElement.boxObject.x;
  var refY = refElement.boxObject.y;
  var refHeight = refElement.boxObject.height;
  var refWidth = refElement.boxObject.width;
  G_Debug(this, "Ref element is at [window-relative] (" + refX + ", " + 
          refY + ")");

  var pixelsIntoRefY = -2;
  var tailY = refY + refHeight - pixelsIntoRefY;
  var tailPixelsLeftOfRefX = tail.boxObject.width;
  var tailPixelsIntoRefX = Math.round(refWidth / 2);
  var tailX = refX - tailPixelsLeftOfRefX + tailPixelsIntoRefX;

  // Move message up a couple pixels so the tail overlaps it.
  var messageY = tailY + tail.boxObject.height - 2;
  var messagePixelsLeftOfRefX = 375;
  var messageX = refX - messagePixelsLeftOfRefX;
  G_Debug(this, "Message is at [window-relative] (" + messageX + ", " + 
          messageY + ")");
  G_Debug(this, "Tail is at [window-relative] (" + tailX + ", " + 
          tailY + ")");

  tail.style.top = tailY + "px";
  tail.style.left = tailX + "px";
  message.style.top = messageY + "px";
  message.style.left = messageX + "px";
}

/**
 * Position the warning bubble with no reference element.  In this case we
 * just center the warning bubble at the top of the users window.
 * @param message XULElement message bubble XUL container.
 */
PROT_PhishMsgDisplayerBase.prototype.adjustLocationFloating_ = function(message) {
  // Compute X offset
  var browserX = this.browser_.boxObject.x;
  var browserXCenter = browserX + this.browser_.boxObject.width / 2;
  var messageX = browserXCenter - (message.boxObject.width / 2);

  // Compute Y offset (top of the browser window)
  var messageY = this.browser_.boxObject.y;

  // Position message
  message.style.top = messageY + "px";
  message.style.left = messageX + "px";
}

/**
 * Show the extended warning message
 */
PROT_PhishMsgDisplayerBase.prototype.showMore_ = function() {
  this.doc_.getElementById(this.extendedMessageId_).hidden = false;
  this.doc_.getElementById(this.showmoreLinkId_).style.display = "none";
}

/**
 * The user clicked on one of the links in the buble.  Display the
 * corresponding page in a new window with all the chrome enabled.
 *
 * @param url The URL to display in a new window
 */
PROT_PhishMsgDisplayerBase.prototype.showURL_ = function(url) {
  this.windowWatcher_.openWindow(this.windowWatcher_.activeWindow,
                                 url,
                                 "_blank",
                                 null,
                                 null);
}

/**
 * If the warning bubble came up in error, this url goes to a form
 * to notify the data provider.
 * @return url String
 */
PROT_PhishMsgDisplayerBase.prototype.getReportErrorURL_ = function() {
  var badUrl = this.url_;

  var url = gDataProvider.getReportErrorURL();
  url += "&url=" + encodeURIComponent(badUrl);
  return url;
}

/**
 * URL for the user to report back to us.  This is to provide the user
 * with an action after being warned.
 */
PROT_PhishMsgDisplayerBase.prototype.getReportGenericURL_ = function() {
  var badUrl = this.url_;

  var url = gDataProvider.getReportGenericURL();
  url += "&url=" + encodeURIComponent(badUrl);
  return url;
}


/**
 * A specific implementation of the dislpayer using a canvas. This
 * class is meant for use on platforms that don't support transparent
 * elements over browser content (currently: all platforms). 
 *
 * The main ugliness is the fact that we're hiding the content area and
 * painting the page to canvas. As a result, we must periodically
 * re-paint the canvas to reflect updates to the page. Otherwise if
 * the page was half-loaded when we showed our warning, it would
 * stay that way even though the page actually finished loading. 
 *
 * See base constructor for full details of constructor args.
 *
 * @constructor
 */
function PROT_PhishMsgDisplayerCanvas(msgDesc, browser, doc, url) {
  PROT_PhishMsgDisplayerBase.call(this, msgDesc, browser, doc, url);

  this.dimAreaId_ = "safebrowsing-dim-area-canvas";
  this.pageCanvasId_ = "safebrowsing-page-canvas";
  this.xhtmlNS_ = "http://www.w3.org/1999/xhtml";     // we create html:canvas
}

PROT_PhishMsgDisplayerCanvas.inherits(PROT_PhishMsgDisplayerBase);

/**
 * Displays the warning message.  First we make sure the overlay is loaded
 * then call showMessageAfterOverlay_.
 */
PROT_PhishMsgDisplayerCanvas.prototype.showMessage_ = function() {
  G_Debug(this, "Showing message.");

  // Load the overlay if we haven't already.
  var dimmer = this.doc_.getElementById('safebrowsing-dim-area-canvas');
  if (!dimmer) {
    var onOverlayMerged = BindToObject(this.showMessageAfterOverlay_,
                                       this);
    var observer = new G_ObserverWrapper("xul-overlay-merged",
                                         onOverlayMerged);

    this.doc_.loadOverlay(
        "chrome://browser/content/safebrowsing/warning-overlay.xul",
        observer);
  } else {
    // The overlay is already loaded so we go ahead and call
    // showMessageAfterOverlay_.
    this.showMessageAfterOverlay_();
  }
}

/**
 * This does the actual work of showing the warning message.
 */
PROT_PhishMsgDisplayerCanvas.prototype.showMessageAfterOverlay_ = function() {
  this.messageShowing_ = true;

  // Position the canvas overlay. Order here is significant, but don't ask me
  // why for some of these. You need to:
  // 1. get browser dimensions
  // 2. add canvas to the document
  // 3. unhide the dimmer (gray out overlay)
  // 4. display to the canvas
  // 5. unhide the warning message
  // 6. update link targets in warning message
  // 7. focus the warning message

  // (1)
  var w = this.browser_.boxObject.width;
  G_Debug(this, "browser w=" + w);
  var h = this.browser_.boxObject.height;
  G_Debug(this, "browser h=" + h);
  var x = this.browser_.boxObject.x;
  G_Debug(this, "browser x=" + w);
  var y = this.browser_.boxObject.y;
  G_Debug(this, "browser y=" + h);

  var win = this.browser_.contentWindow;
  var scrollX = win.scrollX;
  G_Debug(this, "win scrollx=" + scrollX);
  var scrollY = win.scrollY;
  G_Debug(this, "win scrolly=" + scrollY);

  // (2)
  // We add the canvas dynamically and remove it when we're done because
  // leaving it hanging around consumes a lot of memory.
  var pageCanvas = this.doc_.createElementNS(this.xhtmlNS_, "html:canvas");
  pageCanvas.id = this.pageCanvasId_;
  pageCanvas.style.left = x + 'px';
  pageCanvas.style.top = y + 'px';

  var dimarea = this.doc_.getElementById(this.dimAreaId_);
  this.doc_.getElementById('main-window').insertBefore(pageCanvas,
                                                       dimarea);

  // (3)
  dimarea.style.left = x + 'px';
  dimarea.style.top = y + 'px';
  dimarea.style.width = w + 'px';
  dimarea.style.height = h + 'px';
  dimarea.hidden = false;
  
  // (4)
  pageCanvas.setAttribute("width", w);
  pageCanvas.setAttribute("height", h);

  var bgcolor = this.getBackgroundColor_();

  var cx = pageCanvas.getContext("2d");
  cx.drawWindow(win, scrollX, scrollY, w, h, bgcolor);

  // Now repaint the window every so often in case the content hasn't fully
  // loaded at this point.
  var debZone = this.debugZone;
  function repaint() {
    G_Debug(debZone, "Repainting canvas...");
    cx.drawWindow(win, scrollX, scrollY, w, h, bgcolor);
  };
  this.repainter_ = new PROT_PhishMsgCanvasRepainter(repaint);

  // (5)
  this.showAndPositionWarning_();

  // (6)
  var link = this.doc_.getElementById('safebrowsing-palm-falsepositive-link');
  link.href = this.getReportErrorURL_();
  link = this.doc_.getElementById('safebrowsing-palm-report-link');
  link.href = this.getReportGenericURL_();

  // (7)
  this.doc_.getElementById(this.messageContentId_).focus();
}

/**
 * Show and position the warning message.  We position the waring message
 * relative to the icon in the url bar, but if the element doesn't exist,
 * (e.g., the user remove the url bar from her/his chrome), we anchor at the
 * top of the window.
 */
PROT_PhishMsgDisplayerCanvas.prototype.showAndPositionWarning_ = function() {
  var refElement = this.doc_.getElementById(this.refElementId_);
  var message = this.doc_.getElementById(this.messageId_);
  var tail = this.doc_.getElementById(this.messageTailId_);

  message.hidden = false;
  message.style.display = "block";

  // Determine if the refElement is visible.
  if (this.isVisibleElement_(refElement)) {
    // Show tail and position warning relative to refElement.
    tail.hidden = false;
    tail.style.display = "block";
    this.adjustLocation_(message, tail, refElement);
  } else {
    // No ref element, position in the top center of window.
    tail.hidden = true;
    tail.style.display = "none";
    this.adjustLocationFloating_(message);
  }
}

/**
 * @return Boolean true if elt is a visible XUL element.
 */
PROT_PhishMsgDisplayerCanvas.prototype.isVisibleElement_ = function(elt) {
  if (!elt)
    return false;
  
  // If it's on a collapsed/hidden toolbar, the x position is set to 0.
  if (elt.boxObject.x == 0)
    return false;

  return true;
}

/**
 * Hide the warning message from the user.
 */
PROT_PhishMsgDisplayerCanvas.prototype.hideMessage_ = function() {
  G_Debug(this, "Hiding phishing warning.");
  G_Assert(this, this.messageShowing_, "Hide message called but not showing?");

  this.messageShowing_ = false;
  this.repainter_.cancel();
  this.repainter_ = null;

  // Hide the warning popup.
  var message = this.doc_.getElementById(this.messageId_);
  message.hidden = true;
  message.style.display = "none";

  var tail = this.doc_.getElementById(this.messageTailId_);
  tail.hidden = true;
  tail.style.display = "none";

  // Remove the canvas element from the chrome document.
  var pageCanvas = this.doc_.getElementById(this.pageCanvasId_);
  pageCanvas.parentNode.removeChild(pageCanvas);

  // Hide the dimmer.
  var dimarea = this.doc_.getElementById(this.dimAreaId_);
  dimarea.hidden = true;
}


/**
 * Helper class that periodically repaints the canvas. We repaint
 * frequently at first, and then back off to a less frequent schedule
 * at "steady state," and finally just stop altogether. We have to do
 * this because we're not sure if the page has finished loading when
 * we first paint the canvas, and because we want to reflect any
 * dynamically written content into the canvas as it appears in the
 * page after load.
 *
 * @param repaintFunc Function to call to repaint browser.
 *
 * @constructor
 */
function PROT_PhishMsgCanvasRepainter(repaintFunc) {
  this.count_ = 0;
  this.repaintFunc_ = repaintFunc;
  this.initPeriodMS_ = 500;             // Initially repaint every 500ms
  this.steadyStateAtMS_ = 10 * 1000;    // Go slowly after 10 seconds,
  this.steadyStatePeriodMS_ = 3 * 1000; // repainting every 3 seconds, and
  this.quitAtMS_ = 20 * 1000;           // stop after 20 seconds
  this.startMS_ = (new Date).getTime();
  this.alarm_ = new G_Alarm(BindToObject(this.repaint, this), 
                            this.initPeriodMS_);
}

/**
 * Called periodically to repaint the canvas
 */
PROT_PhishMsgCanvasRepainter.prototype.repaint = function() {
  this.repaintFunc_();

  var nextRepaint;
  // If we're in "steady state", use the slow repaint rate, else fast
  if ((new Date).getTime() - this.startMS_ > this.steadyStateAtMS_)
    nextRepaint = this.steadyStatePeriodMS_;
  else 
    nextRepaint = this.initPeriodMS_;

  if (!((new Date).getTime() - this.startMS_ > this.quitAtMS_))
    this.alarm_ = new G_Alarm(BindToObject(this.repaint, this), nextRepaint);
}

/**
 * Called to stop repainting the canvas
 */
PROT_PhishMsgCanvasRepainter.prototype.cancel = function() {
  if (this.alarm_) {
    this.alarm_.cancel();
    this.alarm_ = null;
  }
  this.repaintFunc_ = null;
}
