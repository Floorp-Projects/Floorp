/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
/* globals document, window */
/* import-globals-from ./debugger-controller.js */
"use strict";

// Maps known URLs to friendly source group names and put them at the
// bottom of source list.
var KNOWN_SOURCE_GROUPS = {
  "Add-on SDK": "resource://gre/modules/commonjs/",
};

KNOWN_SOURCE_GROUPS[L10N.getStr("anonymousSourcesLabel")] = "anonymous";

var XULUtils = {
  /**
   * Create <command> elements within `commandset` with event handlers
   * bound to the `command` event
   *
   * @param commandset HTML Element
   *        A <commandset> element
   * @param commands Object
   *        An object where keys specify <command> ids and values
   *        specify event handlers to be bound on the `command` event
   */
  addCommands: function (commandset, commands) {
    Object.keys(commands).forEach(name => {
      let node = document.createElement("command");
      node.id = name;
      // XXX bug 371900: the command element must have an oncommand
      // attribute as a string set by `setAttribute` for keys to use it
      node.setAttribute("oncommand", " ");
      node.addEventListener("command", commands[name]);
      commandset.appendChild(node);
    });
  }
};

// Used to detect minification for automatic pretty printing
const SAMPLE_SIZE = 50; // no of lines
const INDENT_COUNT_THRESHOLD = 5; // percentage
const CHARACTER_LIMIT = 250; // line character limit

/**
 * Utility functions for handling sources.
 */
var SourceUtils = {
  _labelsCache: new Map(), // Can't use WeakMaps because keys are strings.
  _groupsCache: new Map(),
  _minifiedCache: new Map(),

  /**
   * Returns true if the specified url and/or content type are specific to
   * javascript files.
   *
   * @return boolean
   *         True if the source is likely javascript.
   */
  isJavaScript: function (aUrl, aContentType = "") {
    return (aUrl && /\.jsm?$/.test(this.trimUrlQuery(aUrl))) ||
           aContentType.includes("javascript");
  },

  /**
   * Determines if the source text is minified by using
   * the percentage indented of a subset of lines
   *
   * @return object
   *         A promise that resolves to true if source text is minified.
   */
  isMinified: function (key, text) {
    if (this._minifiedCache.has(key)) {
      return this._minifiedCache.get(key);
    }

    let isMinified;
    let lineEndIndex = 0;
    let lineStartIndex = 0;
    let lines = 0;
    let indentCount = 0;
    let overCharLimit = false;

    // Strip comments.
    text = text.replace(/\/\*[\S\s]*?\*\/|\/\/(.+|\n)/g, "");

    while (lines++ < SAMPLE_SIZE) {
      lineEndIndex = text.indexOf("\n", lineStartIndex);
      if (lineEndIndex == -1) {
        break;
      }
      if (/^\s+/.test(text.slice(lineStartIndex, lineEndIndex))) {
        indentCount++;
      }
      // For files with no indents but are not minified.
      if ((lineEndIndex - lineStartIndex) > CHARACTER_LIMIT) {
        overCharLimit = true;
        break;
      }
      lineStartIndex = lineEndIndex + 1;
    }

    isMinified =
      ((indentCount / lines) * 100) < INDENT_COUNT_THRESHOLD || overCharLimit;

    this._minifiedCache.set(key, isMinified);
    return isMinified;
  },

  /**
   * Clears the labels, groups and minify cache, populated by methods like
   * SourceUtils.getSourceLabel or Source Utils.getSourceGroup.
   * This should be done every time the content location changes.
   */
  clearCache: function () {
    this._labelsCache.clear();
    this._groupsCache.clear();
    this._minifiedCache.clear();
  },

  /**
   * Gets a unique, simplified label from a source url.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The simplified label.
   */
  getSourceLabel: function (aUrl) {
    let cachedLabel = this._labelsCache.get(aUrl);
    if (cachedLabel) {
      return cachedLabel;
    }

    let sourceLabel = null;

    for (let name of Object.keys(KNOWN_SOURCE_GROUPS)) {
      if (aUrl.startsWith(KNOWN_SOURCE_GROUPS[name])) {
        sourceLabel = aUrl.substring(KNOWN_SOURCE_GROUPS[name].length);
      }
    }

    if (!sourceLabel) {
      sourceLabel = this.trimUrl(aUrl);
    }

    let unicodeLabel = NetworkHelper.convertToUnicode(unescape(sourceLabel));
    this._labelsCache.set(aUrl, unicodeLabel);
    return unicodeLabel;
  },

  /**
   * Gets as much information as possible about the hostname and directory paths
   * of an url to create a short url group identifier.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The simplified group.
   */
  getSourceGroup: function (aUrl) {
    let cachedGroup = this._groupsCache.get(aUrl);
    if (cachedGroup) {
      return cachedGroup;
    }

    try {
      // Use an nsIURL to parse all the url path parts.
      var uri = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
    } catch (e) {
      // This doesn't look like a url, or nsIURL can't handle it.
      return "";
    }

    let groupLabel = uri.prePath;

    for (let name of Object.keys(KNOWN_SOURCE_GROUPS)) {
      if (aUrl.startsWith(KNOWN_SOURCE_GROUPS[name])) {
        groupLabel = name;
      }
    }

    let unicodeLabel = NetworkHelper.convertToUnicode(unescape(groupLabel));
    this._groupsCache.set(aUrl, unicodeLabel);
    return unicodeLabel;
  },

  /**
   * Trims the url by shortening it if it exceeds a certain length, adding an
   * ellipsis at the end.
   *
   * @param string aUrl
   *        The source url.
   * @param number aLength [optional]
   *        The expected source url length.
   * @param number aSection [optional]
   *        The section to trim. Supported values: "start", "center", "end"
   * @return string
   *         The shortened url.
   */
  trimUrlLength: function (aUrl, aLength, aSection) {
    aLength = aLength || SOURCE_URL_DEFAULT_MAX_LENGTH;
    aSection = aSection || "end";

    if (aUrl.length > aLength) {
      switch (aSection) {
        case "start":
          return ELLIPSIS + aUrl.slice(-aLength);
          break;
        case "center":
          return aUrl.substr(0, aLength / 2 - 1) + ELLIPSIS + aUrl.slice(-aLength / 2 + 1);
          break;
        case "end":
          return aUrl.substr(0, aLength) + ELLIPSIS;
          break;
      }
    }
    return aUrl;
  },

  /**
   * Trims the query part or reference identifier of a url string, if necessary.
   *
   * @param string aUrl
   *        The source url.
   * @return string
   *         The shortened url.
   */
  trimUrlQuery: function (aUrl) {
    let length = aUrl.length;
    let q1 = aUrl.indexOf("?");
    let q2 = aUrl.indexOf("&");
    let q3 = aUrl.indexOf("#");
    let q = Math.min(q1 != -1 ? q1 : length,
                     q2 != -1 ? q2 : length,
                     q3 != -1 ? q3 : length);

    return aUrl.slice(0, q);
  },

  /**
   * Trims as much as possible from a url, while keeping the label unique
   * in the sources container.
   *
   * @param string | nsIURL aUrl
   *        The source url.
   * @param string aLabel [optional]
   *        The resulting label at each step.
   * @param number aSeq [optional]
   *        The current iteration step.
   * @return string
   *         The resulting label at the final step.
   */
  trimUrl: function (aUrl, aLabel, aSeq) {
    if (!(aUrl instanceof Ci.nsIURL)) {
      try {
        // Use an nsIURL to parse all the url path parts.
        aUrl = Services.io.newURI(aUrl, null, null).QueryInterface(Ci.nsIURL);
      } catch (e) {
        // This doesn't look like a url, or nsIURL can't handle it.
        return aUrl;
      }
    }
    if (!aSeq) {
      let name = aUrl.fileName;
      if (name) {
        // This is a regular file url, get only the file name (contains the
        // base name and extension if available).

        // If this url contains an invalid query, unfortunately nsIURL thinks
        // it's part of the file extension. It must be removed.
        aLabel = aUrl.fileName.replace(/\&.*/, "");
      } else {
        // This is not a file url, hence there is no base name, nor extension.
        // Proceed using other available information.
        aLabel = "";
      }
      aSeq = 1;
    }

    // If we have a label and it doesn't only contain a query...
    if (aLabel && aLabel.indexOf("?") != 0) {
      // A page may contain multiple requests to the same url but with different
      // queries. It is *not* redundant to show each one.
      if (!DebuggerView.Sources.getItemForAttachment(e => e.label == aLabel)) {
        return aLabel;
      }
    }

    // Append the url query.
    if (aSeq == 1) {
      let query = aUrl.query;
      if (query) {
        return this.trimUrl(aUrl, aLabel + "?" + query, aSeq + 1);
      }
      aSeq++;
    }
    // Append the url reference.
    if (aSeq == 2) {
      let ref = aUrl.ref;
      if (ref) {
        return this.trimUrl(aUrl, aLabel + "#" + aUrl.ref, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the url directory.
    if (aSeq == 3) {
      let dir = aUrl.directory;
      if (dir) {
        return this.trimUrl(aUrl, dir.replace(/^\//, "") + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Prepend the hostname and port number.
    if (aSeq == 4) {
      let host;
      try {
        // Bug 1261860: jar: URLs throw when accessing `hostPost`
        host = aUrl.hostPort;
      } catch (e) {}
      if (host) {
        return this.trimUrl(aUrl, host + "/" + aLabel, aSeq + 1);
      }
      aSeq++;
    }
    // Use the whole url spec but ignoring the reference.
    if (aSeq == 5) {
      return this.trimUrl(aUrl, aUrl.specIgnoringRef, aSeq + 1);
    }
    // Give up.
    return aUrl.spec;
  },

  parseSource: function (aDebuggerView, aParser) {
    let editor = aDebuggerView.editor;

    let contents = editor.getText();
    let location = aDebuggerView.Sources.selectedValue;
    let parsedSource = aParser.get(contents, location);

    return parsedSource;
  },

  findIdentifier: function (aEditor, parsedSource, x, y) {
    let editor = aEditor;

    // Calculate the editor's line and column at the current x and y coords.
    let hoveredPos = editor.getPositionFromCoords({ left: x, top: y });
    let hoveredOffset = editor.getOffset(hoveredPos);
    let hoveredLine = hoveredPos.line;
    let hoveredColumn = hoveredPos.ch;

    let scriptInfo = parsedSource.getScriptInfo(hoveredOffset);

    // If the script length is negative, we're not hovering JS source code.
    if (scriptInfo.length == -1) {
      return;
    }

    // Using the script offset, determine the actual line and column inside the
    // script, to use when finding identifiers.
    let scriptStart = editor.getPosition(scriptInfo.start);
    let scriptLineOffset = scriptStart.line;
    let scriptColumnOffset = (hoveredLine == scriptStart.line ? scriptStart.ch : 0);

    let scriptLine = hoveredLine - scriptLineOffset;
    let scriptColumn = hoveredColumn - scriptColumnOffset;
    let identifierInfo = parsedSource.getIdentifierAt({
      line: scriptLine + 1,
      column: scriptColumn,
      scriptIndex: scriptInfo.index
    });

    return identifierInfo;
  }
};
