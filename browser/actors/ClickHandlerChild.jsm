/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["ClickHandlerChild"];

const {ActorChild} = ChromeUtils.import("resource://gre/modules/ActorChild.jsm");
const {Services} = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(this, "BrowserUtils",
                               "resource://gre/modules/BrowserUtils.jsm");
ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
                               "resource://gre/modules/PrivateBrowsingUtils.jsm");
ChromeUtils.defineModuleGetter(this, "WebNavigationFrames",
                               "resource://gre/modules/WebNavigationFrames.jsm");
ChromeUtils.defineModuleGetter(this, "E10SUtils",
                               "resource://gre/modules/E10SUtils.jsm");

class ClickHandlerChild extends ActorChild {
  handleEvent(event) {
    if (!event.isTrusted || event.defaultPrevented || event.button == 2 ||
        (event.type == "click" && event.button == 1)) {
      return;
    }
    // Don't do anything on editable things, we shouldn't open links in
    // contenteditables, and editor needs to possibly handle middlemouse paste
    let composedTarget = event.composedTarget;
    if (composedTarget.isContentEditable ||
        (composedTarget.ownerDocument &&
         composedTarget.ownerDocument.designMode == "on") ||
        ChromeUtils.getClassName(composedTarget) == "HTMLInputElement" ||
        ChromeUtils.getClassName(composedTarget) == "HTMLTextAreaElement") {
      return;
    }

    let originalTarget = event.originalTarget;
    let ownerDoc = originalTarget.ownerDocument;
    if (!ownerDoc) {
      return;
    }

    // Handle click events from about pages
    if (event.button == 0) {
      if (ownerDoc.documentURI.startsWith("about:blocked")) {
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

    // Bug 965637, query the CSP from the doc instead of the Principal
    let csp = ownerDoc.nodePrincipal.csp;
    if (csp) {
      csp = E10SUtils.serializeCSP(csp);
    }

    let ReferrerInfo = Components.Constructor("@mozilla.org/referrer-info;1",
                                              "nsIReferrerInfo",
                                              "init");
    let referrerInfo = new ReferrerInfo(
      referrerPolicy,
      !BrowserUtils.linkHasNoReferrer(node),
      ownerDoc.documentURIObject);
    referrerInfo = E10SUtils.serializeReferrerInfo(referrerInfo);
    let frameOuterWindowID = WebNavigationFrames.getFrameId(ownerDoc.defaultView);

    let json = { button: event.button, shiftKey: event.shiftKey,
                 ctrlKey: event.ctrlKey, metaKey: event.metaKey,
                 altKey: event.altKey, href: null, title: null,
                 frameOuterWindowID,
                 triggeringPrincipal: principal,
                 csp,
                 referrerInfo,
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


      // Check if the link needs to be opened with mixed content allowed.
      // Only when the owner doc has |mixedContentChannel| and the same origin
      // should we allow mixed content.
      json.allowMixedContent = false;
      let docshell = ownerDoc.defaultView.docShell;
      if (this.mm.docShell.mixedContentChannel) {
        const sm = Services.scriptSecurityManager;
        try {
          let targetURI = Services.io.newURI(href);
          let isPrivateWin = ownerDoc.nodePrincipal.originAttributes.privateBrowsingId > 0;
          sm.checkSameOriginURI(docshell.mixedContentChannel.URI, targetURI, false, isPrivateWin);
          json.allowMixedContent = true;
        } catch (e) {}
      }
      json.originPrincipal = ownerDoc.nodePrincipal;
      json.triggeringPrincipal = ownerDoc.nodePrincipal;

      // If a link element is clicked with middle button, user wants to open
      // the link somewhere rather than pasting clipboard content.  Therefore,
      // when it's clicked with middle button, we should prevent multiple
      // actions here to avoid leaking clipboard content unexpectedly.
      // Note that whether the link will work actually or not does not matter
      // because in this case, user does not intent to paste clipboard content.
      if (event.button === 1) {
        event.preventMultipleActions();
      }

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

    let node = event.composedTarget;
    while (node && !isHTMLLink(node)) {
      node = node.flattenedTreeParentNode;
    }

    if (node)
      return [node.href, node, node.ownerDocument.nodePrincipal];

    // If there is no linkNode, try simple XLink.
    let href, baseURI;
    node = event.composedTarget;
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
      node = node.flattenedTreeParentNode;
    }

    // In case of XLink, we don't return the node we got href from since
    // callers expect <a>-like elements.
    // Note: makeURI() will throw if aUri is not a valid URI.
    return [href ? Services.io.newURI(href, null, baseURI).spec : null, null,
            node && node.ownerDocument.nodePrincipal];
  }
}
