/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { fetch } = require("resource://devtools/shared/DevToolsUtils.js");

/**
 * For imported stylesheets, `ownerNode` is null.
 *
 * To resolve the ownerNode for an imported stylesheet, loop on `parentStylesheet`
 * until we reach the topmost stylesheet, which should have a valid ownerNode.
 *
 * Constructable stylesheets do not have an owner node and this method will
 * return null.
 *
 * @param {StyleSheet}
 *        The stylesheet for which we want to retrieve the ownerNode.
 * @return {DOMNode|null} The ownerNode or null for constructable stylesheets.
 */
function getStyleSheetOwnerNode(sheet) {
  // If this is not an imported stylesheet and we have an ownerNode available
  // bail out immediately.
  if (sheet.ownerNode) {
    return sheet.ownerNode;
  }

  let parentStyleSheet = sheet;
  while (
    parentStyleSheet.parentStyleSheet &&
    parentStyleSheet !== parentStyleSheet.parentStyleSheet
  ) {
    parentStyleSheet = parentStyleSheet.parentStyleSheet;
  }

  return parentStyleSheet.ownerNode;
}

exports.getStyleSheetOwnerNode = getStyleSheetOwnerNode;

/**
 * Get the text of a stylesheet.
 *
 * TODO: A call site in window-global.js expects this method to return a promise
 * so it is mandatory to keep it as an async function even if we are not using
 * await explicitly. Bug 1810572.
 *
 * @param {StyleSheet}
 *        The stylesheet for which we want to retrieve the text.
 * @returns {Promise}
 */
async function getStyleSheetText(styleSheet) {
  if (!styleSheet.href) {
    if (styleSheet.ownerNode) {
      // this is an inline <style> sheet
      return styleSheet.ownerNode.textContent;
    }
    // Constructed stylesheet.
    // TODO(bug 1769933, bug 1809108): Maybe preserve authored text?
    return "";
  }

  return fetchStyleSheetText(styleSheet);
}

exports.getStyleSheetText = getStyleSheetText;

/**
 * Retrieve the content of a given stylesheet
 *
 * @param {StyleSheet} styleSheet
 * @returns {String}
 */
async function fetchStyleSheetText(styleSheet) {
  const href = styleSheet.href;

  const options = {
    loadFromCache: true,
    policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
    charset: getCSSCharset(styleSheet),
    headers: {
      // https://searchfox.org/mozilla-central/rev/68b1b0041a78abd06f19202558ccc4922e5ba759/netwerk/protocol/http/nsHttpHandler.cpp#124
      accept: "text/css,*/*;q=0.1",
    },
  };

  // Bug 1282660 - We use the system principal to load the default internal
  // stylesheets instead of the content principal since such stylesheets
  // require system principal to load. At meanwhile, we strip the loadGroup
  // for preventing the assertion of the userContextId mismatching.

  // chrome|file|resource|moz-extension protocols rely on the system principal.
  const excludedProtocolsRe = /^(chrome|file|resource|moz-extension):\/\//;
  if (!excludedProtocolsRe.test(href)) {
    // Stylesheets using other protocols should use the content principal.
    const ownerNode = getStyleSheetOwnerNode(styleSheet);
    if (ownerNode) {
      // eslint-disable-next-line mozilla/use-ownerGlobal
      options.window = ownerNode.ownerDocument.defaultView;
      options.principal = ownerNode.ownerDocument.nodePrincipal;
    }
  }

  let result;

  try {
    result = await fetch(href, options);
    if (result.contentType !== "text/css") {
      console.warn(
        `stylesheets: fetch from cache returned non-css content-type ` +
          `${result.contentType} for ${href}, trying without cache.`
      );
      options.loadFromCache = false;
      result = await fetch(href, options);
    }
  } catch (e) {
    // The list of excluded protocols can be missing some protocols, try to use the
    // system principal if the first fetch failed.
    console.error(
      `stylesheets: fetch failed for ${href},` +
        ` using system principal instead.`
    );
    options.window = undefined;
    options.principal = undefined;
    result = await fetch(href, options);
  }

  return result.content;
}

/**
 * Get charset of a given stylesheet
 *
 * @param {StyleSheet} styleSheet
 * @returns {String}
 */
function getCSSCharset(styleSheet) {
  if (styleSheet) {
    // charset attribute of <link> or <style> element, if it exists
    if (styleSheet.ownerNode?.getAttribute) {
      const linkCharset = styleSheet.ownerNode.getAttribute("charset");
      if (linkCharset != null) {
        return linkCharset;
      }
    }

    // charset of referring document.
    if (styleSheet.ownerNode?.ownerDocument.characterSet) {
      return styleSheet.ownerNode.ownerDocument.characterSet;
    }
  }

  return "UTF-8";
}
