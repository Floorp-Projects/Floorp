/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");

/**
 * Helper function to know if a given WindowGlobal should be exposed via watchTargets API
 * XXX: We probably want to share this function with DevToolsFrameChild,
 * but may be not, it looks like the checks are really differents because WindowGlobalParent and WindowGlobalChild
 * expose very different attributes. (WindowGlobalChild exposes much less!)
 *
 * @param {BrowsingContext} browsingContext: The browsing context we want to check the window global for
 * @param {String} watchedBrowserId
 * @param {Object} options
 * @param {Boolean} options.acceptNonRemoteFrame: Set to true to not restrict to remote frame only
 */
function shouldNotifyWindowGlobal(
  browsingContext,
  watchedBrowserId,
  options = {}
) {
  const windowGlobal = browsingContext.currentWindowGlobal;
  // Loading or destroying BrowsingContext won't have any associated WindowGlobal.
  // Ignore them. They should be either handled via DOMWindowCreated event or JSWindowActor destroy
  if (!windowGlobal) {
    return false;
  }

  // Ignore globals running in the parent process for now as they won't be in a distinct process anyway.
  // And JSWindowActor will most likely only be created if we toggle includeChrome
  // on the JSWindowActor registration.
  if (windowGlobal.osPid == -1 && windowGlobal.isInProcess) {
    return false;
  }
  // Ignore about:blank which are quickly replaced and destroyed by the final URI
  // bug 1625026 aims at removing this workaround and allow debugging any about:blank load
  if (
    windowGlobal.documentURI &&
    windowGlobal.documentURI.spec == "about:blank"
  ) {
    return false;
  }

  if (watchedBrowserId && browsingContext.browserId != watchedBrowserId) {
    return false;
  }

  if (options.acceptNonRemoteFrame) {
    return true;
  }

  // If `acceptNonRemoteFrame` options isn't true, only mention the "remote frames".
  // i.e. the frames which are in a distinct process compared to their parent document
  return (
    !browsingContext.parent ||
    windowGlobal.osPid != browsingContext.parent.currentWindowGlobal.osPid
  );
}

/**
 * Get all the BrowsingContexts.
 *
 * Really all of them:
 * - For all the privileged windows (browser.xhtml, browser console, ...)
 * - For all chrome *and* content contexts (privileged windows, as well as <browser> elements and their inner content documents)
 * - For all nested browsing context. We fetch the contexts recursively.
 *
 * @param BrowsingContext topBrowsingContext (optional)
 *        If defined, this will restrict to this Browsing Context only
 *        and any of its (nested) children.
 */
function getAllRemoteBrowsingContexts(topBrowsingContext) {
  const browsingContexts = [];

  // For a given BrowsingContext, add the `browsingContext`
  // all of its children, that, recursively.
  function walk(browsingContext) {
    if (browsingContexts.includes(browsingContext)) {
      return;
    }
    browsingContexts.push(browsingContext);

    for (const child of browsingContext.children) {
      walk(child);
    }

    if (browsingContext.window) {
      // If the document is in the parent process, also iterate over each <browser>'s browsing context.
      // BrowsingContext.children doesn't cross chrome to content boundaries,
      // so we have to cross these boundaries by ourself.
      for (const browser of browsingContext.window.document.querySelectorAll(
        `browser[remote="true"]`
      )) {
        walk(browser.browsingContext);
      }
    }
  }

  // If a Browsing Context is passed, only walk through the given BrowsingContext
  if (topBrowsingContext) {
    walk(topBrowsingContext);
    // Remove the top level browsing context we just added by calling walk.
    browsingContexts.shift();
  } else {
    // Fetch all top level window's browsing contexts
    // Note that getWindowEnumerator works from all processes, including the content process.
    for (const window of Services.ww.getWindowEnumerator()) {
      if (window.docShell.browsingContext) {
        walk(window.docShell.browsingContext);
      }
    }
  }

  return browsingContexts;
}

module.exports = {
  getAllRemoteBrowsingContexts,
  shouldNotifyWindowGlobal,
};
