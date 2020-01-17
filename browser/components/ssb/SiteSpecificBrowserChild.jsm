/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const EXPORTED_SYMBOLS = ["SiteSpecificBrowserChild"];

const { SiteSpecificBrowserBase } = ChromeUtils.import(
  "resource:///modules/SiteSpecificBrowserService.jsm"
);
const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { E10SUtils } = ChromeUtils.import(
  "resource://gre/modules/E10SUtils.jsm"
);

/**
 * Loads an icon URL into a data URI.
 *
 * @param {Window} window the DOM window providing the icon.
 * @param {string} uri the href for the icon, may be relative to the source page.
 * @return {Promise<string>} the data URI.
 */
async function loadIcon(window, uri) {
  let iconURL = new window.URL(uri, window.location);

  let request = new window.Request(iconURL, { mode: "cors" });
  request.overrideContentPolicyType(Ci.nsIContentPolicy.TYPE_IMAGE);

  let response = await window.fetch(request);
  let blob = await response.blob();

  return new Promise((resolve, reject) => {
    const reader = new FileReader();
    reader.onloadend = () => resolve(reader.result);
    reader.onerror = reject;
    reader.readAsDataURL(blob);
  });
}

class SiteSpecificBrowserChild extends JSWindowActorChild {
  receiveMessage(message) {
    switch (message.name) {
      case "SetSSB":
        // Note that this sets the webbrowserchrome for the top-level browser
        // child in this page. This means that any inner-frames loading in
        // different processes will not be handled correctly. Fixing this will
        // happen in bug 1602849.
        this.docShell.browserChild.webBrowserChrome = new WebBrowserChrome(
          message.data
        );
        break;
      case "LoadIcon":
        return loadIcon(this.contentWindow, message.data);
    }

    return null;
  }
}

function getActor(docShell) {
  return docShell.domWindow.gwindowGlobalChild.getActor("SiteSpecificBrowser");
}

// JS actors can't generally be XPCOM objects so we must use a separate class.
class WebBrowserChrome {
  constructor(id) {
    this.id = id;
  }

  get ssb() {
    return SiteSpecificBrowserBase.get(this.id);
  }

  // nsIWebBrowserChrome3

  /**
   * This gets called when a user clicks on a link or submits a form. We can use
   * it to see where the resulting page load will occur and if needed redirect
   * it to a different target.
   *
   * @param {string}  originalTarget the target intended for the load.
   * @param {nsIURI}  linkURI        the URI that will be loaded.
   * @param {Node}    linkNode       the element causing the load.
   * @param {boolean} isAppTab       whether the source docshell is marked as an
   *                                 app tab.
   * @return {string} the target to use for the load.
   */
  onBeforeLinkTraversal(originalTarget, linkURI, linkNode, isAppTab) {
    // Our actor is for the top-level frame in the page while this may be being
    // called for a navigation in an inner frame. First we have to find the
    // browsing context for the frame doing the load.

    let docShell = linkNode.ownerGlobal.docShell;
    let bc = docShell.browsingContext;

    // Which browsing context is this link targetting?
    let target = originalTarget ? bc.findWithName(originalTarget) : bc;

    if (target) {
      // If we found a target then it must be one of the frames within this
      // frame tree since we don't support popup windows.
      if (target.parent) {
        // An inner frame, continue.
        return originalTarget;
      }

      // A top-level load. If our SSB cannot load this URI then start the
      // process of opening it into a new tab somewhere.
      return this.ssb.canLoad(linkURI) ? originalTarget : "_blank";
    }

    // An attempt to open a new window/tab. If the new URI can be loaded by our
    // SSB then load it at the top-level. Note that we override the requested
    // target so that this page can't reach the new context.
    return this.ssb.canLoad(linkURI) ? "_top" : "_blank";
  }

  /**
   * A load is about to occur in a frame. This is an opportunity to stop it
   * and redirect it somewhere.
   *
   * @param {nsIDocShell}     docShell            the current docshell.
   * @param {nsIURI}          uri                 the URI that will be loaded.
   * @param {nsIReferrerInfo} referrerInfo        the referrer info.
   * @param {boolean}         hasPostData         whether there is POST data
   *                                              for the load.
   * @param {nsIPrincipal}    triggeringPrincipal the triggering principal.
   * @param {nsIContentSecurityPolicy} csp the content security policy.
   * @return {boolean} whether the load should proceed or not.
   */
  shouldLoadURI(
    docShell,
    uri,
    referrerInfo,
    hasPostData,
    triggeringPrincipal,
    csp
  ) {
    // As above, our actor is for the top-level frame in the page however we
    // are passed the docshell potentially handling the load here so we can
    // do the right thing.

    // We only police loads at the top level.
    if (docShell.browsingContext.parent) {
      return true;
    }

    if (!this.ssb.canLoad(uri)) {
      // Should only have got this far for a window.location manipulation.

      getActor(docShell).sendAsyncMessage("RetargetOutOfScopeURIToBrowser", {
        uri: uri.spec,
        referrerInfo: E10SUtils.serializeReferrerInfo(referrerInfo),
        triggeringPrincipal: E10SUtils.serializePrincipal(
          triggeringPrincipal ||
            Services.scriptSecurityManager.createNullPrincipal({})
        ),
        csp: csp ? E10SUtils.serializeCSP(csp) : null,
      });

      return false;
    }

    return true;
  }

  /**
   * A simple check for whether this is the correct process to load this URI.
   *
   * @param {nsIURI}   uri  the URI that will be loaded.
   * @return {boolean} whether the load should proceed or not.
   */
  shouldLoadURIInThisProcess(uri) {
    return this.ssb.canLoad(uri);
  }

  /**
   * Instructs us to start a fresh process to load this URI. Usually used for
   * large allocation sites. SSB does not support this.
   *
   * @param {nsIDocShell}     docShell            the current docshell.
   * @param {nsIURI}          uri                 the URI that will be loaded.
   * @param {nsIReferrerInfo} referrerInfo        the referrer info.
   * @param {nsIPrincipal}    triggeringPrincipal the triggering principal.
   * @param {Number}          loadFlags           the load flags.
   * @param {nsIContentSecurityPolicy} csp the content security policy.
   * @return {boolean} whether the load should proceed or not.
   */
  reloadInFreshProcess(
    docShell,
    uri,
    referrerInfo,
    triggeringPrincipal,
    loadFlags,
    csp
  ) {
    return false;
  }
}
