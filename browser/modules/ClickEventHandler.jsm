/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ClickEventHandler"];

/* eslint no-unused-vars: ["error", {args: "none"}] */

ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BlockedSiteContent",
                               "resource:///modules/BlockedSiteContent.jsm");
ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "NetErrorContent",
                               "resource:///modules/NetErrorContent.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "WebNavigationFrames",
                               "resource://gre/modules/WebNavigationFrames.jsm");

class ClickEventHandler {
  constructor(mm) {
    this.mm = mm;
  }

  handleEvent(event) {
    if (!event.isTrusted || event.defaultPrevented || event.button == 2) {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (event.button == 0) {
      if (this.mm.AboutNetAndCertErrorListener.isAboutCertError(ownerDoc)) {
        NetErrorContent.onCertError(this.mm, originalTarget, ownerDoc.defaultView);
        return;
      } else if (ownerDoc.documentURI.startsWith("about:blocked")) {
        BlockedSiteContent.onAboutBlocked(this.mm, originalTarget, ownerDoc);
        return;
      } else if (this.mm.AboutNetAndCertErrorListener.isAboutNetError(ownerDoc)) {
        NetErrorContent.onAboutNetError(this.mm, event, ownerDoc.documentURI);
        return;
      }
    }

    let [href, node, principal] = this._hrefAndLinkNodeForClickEvent(event);

    // get referrer attribute from clicked link and parse it
    // if per element referrer is enabled, the element referrer overrules
    // the document wide referrer
    let referrerPolicy = ownerDoc.referrerPolicy;
    if (node) {
      let referrerAttrValue = Services.netUtils.parseAttributePolicyString(node.
                              getAttribute("referrerpolicy"));
      if (referrerAttrValue !== Ci.nsIHttpChannel.REFERRER_POLICY_UNSET) {
        referrerPolicy = referrerAttrValue;
      }
    }

    let frameOuterWindowID = WebNavigationFrames.getFrameId(ownerDoc.defaultView);

    let json = { button: event.button, shiftKey: event.shiftKey,
                 ctrlKey: event.ctrlKey, metaKey: event.metaKey,
                 altKey: event.altKey, href: null, title: null,
                 frameOuterWindowID, referrerPolicy,
                 triggeringPrincipal: principal,
                 originAttributes: principal ? principal.originAttributes : {},
                 isContentWindowPrivate: PrivateBrowsingUtils.isContentWindowPrivate(ownerDoc.defaultView)};

    if (href) {
      try {
        BrowserUtils.urlSecurityCheck(href, principal);
      } catch (e) {
        return;
      }

      json.href = href;
      if (node) {
        json.title = node.getAttribute("title");
      }
      json.noReferrer = BrowserUtils.linkHasNoReferrer(node);

      // Check if the link needs to be opened with mixed content allowed.
      // Only when the owner doc has |mixedContentChannel| and the same origin
      // should we allow mixed content.
      json.allowMixedContent = false;
      let docshell = ownerDoc.docShell;
      if (this.mm.docShell.mixedContentChannel) {
        const sm = Services.scriptSecurityManager;
        try {
          let targetURI = Services.io.newURI(href);
          sm.checkSameOriginURI(docshell.mixedContentChannel.URI, targetURI, false);
          json.allowMixedContent = true;
        } catch (e) {}
      }
      json.originPrincipal = ownerDoc.nodePrincipal;
      json.triggeringPrincipal = ownerDoc.nodePrincipal;

      this.mm.sendAsyncMessage("Content:Click", json);
      return;
    }

    // This might be middle mouse navigation.
    if (event.button == 1) {
      this.mm.sendAsyncMessage("Content:Click", json);
    }
  }

  /**
   * Extracts linkNode and href for the current click target.
   *
   * @param event
   *        The click event.
   * @return [href, linkNode, linkPrincipal].
   *
   * @note linkNode will be null if the click wasn't on an anchor
   *       element. This includes SVG links, because callers expect |node|
   *       to behave like an <a> element, which SVG links (XLink) don't.
   */
  _hrefAndLinkNodeForClickEvent(event) {
    let {content} = this.mm;
    function isHTMLLink(aNode) {
      // Be consistent with what nsContextMenu.js does.
      return ((aNode instanceof content.HTMLAnchorElement && aNode.href) ||
              (aNode instanceof content.HTMLAreaElement && aNode.href) ||
              aNode instanceof content.HTMLLinkElement);
    }

    let node = event.target;
    while (node && !isHTMLLink(node)) {
      node = node.parentNode;
    }

    if (node)
      return [node.href, node, node.ownerDocument.nodePrincipal];

    // If there is no linkNode, try simple XLink.
    let href, baseURI;
    node = event.target;
    while (node && !href) {
      if (node.nodeType == content.Node.ELEMENT_NODE &&
          (node.localName == "a" ||
           node.namespaceURI == "http://www.w3.org/1998/Math/MathML")) {
        href = node.getAttribute("href") ||
               node.getAttributeNS("http://www.w3.org/1999/xlink", "href");
        if (href) {
          baseURI = node.ownerDocument.baseURIObject;
          break;
        }
      }
      node = node.parentNode;
    }

    // In case of XLink, we don't return the node we got href from since
    // callers expect <a>-like elements.
    // Note: makeURI() will throw if aUri is not a valid URI.
    return [href ? Services.io.newURI(href, null, baseURI).spec : null, null,
            node && node.ownerDocument.nodePrincipal];
  }
}
