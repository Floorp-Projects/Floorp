/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Curl } = require("devtools/client/shared/curl");
const { gDevTools } = require("devtools/client/framework/devtools");
const { saveAs } = require("devtools/client/shared/file-saver");
const { copyString } = require("devtools/shared/platform/clipboard");
const { HarExporter } = require("./har/har-exporter");
const {
  getSelectedRequest,
  getSortedRequests,
} = require("./selectors/index");
const { L10N } = require("./utils/l10n");
const { showMenu } = require("devtools/client/netmonitor/src/utils/menu");
const {
  getUrlQuery,
  parseQueryString,
  getUrlBaseName,
} = require("./utils/request-utils");

function RequestListContextMenu({
  cloneSelectedRequest,
  getLongString,
  getTabTarget,
  openStatistics,
}) {
  this.cloneSelectedRequest = cloneSelectedRequest;
  this.getLongString = getLongString;
  this.getTabTarget = getTabTarget;
  this.openStatistics = openStatistics;
}

RequestListContextMenu.prototype = {
  get selectedRequest() {
    // FIXME: Bug 1336382 - Implement RequestListContextMenu React component
    // Remove window.store
    return getSelectedRequest(window.store.getState());
  },

  get sortedRequests() {
    // FIXME: Bug 1336382 - Implement RequestListContextMenu React component
    // Remove window.store
    return getSortedRequests(window.store.getState());
  },

  /**
   * Handle the context menu opening. Hide items if no request is selected.
   * Since visible attribute only accept boolean value but the method call may
   * return undefined, we use !! to force convert any object to boolean
   */
  open(event = {}) {
    let selectedRequest = this.selectedRequest;
    let menu = [];
    let copySubmenu = [];

    copySubmenu.push({
      id: "request-list-context-copy-url",
      label: L10N.getStr("netmonitor.context.copyUrl"),
      accesskey: L10N.getStr("netmonitor.context.copyUrl.accesskey"),
      visible: !!selectedRequest,
      click: () => this.copyUrl(),
    });

    copySubmenu.push({
      id: "request-list-context-copy-url-params",
      label: L10N.getStr("netmonitor.context.copyUrlParams"),
      accesskey: L10N.getStr("netmonitor.context.copyUrlParams.accesskey"),
      visible: !!(selectedRequest && getUrlQuery(selectedRequest.url)),
      click: () => this.copyUrlParams(),
    });

    copySubmenu.push({
      id: "request-list-context-copy-post-data",
      label: L10N.getStr("netmonitor.context.copyPostData"),
      accesskey: L10N.getStr("netmonitor.context.copyPostData.accesskey"),
      visible: !!(selectedRequest && selectedRequest.requestPostData),
      click: () => this.copyPostData(),
    });

    copySubmenu.push({
      id: "request-list-context-copy-as-curl",
      label: L10N.getStr("netmonitor.context.copyAsCurl"),
      accesskey: L10N.getStr("netmonitor.context.copyAsCurl.accesskey"),
      visible: !!selectedRequest,
      click: () => this.copyAsCurl(),
    });

    copySubmenu.push({
      type: "separator",
      visible: !!selectedRequest,
    });

    copySubmenu.push({
      id: "request-list-context-copy-request-headers",
      label: L10N.getStr("netmonitor.context.copyRequestHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyRequestHeaders.accesskey"),
      visible: !!(selectedRequest && selectedRequest.requestHeaders),
      click: () => this.copyRequestHeaders(),
    });

    copySubmenu.push({
      id: "response-list-context-copy-response-headers",
      label: L10N.getStr("netmonitor.context.copyResponseHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyResponseHeaders.accesskey"),
      visible: !!(selectedRequest && selectedRequest.responseHeaders),
      click: () => this.copyResponseHeaders(),
    });

    copySubmenu.push({
      id: "request-list-context-copy-response",
      label: L10N.getStr("netmonitor.context.copyResponse"),
      accesskey: L10N.getStr("netmonitor.context.copyResponse.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.text &&
               selectedRequest.responseContent.content.text.length !== 0),
      click: () => this.copyResponse(),
    });

    copySubmenu.push({
      id: "request-list-context-copy-image-as-data-uri",
      label: L10N.getStr("netmonitor.context.copyImageAsDataUri"),
      accesskey: L10N.getStr("netmonitor.context.copyImageAsDataUri.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.mimeType.includes("image/")),
      click: () => this.copyImageAsDataUri(),
    });

    copySubmenu.push({
      type: "separator",
      visible: !!selectedRequest,
    });

    copySubmenu.push({
      id: "request-list-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      visible: this.sortedRequests.size > 0,
      click: () => this.copyAllAsHar(),
    });

    menu.push({
      label: L10N.getStr("netmonitor.context.copy"),
      accesskey: L10N.getStr("netmonitor.context.copy.accesskey"),
      visible: !!selectedRequest,
      submenu: copySubmenu,
    });

    menu.push({
      id: "request-list-context-save-all-as-har",
      label: L10N.getStr("netmonitor.context.saveAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
      visible: this.sortedRequests.size > 0,
      click: () => this.saveAllAsHar(),
    });

    menu.push({
      id: "request-list-context-save-image-as",
      label: L10N.getStr("netmonitor.context.saveImageAs"),
      accesskey: L10N.getStr("netmonitor.context.saveImageAs.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.mimeType.includes("image/")),
      click: () => this.saveImageAs(),
    });

    menu.push({
      type: "separator",
      visible: !!(selectedRequest && !selectedRequest.isCustom),
    });

    menu.push({
      id: "request-list-context-resend",
      label: L10N.getStr("netmonitor.context.editAndResend"),
      accesskey: L10N.getStr("netmonitor.context.editAndResend.accesskey"),
      visible: !!(selectedRequest && !selectedRequest.isCustom),
      click: this.cloneSelectedRequest,
    });

    menu.push({
      type: "separator",
      visible: !!selectedRequest,
    });

    menu.push({
      id: "request-list-context-newtab",
      label: L10N.getStr("netmonitor.context.newTab"),
      accesskey: L10N.getStr("netmonitor.context.newTab.accesskey"),
      visible: !!selectedRequest,
      click: () => this.openRequestInTab()
    });

    menu.push({
      id: "request-list-context-open-in-debugger",
      label: L10N.getStr("netmonitor.context.openInDebugger"),
      accesskey: L10N.getStr("netmonitor.context.openInDebugger.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.mimeType.includes("javascript")),
      click: () => this.openInDebugger()
    });

    menu.push({
      id: "request-list-context-open-in-style-editor",
      label: L10N.getStr("netmonitor.context.openInStyleEditor"),
      accesskey: L10N.getStr("netmonitor.context.openInStyleEditor.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               Services.prefs.getBoolPref("devtools.styleeditor.enabled") &&
               selectedRequest.responseContent.content.mimeType.includes("css")),
      click: () => this.openInStyleEditor()
    });

    menu.push({
      id: "request-list-context-perf",
      label: L10N.getStr("netmonitor.context.perfTools"),
      accesskey: L10N.getStr("netmonitor.context.perfTools.accesskey"),
      visible: this.sortedRequests.size > 0,
      click: () => this.openStatistics(true)
    });

    return showMenu(event, menu);
  },

  /**
   * Opens selected item in a new tab.
   */
  openRequestInTab() {
    let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
    win.openUILinkIn(this.selectedRequest.url, "tab", { relatedToCurrent: true });
  },

  /**
   * Opens selected item in the debugger
   */
  openInDebugger() {
    let toolbox = gDevTools.getToolbox(this.getTabTarget());
    toolbox.viewSourceInDebugger(this.selectedRequest.url, 0);
  },

  /**
   * Opens selected item in the style editor
   */
  openInStyleEditor() {
    let toolbox = gDevTools.getToolbox(this.getTabTarget());
    toolbox.viewSourceInStyleEditor(this.selectedRequest.url, 0);
  },

  /**
   * Copy the request url from the currently selected item.
   */
  copyUrl() {
    copyString(this.selectedRequest.url);
  },

  /**
   * Copy the request url query string parameters from the currently
   * selected item.
   */
  copyUrlParams() {
    let params = getUrlQuery(this.selectedRequest.url).split("&");
    copyString(params.join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n"));
  },

  /**
   * Copy the request form data parameters (or raw payload) from
   * the currently selected item.
   */
  copyPostData() {
    let { formDataSections, requestPostData } = this.selectedRequest;
    let params = [];

    // Try to extract any form data parameters.
    formDataSections.forEach(section => {
      let paramsArray = parseQueryString(section);
      if (paramsArray) {
        params = [...params, ...paramsArray];
      }
    });

    let string = params
      .map(param => param.name + (param.value ? "=" + param.value : ""))
      .join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n");

    // Fall back to raw payload.
    if (!string) {
      string = requestPostData.postData.text;
      if (Services.appinfo.OS !== "WINNT") {
        string = string.replace(/\r/g, "");
      }
    }
    copyString(string);
  },

  /**
   * Copy a cURL command from the currently selected item.
   */
  copyAsCurl() {
    let selected = this.selectedRequest;
    // Create a sanitized object for the Curl command generator.
    let data = {
      url: selected.url,
      method: selected.method,
      headers: selected.requestHeaders.headers,
      httpVersion: selected.httpVersion,
      postDataText: selected.requestPostData && selected.requestPostData.postData.text,
    };
    copyString(Curl.generateCommand(data));
  },

  /**
   * Copy the raw request headers from the currently selected item.
   */
  copyRequestHeaders() {
    let rawHeaders = this.selectedRequest.requestHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    copyString(rawHeaders);
  },

  /**
   * Copy the raw response headers from the currently selected item.
   */
  copyResponseHeaders() {
    let rawHeaders = this.selectedRequest.responseHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    copyString(rawHeaders);
  },

  /**
   * Copy image as data uri.
   */
  copyImageAsDataUri() {
    copyString(this.selectedRequest.responseContentDataUri);
  },

  /**
   * Save image as.
   */
  saveImageAs() {
    let { encoding, text } = this.selectedRequest.responseContent.content;
    let fileName = getUrlBaseName(this.selectedRequest.url);
    let data;
    if (encoding === "base64") {
      let decoded = atob(text);
      data = new Uint8Array(decoded.length);
      for (let i = 0; i < decoded.length; ++i) {
        data[i] = decoded.charCodeAt(i);
      }
    } else {
      data = text;
    }
    saveAs(new Blob([data]), fileName, document);
  },

  /**
   * Copy response data as a string.
   */
  copyResponse() {
    copyString(this.selectedRequest.responseContent.content.text);
  },

  /**
   * Copy HAR from the network panel content to the clipboard.
   */
  copyAllAsHar() {
    return HarExporter.copy(this.getDefaultHarOptions());
  },

  /**
   * Save HAR from the network panel content to a file.
   */
  saveAllAsHar() {
    // FIXME: This will not work in launchpad
    // document.execCommand(‘cut’/‘copy’) was denied because it was not called from
    // inside a short running user-generated event handler.
    // https://developer.mozilla.org/en-US/Add-ons/WebExtensions/Interact_with_the_clipboard
    return HarExporter.save(this.getDefaultHarOptions());
  },

  getDefaultHarOptions() {
    let form = this.getTabTarget().form;
    let title = form.title || form.url;

    return {
      getString: this.getLongString,
      items: this.sortedRequests,
      title: title
    };
  }
};

module.exports = RequestListContextMenu;
