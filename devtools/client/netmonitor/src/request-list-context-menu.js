/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Curl } = require("devtools/client/shared/curl");
const { gDevTools } = require("devtools/client/framework/devtools");
const Menu = require("devtools/client/framework/menu");
const MenuItem = require("devtools/client/framework/menu-item");
const clipboardHelper = require("devtools/shared/platform/clipboard");
const { HarExporter } = require("./har/har-exporter");
const { getLongString } = require("./utils/client");
const { L10N } = require("./utils/l10n");
const {
  formDataURI,
  getFormDataSections,
  getUrlQuery,
  parseQueryString,
} = require("./utils/request-utils");
const {
  getSelectedRequest,
  getSortedRequests,
} = require("./selectors/index");

function RequestListContextMenu({
  cloneSelectedRequest,
  openStatistics,
}) {
  this.cloneSelectedRequest = cloneSelectedRequest;
  this.openStatistics = openStatistics;
}

RequestListContextMenu.prototype = {
  get selectedRequest() {
    return getSelectedRequest(window.gStore.getState());
  },

  get sortedRequests() {
    return getSortedRequests(window.gStore.getState());
  },

  /**
   * Handle the context menu opening. Hide items if no request is selected.
   * Since visible attribute only accept boolean value but the method call may
   * return undefined, we use !! to force convert any object to boolean
   */
  open({ screenX = 0, screenY = 0 } = {}) {
    let selectedRequest = this.selectedRequest;

    let menu = new Menu();
    menu.append(new MenuItem({
      id: "request-list-context-copy-url",
      label: L10N.getStr("netmonitor.context.copyUrl"),
      accesskey: L10N.getStr("netmonitor.context.copyUrl.accesskey"),
      visible: !!selectedRequest,
      click: () => this.copyUrl(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-url-params",
      label: L10N.getStr("netmonitor.context.copyUrlParams"),
      accesskey: L10N.getStr("netmonitor.context.copyUrlParams.accesskey"),
      visible: !!(selectedRequest && getUrlQuery(selectedRequest.url)),
      click: () => this.copyUrlParams(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-post-data",
      label: L10N.getStr("netmonitor.context.copyPostData"),
      accesskey: L10N.getStr("netmonitor.context.copyPostData.accesskey"),
      visible: !!(selectedRequest && selectedRequest.requestPostData),
      click: () => this.copyPostData(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-as-curl",
      label: L10N.getStr("netmonitor.context.copyAsCurl"),
      accesskey: L10N.getStr("netmonitor.context.copyAsCurl.accesskey"),
      visible: !!selectedRequest,
      click: () => this.copyAsCurl(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedRequest,
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-request-headers",
      label: L10N.getStr("netmonitor.context.copyRequestHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyRequestHeaders.accesskey"),
      visible: !!(selectedRequest && selectedRequest.requestHeaders),
      click: () => this.copyRequestHeaders(),
    }));

    menu.append(new MenuItem({
      id: "response-list-context-copy-response-headers",
      label: L10N.getStr("netmonitor.context.copyResponseHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyResponseHeaders.accesskey"),
      visible: !!(selectedRequest && selectedRequest.responseHeaders),
      click: () => this.copyResponseHeaders(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-response",
      label: L10N.getStr("netmonitor.context.copyResponse"),
      accesskey: L10N.getStr("netmonitor.context.copyResponse.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.text &&
               selectedRequest.responseContent.content.text.length !== 0),
      click: () => this.copyResponse(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-image-as-data-uri",
      label: L10N.getStr("netmonitor.context.copyImageAsDataUri"),
      accesskey: L10N.getStr("netmonitor.context.copyImageAsDataUri.accesskey"),
      visible: !!(selectedRequest &&
               selectedRequest.responseContent &&
               selectedRequest.responseContent.content.mimeType.includes("image/")),
      click: () => this.copyImageAsDataUri(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedRequest,
    }));

    menu.append(new MenuItem({
      id: "request-list-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      visible: this.sortedRequests.size > 0,
      click: () => this.copyAllAsHar(),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-save-all-as-har",
      label: L10N.getStr("netmonitor.context.saveAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
      visible: this.sortedRequests.size > 0,
      click: () => this.saveAllAsHar(),
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!(window.NetMonitorController.supportsCustomRequest &&
               selectedRequest && !selectedRequest.isCustom),
    }));

    menu.append(new MenuItem({
      id: "request-list-context-resend",
      label: L10N.getStr("netmonitor.context.editAndResend"),
      accesskey: L10N.getStr("netmonitor.context.editAndResend.accesskey"),
      visible: !!(window.NetMonitorController.supportsCustomRequest &&
               selectedRequest && !selectedRequest.isCustom),
      click: this.cloneSelectedRequest,
    }));

    menu.append(new MenuItem({
      type: "separator",
      visible: !!selectedRequest,
    }));

    menu.append(new MenuItem({
      id: "request-list-context-newtab",
      label: L10N.getStr("netmonitor.context.newTab"),
      accesskey: L10N.getStr("netmonitor.context.newTab.accesskey"),
      visible: !!selectedRequest,
      click: () => this.openRequestInTab()
    }));

    menu.append(new MenuItem({
      id: "request-list-context-perf",
      label: L10N.getStr("netmonitor.context.perfTools"),
      accesskey: L10N.getStr("netmonitor.context.perfTools.accesskey"),
      visible: !!window.NetMonitorController.supportsPerfStats,
      click: () => this.openStatistics(true)
    }));

    menu.popup(screenX, screenY, { doc: window.parent.document });
    return menu;
  },

  /**
   * Opens selected item in a new tab.
   */
  openRequestInTab() {
    let win = Services.wm.getMostRecentWindow(gDevTools.chromeWindowType);
    win.openUILinkIn(this.selectedRequest.url, "tab", { relatedToCurrent: true });
  },

  /**
   * Copy the request url from the currently selected item.
   */
  copyUrl() {
    clipboardHelper.copyString(this.selectedRequest.url);
  },

  /**
   * Copy the request url query string parameters from the currently
   * selected item.
   */
  copyUrlParams() {
    let { url } = this.selectedRequest;
    let params = getUrlQuery(url).split("&");
    let string = params.join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n");
    clipboardHelper.copyString(string);
  },

  /**
   * Copy the request form data parameters (or raw payload) from
   * the currently selected item.
   */
  async copyPostData() {
    let selected = this.selectedRequest;

    // Try to extract any form data parameters.
    let formDataSections = await getFormDataSections(
      selected.requestHeaders,
      selected.requestHeadersFromUploadStream,
      selected.requestPostData,
      getLongString);

    let params = [];
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
      let postData = selected.requestPostData.postData.text;
      string = await getLongString(postData);
      if (Services.appinfo.OS !== "WINNT") {
        string = string.replace(/\r/g, "");
      }
    }

    clipboardHelper.copyString(string);
  },

  /**
   * Copy a cURL command from the currently selected item.
   */
  async copyAsCurl() {
    let selected = this.selectedRequest;

    // Create a sanitized object for the Curl command generator.
    let data = {
      url: selected.url,
      method: selected.method,
      headers: [],
      httpVersion: selected.httpVersion,
      postDataText: null
    };

    // Fetch header values.
    for (let { name, value } of selected.requestHeaders.headers) {
      let text = await getLongString(value);
      data.headers.push({ name: name, value: text });
    }

    // Fetch the request payload.
    if (selected.requestPostData) {
      let postData = selected.requestPostData.postData.text;
      data.postDataText = await getLongString(postData);
    }

    clipboardHelper.copyString(Curl.generateCommand(data));
  },

  /**
   * Copy the raw request headers from the currently selected item.
   */
  copyRequestHeaders() {
    let rawHeaders = this.selectedRequest.requestHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    clipboardHelper.copyString(rawHeaders);
  },

  /**
   * Copy the raw response headers from the currently selected item.
   */
  copyResponseHeaders() {
    let rawHeaders = this.selectedRequest.responseHeaders.rawHeaders.trim();
    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    clipboardHelper.copyString(rawHeaders);
  },

  /**
   * Copy image as data uri.
   */
  copyImageAsDataUri() {
    const { mimeType, text, encoding } = this.selectedRequest.responseContent.content;

    getLongString(text).then(string => {
      let data = formDataURI(mimeType, encoding, string);
      clipboardHelper.copyString(data);
    });
  },

  /**
   * Copy response data as a string.
   */
  copyResponse() {
    const { text } = this.selectedRequest.responseContent.content;

    getLongString(text).then(string => {
      clipboardHelper.copyString(string);
    });
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
    return HarExporter.save(this.getDefaultHarOptions());
  },

  getDefaultHarOptions() {
    let form = window.NetMonitorController._target.form;
    let title = form.title || form.url;

    return {
      getString: getLongString,
      items: this.sortedRequests,
      title: title
    };
  }
};

module.exports = RequestListContextMenu;
