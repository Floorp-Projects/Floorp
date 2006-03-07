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

// There is one BrowserView per browser window, and each BrowserView
// is responsible for keeping track of problems (phishy documents)
// within that window. The BrowserView is also responsible for
// figuring out what to do about such problems, for example, whether
// the tab with a phishy page is currently showing and therefore if we
// should be showing a warning.
// 
// The BrowserView receives information from three places:
//
// - from the phishing warden. When the phishing warden notices a
//   problem, it queries all browser views to see which one (if any)
//   has the Document that is problematic. It then hands the problem
//   off to the appropriate BrowserView.
// 
// - from the controller. The controller responds to explicit user 
//   actions (tab switches, requests to hide the warning message, 
//   etc.) and let's the BrowserView know about any user action 
//   having to do with the problems it is tracking.
//
// - from the TabbedBrowserWatcher. When the BrowserView is keeping
//   track of a problematic document it listens for interesting
//   events affecting it, for example pagehide (at which point
//   we presumably hide the warning if we're showing it).
//
// The BrowserView associates at most one "problem" with each Document
// in the browser window. It keeps state about which Documents are 
// problematic by storing a "problem queue" on each browser (tab).
// At most one problematic document per browser (tab) is active
// at any time. That is, we show the warning for at most one phishy
// document at any one time. If another phishy doc loads in that tab,
// it goes onto the end of the queue to be activated only when the
// currently active document goes away.
//
// If we had multiple types of warnings (one for after the page had
// loaded, one for when the user clicked a link, etc) here's where
// we'd select the appropate one to use. As it stands, we only have
// one displayer (an "afterload" displayer). A displayer knows _how_
// to display a warning, whereas as the BrowserView knows _what_ and
// _when_.
//
// To keep things (relatively) easy to reason about and efficient (the
// phishwarden could be querying us inside a progresslistener
// notification, or the controller inside an event handler), we have
// the following rules:
//
// - at most one of a displayer's start() or stop() methods is called
//   in any iteration (if calling two is required, the second is run in 
//   the next event loop)
// - displayers should run their operations synchronously so we don't have
//   to look two places (here and in the displayer) to see what is happening 
//   when
// - displayer actions are run after cleaning up the browser view state
//   in case they have consequences
//
// TODO: this could use some redesign, but I don't have time.
// TODO: the queue needs to be abstracted, but we want another release fast,
//       so I'm not going to touch it for the time being
// TODO: IDN issues and canonical URLs?
// TODO: Perhaps we should blur the page before showing a warning in order
//       to prevent stray keystrokes?

/**
 * The BrowerView is responsible for keeping track of and figuring out
 * what to do with problems within a single browser window.
 * 
 * TODO 
 * Unify all browser-related state here. Currently it's split
 * between two objects, this object and the controller. We could have
 * this object be solely responsible for UI hide/show decisions, which
 * would probably make it easier to reason about what's going on.
 * 
 * TODO 
 * Investigate an alternative model. For example, we could factor out
 * the problem signaling stuff from the tab/UI logic into a
 * ProblemRegistry. Attach listeners to new docs/requests as they go
 * by and have these listeners periodically check in with a
 * ProblemRegistry to see if they're watching a problematic
 * doc/request. If so, then have them flag the browser view to be
 * aware of the problem.
 *
 * @constructor
 * @param tabWatcher Reference to the TabbedBrowserWatcher we'll use to query 
 *                   for information about active tabs/browsers.
 * @param doc Reference to the XUL Document (browser window) in which the 
 *            tabwatcher is watching
 */ 
function PROT_BrowserView(tabWatcher, doc) {
  this.debugZone = "browserview";
  this.tabWatcher_ = tabWatcher;
  this.doc_ = doc;
}

/**
 * Invoked by the warden to give us the opportunity to handle a
 * problem.  A problem is signaled once per request for a problem
 * Document and is handled at most once, so there's no issue with us
 * "losing" a problem due to multiple concurrently loading Documents
 * with the same URL.
 *
 * @param warden Reference to the warden signalling the problem. We'll
 *               need him to instantiate one of his warning displayers
 * 
 * @param request The nsIRequest that is problematic
 *
 * @returns Boolean indicating whether we handled problem
 */
PROT_BrowserView.prototype.tryToHandleProblemRequest = function(warden,
                                                                request) {

  var doc = this.getFirstUnhandledDocWithURL_(request.name);
  if (doc) {
    var browser = this.tabWatcher_.getBrowserFromDocument(doc);
    G_Assert(this, !!browser, "Couldn't get browser from problem doc???");
    G_Assert(this, !this.getProblem_(doc, browser),
             "Doc is supposedly unhandled, but has state?");
    
    this.isProblemDocument_(browser, doc, warden);
    return true;
  }
  return false;
}

/**
 * See if we have any Documents with a given (problematic) URL that
 * haven't yet been marked as problems. Called as a subroutine by
 * tryToHandleProblemRequest().
 *
 * @param url String containing the URL to look for
 *
 * @returns Reference to an unhandled Document with the problem URL or null
 */
PROT_BrowserView.prototype.getFirstUnhandledDocWithURL_ = function(url) {
  var docs = this.tabWatcher_.getDocumentsFromURL(url);
  if (!docs.length)
    return null;

  for (var i = 0; i < docs.length; i++) {
    var browser = this.tabWatcher_.getBrowserFromDocument(docs[i]);
    G_Assert(this, !!browser, "Found doc but can't find browser???");
    var alreadyHandled = this.getProblem_(docs[i], browser);

    if (!alreadyHandled)
      return docs[i];
  }
  return null;
}

/**
 * We're sure a particular Document is problematic, so let's instantiate
 * a dispalyer for it and add it to the problem queue for the browser.
 *
 * @param browser Reference to the browser in which the problem doc resides
 *
 * @param doc Reference to the problematic document
 * 
 * @param warden Reference to the warden signalling the problem.
 */
PROT_BrowserView.prototype.isProblemDocument_ = function(browser, 
                                                         doc, 
                                                         warden) {

  G_Debug(this, "Document is problem: " + doc.location.href);
 
  var url = doc.location.href;

  // We only have one type of displayer right now
  var displayer = new warden.displayers_["afterload"]("Phishing afterload",
                                                      browser,
                                                      this.doc_,
                                                      url);

  // We listen for the problematic document being navigated away from
  // so we can remove it from the problem queue

  var hideHandler = BindToObject(this.onNavAwayFromProblem_, 
                                 this, 
                                 doc, 
                                 browser);
  doc.defaultView.addEventListener("pagehide", hideHandler, true);

  // More info than we technically need, but it comes in handy for debugging
  var problem = {
    "browser_": browser,
    "doc_": doc,
    "displayer_": displayer,
    "url_": url,
    "hideHandler_": hideHandler,
  };
  var numInQueue = this.queueProblem_(browser, problem);

  // If the queue was empty, schedule us to take something out
  if (numInQueue == 1)
    new G_Alarm(BindToObject(this.unqueueNextProblem_, this, browser), 0);
}

/**
 * Invoked when a problematic document is navigated away from. 
 *
 * @param doc Reference to the problematic Document navigated away from

 * @param browser Reference to the browser in which the problem document
 *                unloaded
 */
PROT_BrowserView.prototype.onNavAwayFromProblem_ = function(doc, browser) {

  G_Debug(this, "User nav'd away from problem.");
  var problem = this.getProblem_(doc, browser);
  (new PROT_Reporter).report("phishnavaway", problem.url_);

  G_Assert(this, doc === problem.doc_, "State doc not equal to nav away doc?");
  G_Assert(this, browser === problem.browser_, 
           "State browser not equal to nav away browser?");
  
  this.problemResolved(browser, doc);
}

/**
 * @param browser Reference to a browser we'd like to know about
 * 
 * @returns Boolean indicating if the browser in question has 
 *          problematic content
 */
PROT_BrowserView.prototype.hasProblem = function(browser) {
  return this.hasNonemptyProblemQueue_(browser);
}

/**
 * @param browser Reference to a browser we'd like to know about
 *
 * @returns Boolean indicating if the browser in question has a
 *          problem (i.e., it has a non-empty problem queue)
 */
PROT_BrowserView.prototype.hasNonemptyProblemQueue_ = function(browser) {
  try {
    return !!browser.PROT_problemState__ && 
      !!browser.PROT_problemState__.length;
  } catch(e) {
    // We could be checking a browser that has just been closed, in
    // which case its properties will not be valid, causing the above
    // statement to throw an error. Since this case handled elsewhere,
    // just return false.
    return false;
  }
}

/**
 * Invoked to indicate that the problem for a particular problematic
 * document in a browser has been resolved (e.g., by being navigated
 * away from).
 *
 * @param browser Reference to the browser in which resolution is happening
 *
 * @param opt_doc Reference to the problematic doc whose problem was resolved
 *                (if absent, assumes the doc assocaited with the currently
 *                active displayer)
 */
PROT_BrowserView.prototype.problemResolved = function(browser, opt_doc) {
  var problem;
  var doc;
  if (!!opt_doc) {
    doc = opt_doc;
    problem = this.getProblem_(doc, browser);
  } else {
    problem = this.getCurrentProblem_(browser);
    doc = problem.doc_;
  }

  problem.displayer_.done();
  var wasHead = this.deleteProblemFromQueue_(doc, browser);

  // Peek at the next problem (if any) in the queue for this browser
  var queueNotEmpty = this.getCurrentProblem_(browser);

  if (wasHead && queueNotEmpty) {
    G_Debug(this, "More problems pending. Scheduling unqueue.");
    new G_Alarm(BindToObject(this.unqueueNextProblem_, this, browser), 0);
  }
}

/**
 * Peek at the top of the problem queue and if there's something there,
 * make it active. 
 *
 * @param browser Reference to the browser we should activate a problem
 *                displayer in if one is available
 */
PROT_BrowserView.prototype.unqueueNextProblem_ = function(browser) {
  var problem = this.getCurrentProblem_(browser);
  if (!problem) {
    G_Debug(this, "No problem in queue; doc nav'd away from? (shrug)");
    return;
  }

  // Two problem docs that load in rapid succession could both schedule 
  // themselves to be unqueued before this method is called. So ensure 
  // that the problem at the head of the queue is not, in fact, active.
  if (!problem.displayer_.isActive()) {

    // It could be the case that the server is really slow to respond,
    // so there might not yet be anything in the problem Document. If
    // we show the warning when that's the case, the user will see a
    // blank document greyed out, and if they cancel the dialog
    // they'll see the page they're navigating away from because it
    // hasn't been painted over yet (b/c there's no content for the
    // problem page). So here we ensure that we have content for the
    // problem page before showing the dialog.
    var haveContent = false;
    try {
      // This will throw if there's no content yet
      var h = problem.doc_.defaultView.getComputedStyle(problem.doc_.body, "")
              .getPropertyValue("height");
      G_Debug(this, "body height: " + h);

      if (Number(h.substring(0, h.length - 2)))
        haveContent = true;

    } catch (e) {
      G_Debug(this, "Masked in unqueuenextproblem: " + e);
    }
    
    if (!haveContent) {

      G_Debug(this, "Didn't get computed style. Re-queueing.");

      // One stuck problem document in a page shouldn't prevent us
      // warning on other problem frames that might be loading or
      // loaded. So stick the Document that doesn't have content
      // back at the end of the queue.
      var p = this.removeProblemFromQueue_(problem.doc_, browser);
      G_Assert(this, p === problem, "Unqueued wrong problem?");
      this.queueProblem_(browser, problem);

      // Try again in a bit. This opens us up to a potential
      // vulnerability (put tons of hanging frames in a page
      // ahead of your real phishy frame), but the risk at the
      // moment is really low (plus it is outside our threat
      // model).
      new G_Alarm(BindToObject(this.unqueueNextProblem_, 
                               this, 
                               browser),
                  200 /*ms*/);
      return;
    }

    problem.displayer_.start();

    // OK, we have content, but there there is an additional
    // issue. Due to a bfcache bug, if we show the warning during
    // paint suppression, the collapsing of the content pane affects
    // the doc we're naving from :( The symptom is a page with grey
    // screen on navigation to or from a phishing page (the
    // contentDocument will have width zero).
    //
    // Paint supression lasts at most 250ms from when the parser sees
    // the body, and the parser sees the body well before it has a
    // height. We err on the side of caution.
    //
    // Thanks to bryner for helping to track the bfcache bug down.
    // https://bugzilla.mozilla.org/show_bug.cgi?id=319646
    
    if (this.tabWatcher_.getCurrentBrowser() === browser)
      new G_Alarm(BindToObject(this.problemBrowserMaybeSelected, 
                               this, 
                               browser),
                  350 /*ms*/);
  }
}

/**
 * Helper function that adds a new problem to the queue of problems pending
 * on this browser.
 *
 * @param browser Browser to which we should add state
 *
 * @param problem Object (structure, really) encapsulating the problem
 *
 * @returns Number indicating the number of items in the queue (and from
 *          which you can infer whether the recently added item was
 *          placed at the head, and hence should be active.
 */
PROT_BrowserView.prototype.queueProblem_ = function(browser, problem) {
  G_Debug(this, "Adding problem state for " + problem.url_);

  if (this.hasNonemptyProblemQueue_(browser))
    G_Debug(this, "Already has problem state. Queueing this problem...");

  // First problem ever signaled on this browser? Make a new queue!
  if (browser.PROT_problemState__ == undefined)
    browser.PROT_problemState__ = [];

  browser.PROT_problemState__.push(problem);
  return browser.PROT_problemState__.length;
}

/**
 * Helper function that removes a problem from the queue and deactivates
 * it.
 *
 * @param doc Reference to the doc for which we should remove state
 *
 * @param browser Reference to the browser from which we should remove
 *                state
 *
 * @returns Boolean indicating if the remove problem was currently active
 *          (that is, if it was at the head of the queue)
 */
PROT_BrowserView.prototype.deleteProblemFromQueue_ = function(doc, browser) {
  G_Debug(this, "Deleting problem state for " + browser);
  G_Assert(this, !!this.hasNonemptyProblemQueue_(browser),
           "Browser has no problem state");

  var problem = this.getProblem_(doc, browser);
  G_Assert(this, !!problem, "Couldnt find state in removeproblemstate???");

  var wasHead = browser.PROT_problemState__[0] === problem;
  this.removeProblemFromQueue_(doc, browser);

  var hideHandler = problem.hideHandler_;
  G_Assert(this, !!hideHandler, "No hidehandler in state?");
  problem.doc_.defaultView.removeEventListener("pagehide",
                                               hideHandler,
                                               true);
  return wasHead;
}

/**
 * Helper function that removes a problem from the queue but does 
 * NOT deactivate it.
 *
 * @param doc Reference to the doc for which we should remove state
 *
 * @param browser Reference to the browser from which we should remove
 *                state
 *
 * @returns Boolean indicating if the remove problem was currently active
 *          (that is, if it was at the head of the queue)
 */
PROT_BrowserView.prototype.removeProblemFromQueue_ = function(doc, browser) {
  G_Debug(this, "Removing problem state for " + browser);
  G_Assert(this, !!this.hasNonemptyProblemQueue_(browser),
           "Browser has no problem state");

  var problem = null;
  // TODO Blech. Let's please have an abstraction here instead.
  for (var i = 0; i < browser.PROT_problemState__.length; i++)
    if (browser.PROT_problemState__[i].doc_ === doc) {
      problem = browser.PROT_problemState__.splice(i, 1)[0];
      break;
    }
  return problem;
}

/**
 * Retrieve (but do not remove) the problem state for a particular
 * problematic Document in this browser
 *
 * @param doc Reference to the problematic doc to get state for
 *
 * @param browser Reference to the browser from which to get state
 *
 * @returns Object encapsulating the state we stored, or null if none
 */
PROT_BrowserView.prototype.getProblem_ = function(doc, browser) {
  if (!this.hasNonemptyProblemQueue_(browser))
    return null;

  // TODO Blech. Let's please have an abstraction here instead.
  for (var i = 0; i < browser.PROT_problemState__.length; i++)
    if (browser.PROT_problemState__[i].doc_ === doc)
      return browser.PROT_problemState__[i];
  return null;
}

/**
 * Retrieve the problem state for the currently active problem Document 
 * in this browser
 *
 * @param browser Reference to the browser from which to get state
 *
 * @returns Object encapsulating the state we stored, or null if none
 */
PROT_BrowserView.prototype.getCurrentProblem_ = function(browser) {
  return browser.PROT_problemState__[0];
}

/**
 * Invoked by the controller when the user switches tabs away from a problem 
 * tab. 
 *
 * @param browser Reference to the tab that was switched from
 */
PROT_BrowserView.prototype.problemBrowserUnselected = function(browser) {
  var problem = this.getCurrentProblem_(browser);
  G_Assert(this, !!problem, "Couldn't get state from browser");
  problem.displayer_.browserUnselected();
}

/**
 * Checks to see if the problem browser is selected, and if so, 
 * tell it it to show its warning.
 *
 * @param browser Reference to the browser we wish to check
 */
PROT_BrowserView.prototype.problemBrowserMaybeSelected = function(browser) {
  var problem = this.getCurrentProblem_(browser);

  if (this.tabWatcher_.getCurrentBrowser() === browser &&
      problem &&
      problem.displayer_.isActive()) 
    this.problemBrowserSelected(browser);
}

/**
 * Invoked by the controller when the user switches tabs to a problem tab
 *
 * @param browser Reference to the tab that was switched to
 */
PROT_BrowserView.prototype.problemBrowserSelected = function(browser) {
  G_Debug(this, "Problem browser selected");
  var problem = this.getCurrentProblem_(browser);
  G_Assert(this, !!problem, "No state? But we're selected!");
  problem.displayer_.browserSelected();
}

/**
 * Invoked by the controller when the user accepts our warning. Passes
 * the accept through to the message displayer, which knows what to do
 * (it will be displayer-specific).
 *
 * @param browser Reference to the browser for which the user accepted
 *                our warning
 */
PROT_BrowserView.prototype.acceptAction = function(browser) {
  var problem = this.getCurrentProblem_(browser);

  // We run the action only after we're completely through processing
  // this event. We do this because the action could cause state to be
  // cleared (e.g., by navigating the problem document) that we need
  // to finish processing the event.

  new G_Alarm(BindToObject(problem.displayer_.acceptAction, 
                           problem.displayer_), 
              0);
}

/**
 * Invoked by the controller when the user declines our
 * warning. Passes the decline through to the message displayer, which
 * knows what to do (it will be displayer-specific).
 *
 * @param browser Reference to the browser for which the user declined
 *                our warning
 */
PROT_BrowserView.prototype.declineAction = function(browser) {
  var problem = this.getCurrentProblem_(browser);
  G_Assert(this, !!problem, "User declined but no state???");

  // We run the action only after we're completely through processing
  // this event. We do this because the action could cause state to be
  // cleared (e.g., by navigating the problem document) that we need
  // to finish processing the event.

  new G_Alarm(BindToObject(problem.displayer_.declineAction, 
                           problem.displayer_), 
              0);
}

/**
 * The user wants to see the warning message. So let em! At some point when
 * we have multiple types of warnings, we'll have to mediate them here.
 *
 * @param browser Reference to the browser that has the warning the user 
 *                wants to see. 
 */
PROT_BrowserView.prototype.explicitShow = function(browser) {
  var problem = this.getCurrentProblem_(browser);
  G_Assert(this, !!problem, "Explicit show on browser w/o problem state???");
  problem.displayer_.explicitShow();
}
