/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { fetch } = require("resource://devtools/shared/DevToolsUtils.js");

// If the user edits a style sheet, we stash a copy of the edited text
// here, keyed by the style sheet.  This way, if the tools are closed
// and then reopened, the edited text will be available.  A weak map
// is used so that navigation by the user will eventually cause the
// edited text to be collected.
const modifiedStyleSheets = new WeakMap();

function getSheetText(sheet) {
  const cssText = modifiedStyleSheets.get(sheet);
  if (cssText !== undefined) {
    return Promise.resolve(cssText);
  }

  if (!sheet.href) {
    // this is an inline <style> sheet
    const content = sheet.ownerNode.textContent;
    return Promise.resolve(content);
  }

  return fetchStylesheet(sheet).then(({ content }) => content);
}

exports.getSheetText = getSheetText;

/**
 * For imported stylesheets, `ownerNode` is null.
 * To resolve the ownerNode for an imported stylesheet, loop on `parentStylesheet`
 * until we reach the topmost stylesheet, which should have a valid ownerNode.
 *
 * @param {StyleSheet}
 *        The stylesheet for which we want to retrieve the ownerNode.
 * @return {DOMNode} The ownerNode
 */
function getSheetOwnerNode(sheet) {
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
exports.getSheetOwnerNode = getSheetOwnerNode;

/**
 * Get the charset of the stylesheet.
 */
function getCSSCharset(sheet) {
  if (sheet) {
    // charset attribute of <link> or <style> element, if it exists
    if (sheet.ownerNode?.getAttribute) {
      const linkCharset = sheet.ownerNode.getAttribute("charset");
      if (linkCharset != null) {
        return linkCharset;
      }
    }

    // charset of referring document.
    if (sheet.ownerNode?.ownerDocument.characterSet) {
      return sheet.ownerNode.ownerDocument.characterSet;
    }
  }

  return "UTF-8";
}

/**
 * Fetch a stylesheet at the provided URL. Returns a promise that will resolve the
 * result of the fetch command.
 *
 * @return {Promise} a promise that resolves with an object with the following members
 *         on success:
 *           - content: the document at that URL, as a string,
 *           - contentType: the content type of the document
 *         If an error occurs, the promise is rejected with that error.
 */
async function fetchStylesheet(sheet) {
  const href = sheet.href;

  const options = {
    loadFromCache: true,
    policy: Ci.nsIContentPolicy.TYPE_INTERNAL_STYLESHEET,
    charset: getCSSCharset(sheet),
  };

  // Bug 1282660 - We use the system principal to load the default internal
  // stylesheets instead of the content principal since such stylesheets
  // require system principal to load. At meanwhile, we strip the loadGroup
  // for preventing the assertion of the userContextId mismatching.

  // chrome|file|resource|moz-extension protocols rely on the system principal.
  const excludedProtocolsRe = /^(chrome|file|resource|moz-extension):\/\//;
  if (!excludedProtocolsRe.test(href)) {
    // Stylesheets using other protocols should use the content principal.
    const ownerNode = getSheetOwnerNode(sheet);
    if (ownerNode) {
      // eslint-disable-next-line mozilla/use-ownerGlobal
      options.window = ownerNode.ownerDocument.defaultView;
      options.principal = ownerNode.ownerDocument.nodePrincipal;
    }
  }

  let result;

  try {
    result = await fetch(href, options);
  } catch (e) {
    // The list of excluded protocols can be missing some protocols, try to use the
    // system principal if the first fetch failed.
    console.error(
      `stylesheets actor: fetch failed for ${href},` +
        ` using system principal instead.`
    );
    options.window = undefined;
    options.principal = undefined;
    result = await fetch(href, options);
  }

  return result;
}
