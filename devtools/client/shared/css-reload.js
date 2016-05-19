/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { Services } = require("resource://gre/modules/Services.jsm");
const { getTheme } = require("devtools/client/shared/theme");

function iterStyleNodes(window, func) {
  for (let node of window.document.childNodes) {
    // Look for ProcessingInstruction nodes.
    if (node.nodeType === 7) {
      func(node);
    }
  }

  const links = window.document.getElementsByTagNameNS(
    "http://www.w3.org/1999/xhtml", "link"
  );
  for (let node of links) {
    func(node);
  }
}

function replaceCSS(window, fileURI) {
  const document = window.document;
  const randomKey = Math.random();
  Services.obs.notifyObservers(null, "startupcache-invalidate", null);

  // Scan every CSS tag and reload ones that match the file we are
  // looking for.
  iterStyleNodes(window, node => {
    if (node.nodeType === 7) {
      // xml-stylesheet declaration
      if (node.data.includes(fileURI)) {
        const newNode = window.document.createProcessingInstruction(
          "xml-stylesheet",
          `href="${fileURI}?s=${randomKey}" type="text/css"`
        );
        document.insertBefore(newNode, node);
        document.removeChild(node);
      }
    } else if (node.href.includes(fileURI)) {
      const parentNode = node.parentNode;
      const newNode = window.document.createElementNS(
        "http://www.w3.org/1999/xhtml",
        "link"
      );
      newNode.rel = "stylesheet";
      newNode.type = "text/css";
      newNode.href = fileURI + "?s=" + randomKey;

      parentNode.insertBefore(newNode, node);
      parentNode.removeChild(node);
    }
  });
}

function _replaceResourceInSheet(sheet, filename, randomKey) {
  for (let i = 0; i < sheet.cssRules.length; i++) {
    const rule = sheet.cssRules[i];
    if (rule.type === rule.IMPORT_RULE) {
      _replaceResourceInSheet(rule.styleSheet, filename);
    } else if (rule.cssText.includes(filename)) {
      // Strip off any existing query strings. This might lose
      // updates for files if there are multiple resources
      // referenced in the same rule, but the chances of someone hot
      // reloading multiple resources in the same rule is very low.
      const text = rule.cssText.replace(/\?s=0.\d+/g, "");
      const newRule = (
        text.replace(filename, filename + "?s=" + randomKey)
      );

      sheet.deleteRule(i);
      sheet.insertRule(newRule, i);
    }
  }
}

function replaceCSSResource(window, fileURI) {
  const document = window.document;
  const randomKey = Math.random();

  // Only match the filename. False positives are much better than
  // missing updates, as all that would happen is we reload more
  // resources than we need. We do this because many resources only
  // use relative paths.
  const parts = fileURI.split("/");
  const file = parts[parts.length - 1];

  // Scan every single rule in the entire page for any reference to
  // this resource, and re-insert the rule to force it to update.
  for (let sheet of document.styleSheets) {
    _replaceResourceInSheet(sheet, file, randomKey);
  }

  for (let node of document.querySelectorAll("img,image")) {
    if (node.src.startsWith(fileURI)) {
      node.src = fileURI + "?s=" + randomKey;
    }
  }
}

function watchCSS(window) {
  if (Services.prefs.getBoolPref("devtools.loader.hotreload")) {
    const watcher = require("devtools/client/shared/devtools-file-watcher");

    function onFileChanged(_, relativePath) {
      if (relativePath.match(/\.css$/)) {
        if (relativePath.startsWith("client/themes")) {
          let path = relativePath.replace(/^client\/themes\//, "");

          // Special-case a few files that get imported from other CSS
          // files. We just manually hot reload the parent CSS file.
          if (path === "variables.css" || path === "toolbars.css" ||
              path === "common.css" || path === "splitters.css") {
            replaceCSS(window, "chrome://devtools/skin/" + getTheme() + "-theme.css");
          } else {
            replaceCSS(window, "chrome://devtools/skin/" + path);
          }
          return;
        }

        replaceCSS(
          window,
          "chrome://devtools/content/" + relativePath.replace(/^client\//, "")
        );
        replaceCSS(window, "resource://devtools/" + relativePath);
      } else if (relativePath.match(/\.(svg|png)$/)) {
        relativePath = relativePath.replace(/^client\/themes\//, "");
        replaceCSSResource(window, "chrome://devtools/skin/" + relativePath);
      }
    }
    watcher.on("file-changed", onFileChanged);

    window.addEventListener("unload", () => {
      watcher.off("file-changed", onFileChanged);
    });
  }
}

module.exports = { watchCSS };
