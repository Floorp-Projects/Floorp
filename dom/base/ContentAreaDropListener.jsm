/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");
const { OS } = ChromeUtils.import("resource://gre/modules/osfile.jsm");

// This component is used for handling dragover and drop of urls.
//
// It checks to see whether a drop of a url is allowed. For instance, a url
// cannot be dropped if it is not a valid uri or the source of the drag cannot
// access the uri. This prevents, for example, a source document from tricking
// the user into dragging a chrome url.

function ContentAreaDropListener() {}

ContentAreaDropListener.prototype = {
  classID: Components.ID("{1f34bc80-1bc7-11d6-a384-d705dd0746fc}"),
  QueryInterface: ChromeUtils.generateQI([Ci.nsIDroppedLinkHandler]),

  _addLink: function(links, url, name, type) {
    links.push({ url, name, type });
  },

  _addLinksFromItem: function(links, dt, i) {
    let types = dt.mozTypesAt(i);
    let type, data;

    type = "text/uri-list";
    if (types.contains(type)) {
      data = dt.mozGetDataAt(type, i);
      if (data) {
        let urls = data.split("\n");
        for (let url of urls) {
          // lines beginning with # are comments
          if (url.startsWith("#")) {
            continue;
          }
          url = url.replace(/^\s+|\s+$/g, "");
          this._addLink(links, url, url, type);
        }
        return;
      }
    }

    type = "text/x-moz-url";
    if (types.contains(type)) {
      data = dt.mozGetDataAt(type, i);
      if (data) {
        let lines = data.split("\n");
        for (let i = 0, length = lines.length; i < length; i += 2) {
          this._addLink(links, lines[i], lines[i + 1], type);
        }
        return;
      }
    }

    for (let type of ["text/plain", "text/x-moz-text-internal"]) {
      if (types.contains(type)) {
        data = dt.mozGetDataAt(type, i);
        if (data) {
          let lines = data.replace(/^\s+|\s+$/gm, "").split("\n");
          if (!lines.length) {
            return;
          }

          // For plain text, there are 2 cases:
          //   * if there is at least one URI:
          //       Add all URIs, ignoring non-URI lines, so that all URIs
          //       are opened in tabs.
          //   * if there's no URI:
          //       Add the entire text as a single entry, so that the entire
          //       text is searched.
          let hasURI = false;
          let flags =
            Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
            Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
          for (let line of lines) {
            let info = Services.uriFixup.getFixupURIInfo(line, flags);
            if (info.fixedURI) {
              // Use the original line here, and let the caller decide
              // whether to perform fixup or not.
              hasURI = true;
              this._addLink(links, line, line, type);
            }
          }

          if (!hasURI) {
            this._addLink(links, data, data, type);
          }
          return;
        }
      }
    }

    // For shortcuts, we want to check for the file type last, so that the
    // url pointed to in one of the url types is found first before the file
    // type, which points to the actual file.
    let files = dt.files;
    if (files && i < files.length) {
      this._addLink(
        links,
        OS.Path.toFileURI(files[i].mozFullPath),
        files[i].name,
        "application/x-moz-file"
      );
    }
  },

  _getDropLinks: function(dt) {
    let links = [];
    for (let i = 0; i < dt.mozItemCount; i++) {
      this._addLinksFromItem(links, dt, i);
    }
    return links;
  },

  _validateURI: function(
    dataTransfer,
    uriString,
    disallowInherit,
    triggeringPrincipal
  ) {
    if (!uriString) {
      return "";
    }

    // Strip leading and trailing whitespace, then try to create a
    // URI from the dropped string. If that succeeds, we're
    // dropping a URI and we need to do a security check to make
    // sure the source document can load the dropped URI.
    uriString = uriString.replace(/^\s*|\s*$/g, "");

    // Apply URI fixup so that this validation prevents bad URIs even if the
    // similar fixup is applied later, especialy fixing typos up will convert
    // non-URI to URI.
    let fixupFlags =
      Ci.nsIURIFixup.FIXUP_FLAG_FIX_SCHEME_TYPOS |
      Ci.nsIURIFixup.FIXUP_FLAG_ALLOW_KEYWORD_LOOKUP;
    let info = Services.uriFixup.getFixupURIInfo(uriString, fixupFlags);
    if (!info.fixedURI || info.keywordProviderName) {
      // Loading a keyword search should always be fine for all cases.
      return uriString;
    }
    let uri = info.fixedURI;

    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
      Ci.nsIScriptSecurityManager
    );
    let flags = secMan.STANDARD;
    if (disallowInherit) {
      flags |= secMan.DISALLOW_INHERIT_PRINCIPAL;
    }

    secMan.checkLoadURIWithPrincipal(triggeringPrincipal, uri, flags);

    // Once we validated, return the URI after fixup, instead of the original
    // uriString.
    return uri.spec;
  },

  _getTriggeringPrincipalFromDataTransfer: function(
    aDataTransfer,
    fallbackToSystemPrincipal
  ) {
    let sourceNode = aDataTransfer.mozSourceNode;
    if (
      sourceNode &&
      (sourceNode.localName !== "browser" ||
        sourceNode.namespaceURI !==
          "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    ) {
      // Use sourceNode's principal only if the sourceNode is not browser.
      //
      // If sourceNode is browser, the actual triggering principal may be
      // differ than sourceNode's principal, since sourceNode's principal is
      // top level document's one and the drag may be triggered from a frame
      // with different principal.
      if (sourceNode.nodePrincipal) {
        return sourceNode.nodePrincipal;
      }
    }

    // First, fallback to mozTriggeringPrincipalURISpec that is set when the
    // drop comes from another content process.
    let principalURISpec = aDataTransfer.mozTriggeringPrincipalURISpec;
    if (!principalURISpec) {
      // Fallback to either system principal or file principal, supposing
      // the drop comes from outside of the browser, so that drops of file
      // URIs are always allowed.
      //
      // TODO: Investigate and describe the difference between them,
      //       or use only one principal. (Bug 1367038)
      if (fallbackToSystemPrincipal) {
        let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
          Ci.nsIScriptSecurityManager
        );
        return secMan.getSystemPrincipal();
      } else {
        principalURISpec = "file:///";
      }
    }
    let ioService = Cc["@mozilla.org/network/io-service;1"].getService(
      Ci.nsIIOService
    );
    let secMan = Cc["@mozilla.org/scriptsecuritymanager;1"].getService(
      Ci.nsIScriptSecurityManager
    );
    return secMan.createContentPrincipal(
      ioService.newURI(principalURISpec),
      {}
    );
  },

  getTriggeringPrincipal: function(aEvent) {
    let dataTransfer = aEvent.dataTransfer;
    return this._getTriggeringPrincipalFromDataTransfer(dataTransfer, true);
  },

  getCSP: function(aEvent) {
    let sourceNode = aEvent.dataTransfer.mozSourceNode;
    if (
      sourceNode &&
      (sourceNode.localName !== "browser" ||
        sourceNode.namespaceURI !==
          "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul")
    ) {
      // Use sourceNode's csp only if the sourceNode is not browser.
      //
      // If sourceNode is browser, the actual triggering csp may be differ than sourceNode's csp,
      // since sourceNode's csp is top level document's one and the drag may be triggered from a
      // frame with different csp.
      return sourceNode.csp;
    }
    return null;
  },

  canDropLink: function(aEvent, aAllowSameDocument) {
    if (this._eventTargetIsDisabled(aEvent)) {
      return false;
    }

    let dataTransfer = aEvent.dataTransfer;
    let types = dataTransfer.types;
    if (
      !types.includes("application/x-moz-file") &&
      !types.includes("text/x-moz-url") &&
      !types.includes("text/uri-list") &&
      !types.includes("text/x-moz-text-internal") &&
      !types.includes("text/plain")
    ) {
      return false;
    }

    if (aAllowSameDocument) {
      return true;
    }

    let sourceNode = dataTransfer.mozSourceNode;
    if (!sourceNode) {
      return true;
    }

    // don't allow a drop of a node from the same document onto this one
    let sourceDocument = sourceNode.ownerDocument;
    let eventDocument = aEvent.originalTarget.ownerDocument;
    if (sourceDocument == eventDocument) {
      return false;
    }

    // also check for nodes in other child or sibling frames by checking
    // if both have the same top window.
    if (sourceDocument && eventDocument) {
      if (sourceDocument.defaultView == null) {
        return true;
      }
      let sourceRoot = sourceDocument.defaultView.top;
      if (sourceRoot && sourceRoot == eventDocument.defaultView.top) {
        return false;
      }
    }

    return true;
  },

  dropLink: function(aEvent, aName, aDisallowInherit) {
    aName.value = "";
    let links = this.dropLinks(aEvent, aDisallowInherit);
    let url = "";
    if (links.length > 0) {
      url = links[0].url;
      let name = links[0].name;
      if (name) {
        aName.value = name;
      }
    }

    return url;
  },

  dropLinks: function(aEvent, aDisallowInherit) {
    if (aEvent && this._eventTargetIsDisabled(aEvent)) {
      return [];
    }

    let dataTransfer = aEvent.dataTransfer;
    let links = this._getDropLinks(dataTransfer);
    let triggeringPrincipal = this._getTriggeringPrincipalFromDataTransfer(
      dataTransfer,
      false
    );

    for (let link of links) {
      try {
        link.url = this._validateURI(
          dataTransfer,
          link.url,
          aDisallowInherit,
          triggeringPrincipal
        );
      } catch (ex) {
        // Prevent the drop entirely if any of the links are invalid even if
        // one of them is valid.
        aEvent.stopPropagation();
        aEvent.preventDefault();
        throw ex;
      }
    }

    return links;
  },

  validateURIsForDrop: function(aEvent, aURIs, aDisallowInherit) {
    let dataTransfer = aEvent.dataTransfer;
    let triggeringPrincipal = this._getTriggeringPrincipalFromDataTransfer(
      dataTransfer,
      false
    );

    for (let uri of aURIs) {
      this._validateURI(
        dataTransfer,
        uri,
        aDisallowInherit,
        triggeringPrincipal
      );
    }
  },

  queryLinks: function(aDataTransfer) {
    return this._getDropLinks(aDataTransfer);
  },

  _eventTargetIsDisabled: function(aEvent) {
    let ownerDoc = aEvent.originalTarget.ownerDocument;
    if (!ownerDoc || !ownerDoc.defaultView) {
      return false;
    }

    return ownerDoc.defaultView.windowUtils.isNodeDisabledForEvents(
      aEvent.originalTarget
    );
  },
};

var EXPORTED_SYMBOLS = ["ContentAreaDropListener"];
