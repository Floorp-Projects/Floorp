/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = [
  "isBrowsingContextPartOfContext",
  "isWindowGlobalPartOfContext",
  "getAllBrowsingContextsForContext",
];

let Services;
if (typeof module == "object") {
  // Allow this JSM to also be loaded as a CommonJS module
  Services = require("Services");
} else {
  ({ Services } = ChromeUtils.import("resource://gre/modules/Services.jsm"));
}

const isEveryFrameTargetEnabled = Services.prefs.getBoolPref(
  "devtools.every-frame-target.enabled",
  false
);

/**
 * Helper function to know if a given BrowsingContext should be debugged by scope
 * described by the given session context.
 *
 * @param {BrowsingContext} browsingContext
 *        The browsing context we want to check if it is part of debugged context
 * @param {Object} sessionContext
 *        The Session Context to help know what is debugged.
 *        See devtools/server/actors/watcher/session-context.js
 * @param {Object} options
 *        Optional arguments passed via a dictionary.
 * @param {Boolean} options.forceAcceptTopLevelTarget
 *        If true, we will accept top level browsing context even when server target switching
 *        is disabled. In case of client side target switching, the top browsing context
 *        is debugged via a target actor that is being instantiated manually by the frontend.
 *        And this target actor isn't created, nor managed by the watcher actor.
 * @param {Boolean} options.acceptInitialDocument
 *        By default, we ignore initial about:blank documents/WindowGlobals.
 *        But some code cares about all the WindowGlobals, this flag allows to also accept them.
 *        (Used by _validateWindowGlobal)
 * @param {Boolean} options.acceptSameProcessIframes
 *        If true, we will accept WindowGlobal that runs in the same process as their parent document.
 *        That, even when EFT is disabled.
 *        (Used by _validateWindowGlobal)
 * @param {Boolean} options.acceptNoWindowGlobal
 *        By default, we will reject BrowsingContext that don't have any WindowGlobal,
 *        either retrieved via BrowsingContext.currentWindowGlobal in the parent process,
 *        or via the options.windowGlobal argument.
 *        But in some case, we are processing BrowsingContext very early, before any
 *        WindowGlobal has been created for it. But they are still relevant BrowsingContexts
 *        to debug.
 * @param {WindowGlobal} options.windowGlobal
 *        When we are in the content process, we can't easily retrieve the WindowGlobal
 *        for a given BrowsingContext. So allow to pass it via this argument.
 *        Also, there is some race conditions where browsingContext.currentWindowGlobal
 *        is null, while the callsite may have a reference to the WindowGlobal.
 */
function isBrowsingContextPartOfContext(
  browsingContext,
  sessionContext,
  options = {}
) {
  let {
    forceAcceptTopLevelTarget = false,
    acceptNoWindowGlobal = false,
    windowGlobal,
  } = options;

  // For now, reject debugging chrome BrowsingContext.
  // This is for example top level chrome windows like browser.xhtml or webconsole/index.html (only the browser console)
  //
  // Tab and WebExtension debugging shouldn't target any such privileged document.
  // All their document should be of type "content".
  //
  // This may only be an issue for the Browser Toolbox.
  // For now, we expect the ParentProcessTargetActor to debug these.
  // Note that we should probably revisit that, and have each WindowGlobal be debugged
  // by one dedicated WindowGlobalTargetActor (bug 1685500). This requires some tweaks, at least in console-message
  // resource watcher, which makes the ParentProcessTarget's console message resource watcher watch
  // for all documents messages. It should probably only care about window-less messages and have one target per window global,
  // each target fetching one window global messages.
  //
  // Such project would be about applying "EFT" to the browser toolbox and non-content documents
  if (
    browsingContext instanceof CanonicalBrowsingContext &&
    !browsingContext.isContent
  ) {
    return false;
  }

  if (!windowGlobal) {
    // When we are in the parent process, WindowGlobal can be retrieved from the BrowsingContext,
    // while in the content process, the callsites have to pass it manually as an argument
    if (browsingContext instanceof CanonicalBrowsingContext) {
      windowGlobal = browsingContext.currentWindowGlobal;
    } else if (!windowGlobal && !acceptNoWindowGlobal) {
      throw new Error(
        "isBrowsingContextPartOfContext expect a windowGlobal argument when called from the content process"
      );
    }
  }
  // If we have a WindowGlobal, there is some additional checks we can do
  if (
    windowGlobal &&
    !_validateWindowGlobal(windowGlobal, sessionContext, options)
  ) {
    return false;
  }
  // Loading or destroying BrowsingContext won't have any associated WindowGlobal.
  // Ignore them by default. They should be either handled via DOMWindowCreated event or JSWindowActor destroy
  if (!windowGlobal && !acceptNoWindowGlobal) {
    return false;
  }

  // Now do the checks specific to each session context type
  if (sessionContext.type == "all") {
    return true;
  }
  if (sessionContext.type == "browser-element") {
    if (browsingContext.browserId != sessionContext.browserId) {
      return false;
    }
    // For client-side target switching, only mention the "remote frames".
    // i.e. the frames which are in a distinct process compared to their parent document
    // If there is no parent, this is most likely the top level document which we want to ignore.
    //
    // `forceAcceptTopLevelTarget` is set:
    // * when navigating to and from pages in the bfcache, we ignore client side target
    // and start emitting top level target from the server.
    // * when the callsite care about all the debugged browsing contexts,
    // no matter if their related targets are created by client or server.
    const isClientSideTargetSwitching = !sessionContext.isServerTargetSwitchingEnabled;
    const isTopLevelBrowsingContext = !browsingContext.parent;
    if (
      isClientSideTargetSwitching &&
      !forceAcceptTopLevelTarget &&
      isTopLevelBrowsingContext
    ) {
      return false;
    }
    return true;
  }
  if (sessionContext.type == "webextension") {
    // Next and last check expects a WindowGlobal.
    // As we have no way to really know if this BrowsingContext is related to this add-on,
    // ignore it. Even if callsite accepts browsing context without a window global.
    if (!windowGlobal) {
      return false;
    }
    // documentPrincipal is only exposed on WindowGlobalParent,
    // use a fallback for WindowGlobalChild.
    const principal =
      windowGlobal.documentPrincipal ||
      browsingContext.window.document.nodePrincipal;
    return principal.addonId == sessionContext.addonId;
  }
  throw new Error("Unsupported session context type: " + sessionContext.type);
}

/**
 * Helper function of isBrowsingContextPartOfContext to execute all checks
 * against WindowGlobal interface which aren't specific to a given SessionContext type
 *
 * @param {WindowGlobalParent|WindowGlobalChild} windowGlobal
 *        The WindowGlobal we want to check if it is part of debugged context
 * @param {Object} sessionContext
 *        The Session Context to help know what is debugged.
 *        See devtools/server/actors/watcher/session-context.js
 * @param {Object} options
 *        Optional arguments passed via a dictionary.
 *        See `isBrowsingContextPartOfContext` jsdoc.
 */
function _validateWindowGlobal(
  windowGlobal,
  sessionContext,
  { acceptInitialDocument, acceptSameProcessIframes }
) {
  // By default, before loading the actual document (even an about:blank document),
  // we do load immediately "the initial about:blank document".
  // This is expected by the spec. Typically when creating a new BrowsingContext/DocShell/iframe,
  // we would have such transient initial document.
  // `Document.isInitialDocument` helps identify this transient document, which
  // we want to ignore as it would instantiate a very short lived target which
  // confuses many tests and triggers race conditions by spamming many targets.
  //
  // We also ignore some other transient empty documents created while using `window.open()`
  // When using this API with cross process loads, we may create up to three documents/WindowGlobals.
  // We get a first initial about:blank document, and a second document created
  // for moving the document in the right principal.
  // The third document will be the actual document we expect to debug.
  // The second document is an implementation artifact which ideally wouldn't exist
  // and isn't expected by the spec.
  // Note that `window.print` and print preview are using `window.open` and are going through this.
  //
  // WindowGlobalParent will have `isInitialDocument` attribute, while we have to go through the Document for WindowGlobalChild.
  const isInitialDocument =
    windowGlobal.isInitialDocument ||
    windowGlobal.browsingContext.window?.document.isInitialDocument;
  if (isInitialDocument && !acceptInitialDocument) {
    return false;
  }

  // We may process an iframe that runs in the same process as its parent and we don't want
  // to create targets for them if same origin targets (=EFT) are not enabled.
  // Instead the WindowGlobalTargetActor will inspect these children document via docShell tree
  // (typically via `docShells` or `windows` getters).
  // This is quite common when Fission is off as any iframe will run in same process
  // as their parent document. But it can also happen with Fission enabled if iframes have
  // children iframes using the same origin.
  const isSameProcessIframe = !windowGlobal.isProcessRoot;
  if (
    isSameProcessIframe &&
    !acceptSameProcessIframes &&
    !isEveryFrameTargetEnabled
  ) {
    return false;
  }

  return true;
}

/**
 * Helper function to know if a given WindowGlobal should be debugged by scope
 * described by the given session context. This method could be called from any process
 * as so accept either WindowGlobalParent or WindowGlobalChild instances.
 *
 * @param {WindowGlobalParent|WindowGlobalChild} windowGlobal
 *        The WindowGlobal we want to check if it is part of debugged context
 * @param {Object} sessionContext
 *        The Session Context to help know what is debugged.
 *        See devtools/server/actors/watcher/session-context.js
 * @param {Object} options
 *        Optional arguments passed via a dictionary.
 *        See `isBrowsingContextPartOfContext` jsdoc.
 */
function isWindowGlobalPartOfContext(windowGlobal, sessionContext, options) {
  return isBrowsingContextPartOfContext(
    windowGlobal.browsingContext,
    sessionContext,
    {
      ...options,
      windowGlobal,
    }
  );
}

/**
 * Get all the BrowsingContexts that should be debugged by the given session context.
 *
 * Really all of them:
 * - For all the privileged windows (browser.xhtml, browser console, ...)
 * - For all chrome *and* content contexts (privileged windows, as well as <browser> elements and their inner content documents)
 * - For all nested browsing context. We fetch the contexts recursively.
 *
 * @param {Object} sessionContext
 *        The Session Context to help know what is debugged.
 *        See devtools/server/actors/watcher/session-context.js
 * @param {Object} options
 *        Optional arguments passed via a dictionary.
 * @param {Boolean} options.acceptSameProcessIframes
 *        If true, we will accept WindowGlobal that runs in the same process as their parent document.
 *        That, even when EFT is disabled.
 */
function getAllBrowsingContextsForContext(
  sessionContext,
  { acceptSameProcessIframes = false } = {}
) {
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

    if (
      (sessionContext.type == "all" || sessionContext.type == "webextension") &&
      browsingContext.window
    ) {
      // If the document is in the parent process, also iterate over each <browser>'s browsing context.
      // BrowsingContext.children doesn't cross chrome to content boundaries,
      // so we have to cross these boundaries by ourself.
      // (This is also the reason why we aren't using BrowsingContext.getAllBrowsingContextsInSubtree())
      for (const browser of browsingContext.window.document.querySelectorAll(
        `browser[remote="true"]`
      )) {
        walk(browser.browsingContext);
      }
    }
  }

  // If target a single browser element, only walk through its BrowsingContext
  if (sessionContext.type == "browser-element") {
    const topBrowsingContext = BrowsingContext.getCurrentTopByBrowserId(
      sessionContext.browserId
    );
    // Unfortunately, getCurrentTopByBrowserId is subject to race conditions and may refer to a BrowsingContext
    // that already navigated away.
    // Query the current "live" BrowsingContext by going through the embedder element (i.e. the <browser>/<iframe> element)
    // devtools/client/responsive/test/browser/browser_navigation.js covers this with fission enabled.
    const realTopBrowsingContext =
      topBrowsingContext.embedderElement.browsingContext;
    walk(realTopBrowsingContext);
  } else if (
    sessionContext.type == "all" ||
    sessionContext.type == "webextension"
  ) {
    // For the browser toolbox and web extension, retrieve all possible BrowsingContext.
    // For WebExtension, we will then filter out the BrowsingContexts via `isBrowsingContextPartOfContext`.
    //
    // Fetch all top level window's browsing contexts
    for (const window of Services.ww.getWindowEnumerator()) {
      if (window.docShell.browsingContext) {
        walk(window.docShell.browsingContext);
      }
    }
  } else {
    throw new Error("Unsupported session context type: " + sessionContext.type);
  }

  return browsingContexts.filter(bc =>
    // We force accepting the top level browsing context, otherwise
    // it would only be returned if sessionContext.isServerSideTargetSwitching is enabled.
    isBrowsingContextPartOfContext(bc, sessionContext, {
      forceAcceptTopLevelTarget: true,
      acceptSameProcessIframes,
    })
  );
}
if (typeof module == "object") {
  module.exports = {
    isBrowsingContextPartOfContext,
    isWindowGlobalPartOfContext,
    getAllBrowsingContextsForContext,
  };
}
