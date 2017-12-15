/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { Curl } = require("devtools/client/shared/curl");
const { gDevTools } = require("devtools/client/framework/devtools");
const { saveAs } = require("devtools/client/shared/file-saver");
const { copyString } = require("devtools/shared/platform/clipboard");
const { showMenu } = require("devtools/client/netmonitor/src/utils/menu");
const { openRequestInTab } = require("devtools/client/netmonitor/src/utils/firefox/open-request-in-tab");
const { HarExporter } = require("../har/har-exporter");
const { L10N } = require("../utils/l10n");
const {
  formDataURI,
  getUrlQuery,
  getUrlBaseName,
  parseQueryString,
} = require("../utils/request-utils");

class RequestListContextMenu {
  constructor(props) {
    this.props = props;
  }

  open(event, selectedRequest, sortedRequests) {
    let {
      id,
      isCustom,
      formDataSections,
      method,
      mimeType,
      httpVersion,
      requestHeaders,
      requestHeadersAvailable,
      requestPostData,
      requestPostDataAvailable,
      responseHeaders,
      responseHeadersAvailable,
      responseContentAvailable,
      url,
    } = selectedRequest;
    let {
      cloneSelectedRequest,
      openStatistics,
    } = this.props;
    let menu = [];
    let copySubmenu = [];

    copySubmenu.push({
      id: "request-list-context-copy-url",
      label: L10N.getStr("netmonitor.context.copyUrl"),
      accesskey: L10N.getStr("netmonitor.context.copyUrl.accesskey"),
      visible: !!selectedRequest,
      click: () => this.copyUrl(url),
    });

    copySubmenu.push({
      id: "request-list-context-copy-url-params",
      label: L10N.getStr("netmonitor.context.copyUrlParams"),
      accesskey: L10N.getStr("netmonitor.context.copyUrlParams.accesskey"),
      visible: !!(selectedRequest && getUrlQuery(url)),
      click: () => this.copyUrlParams(url),
    });

    copySubmenu.push({
      id: "request-list-context-copy-post-data",
      label: L10N.getStr("netmonitor.context.copyPostData"),
      accesskey: L10N.getStr("netmonitor.context.copyPostData.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(selectedRequest && (requestPostDataAvailable || requestPostData)),
      click: () => this.copyPostData(id, formDataSections),
    });

    copySubmenu.push({
      id: "request-list-context-copy-as-curl",
      label: L10N.getStr("netmonitor.context.copyAsCurl"),
      accesskey: L10N.getStr("netmonitor.context.copyAsCurl.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(selectedRequest && (requestHeadersAvailable || requestHeaders)),
      click: () => this.copyAsCurl(id, url, method, requestHeaders, httpVersion),
    });

    copySubmenu.push({
      type: "separator",
      visible: copySubmenu.slice(0, 4).some((subMenu) => subMenu.visible),
    });

    copySubmenu.push({
      id: "request-list-context-copy-request-headers",
      label: L10N.getStr("netmonitor.context.copyRequestHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyRequestHeaders.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(selectedRequest && (requestHeadersAvailable || requestHeaders)),
      click: () => this.copyRequestHeaders(id, requestHeaders),
    });

    copySubmenu.push({
      id: "response-list-context-copy-response-headers",
      label: L10N.getStr("netmonitor.context.copyResponseHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyResponseHeaders.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(selectedRequest && (responseHeadersAvailable || responseHeaders)),
      click: () => this.copyResponseHeaders(id, responseHeaders),
    });

    copySubmenu.push({
      id: "request-list-context-copy-response",
      label: L10N.getStr("netmonitor.context.copyResponse"),
      accesskey: L10N.getStr("netmonitor.context.copyResponse.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(selectedRequest && responseContentAvailable),
      click: () => this.copyResponse(id),
    });

    copySubmenu.push({
      id: "request-list-context-copy-image-as-data-uri",
      label: L10N.getStr("netmonitor.context.copyImageAsDataUri"),
      accesskey: L10N.getStr("netmonitor.context.copyImageAsDataUri.accesskey"),
      visible: !!(selectedRequest && mimeType && mimeType.includes("image/")),
      click: () => this.copyImageAsDataUri(id, mimeType),
    });

    copySubmenu.push({
      type: "separator",
      visible: copySubmenu.slice(5, 9).some((subMenu) => subMenu.visible),
    });

    copySubmenu.push({
      id: "request-list-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      visible: sortedRequests.size > 0,
      click: () => this.copyAllAsHar(sortedRequests),
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
      visible: sortedRequests.size > 0,
      click: () => this.saveAllAsHar(sortedRequests),
    });

    menu.push({
      id: "request-list-context-save-image-as",
      label: L10N.getStr("netmonitor.context.saveImageAs"),
      accesskey: L10N.getStr("netmonitor.context.saveImageAs.accesskey"),
      visible: !!(selectedRequest && mimeType && mimeType.includes("image/")),
      click: () => this.saveImageAs(id, url),
    });

    menu.push({
      type: "separator",
      visible: copySubmenu.slice(10, 14).some((subMenu) => subMenu.visible),
    });

    menu.push({
      id: "request-list-context-resend",
      label: L10N.getStr("netmonitor.context.editAndResend"),
      accesskey: L10N.getStr("netmonitor.context.editAndResend.accesskey"),
      visible: !!(selectedRequest && !isCustom),
      click: cloneSelectedRequest,
    });

    menu.push({
      type: "separator",
      visible: copySubmenu.slice(15, 16).some((subMenu) => subMenu.visible),
    });

    menu.push({
      id: "request-list-context-newtab",
      label: L10N.getStr("netmonitor.context.newTab"),
      accesskey: L10N.getStr("netmonitor.context.newTab.accesskey"),
      visible: !!selectedRequest,
      click: () => this.openRequestInTab(selectedRequest),
    });

    menu.push({
      id: "request-list-context-open-in-debugger",
      label: L10N.getStr("netmonitor.context.openInDebugger"),
      accesskey: L10N.getStr("netmonitor.context.openInDebugger.accesskey"),
      visible: !!(selectedRequest && mimeType && mimeType.includes("javascript")),
      click: () => this.openInDebugger(url),
    });

    menu.push({
      id: "request-list-context-open-in-style-editor",
      label: L10N.getStr("netmonitor.context.openInStyleEditor"),
      accesskey: L10N.getStr("netmonitor.context.openInStyleEditor.accesskey"),
      visible: !!(selectedRequest &&
        Services.prefs.getBoolPref("devtools.styleeditor.enabled") &&
        mimeType && mimeType.includes("css")),
      click: () => this.openInStyleEditor(url),
    });

    menu.push({
      id: "request-list-context-perf",
      label: L10N.getStr("netmonitor.context.perfTools"),
      accesskey: L10N.getStr("netmonitor.context.perfTools.accesskey"),
      visible: sortedRequests.size > 0,
      click: () => openStatistics(true),
    });

    showMenu(event, menu);
  }

  /**
   * Opens selected item in a new tab.
   */
  openRequestInTab(selectedRequest) {
    openRequestInTab(selectedRequest);
  }

  /**
   * Opens selected item in the debugger
   */
  openInDebugger(url) {
    let toolbox = gDevTools.getToolbox(this.props.connector.getTabTarget());
    toolbox.viewSourceInDebugger(url, 0);
  }

  /**
   * Opens selected item in the style editor
   */
  openInStyleEditor(url) {
    let toolbox = gDevTools.getToolbox(this.props.connector.getTabTarget());
    toolbox.viewSourceInStyleEditor(url, 0);
  }

  /**
   * Copy the request url from the currently selected item.
   */
  copyUrl(url) {
    copyString(url);
  }

  /**
   * Copy the request url query string parameters from the currently
   * selected item.
   */
  copyUrlParams(url) {
    let params = getUrlQuery(url).split("&");
    copyString(params.join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n"));
  }

  /**
   * Copy the request form data parameters (or raw payload) from
   * the currently selected item.
   */
  async copyPostData(id, formDataSections) {
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
      let { requestPostData } = await this.props.connector
        .requestData(id, "requestPostData");
      string = requestPostData.postData.text;
      if (Services.appinfo.OS !== "WINNT") {
        string = string.replace(/\r/g, "");
      }
    }
    copyString(string);
  }

  /**
   * Copy a cURL command from the currently selected item.
   */
  async copyAsCurl(id, url, method, requestHeaders, httpVersion) {
    if (!requestHeaders) {
      requestHeaders = await this.props.connector.requestData(id, "requestHeaders");
    }
    let { requestPostData } = await this.props.connector
      .requestData(id, "requestPostData");
    // Create a sanitized object for the Curl command generator.
    let data = {
      url,
      method,
      headers: requestHeaders.headers,
      httpVersion: httpVersion,
      postDataText: requestPostData ? requestPostData.postData.text : "",
    };
    copyString(Curl.generateCommand(data));
  }

  /**
   * Copy the raw request headers from the currently selected item.
   */
  async copyRequestHeaders(id, requestHeaders) {
    if (!requestHeaders) {
      requestHeaders = await this.props.connector.requestData(id, "requestHeaders");
    }
    let rawHeaders = requestHeaders.rawHeaders.trim();

    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    copyString(rawHeaders);
  }

  /**
   * Copy the raw response headers from the currently selected item.
   */
  async copyResponseHeaders(id, responseHeaders) {
    if (!responseHeaders) {
      responseHeaders = await this.props.connector.requestData(id, "responseHeaders");
    }
    let rawHeaders = responseHeaders.rawHeaders.trim();

    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    copyString(rawHeaders);
  }

  /**
   * Copy image as data uri.
   */
  async copyImageAsDataUri(id, mimeType) {
    let responseContent = await this.props.connector.requestData(id, "responseContent");
    let { encoding, text } = responseContent.content;
    copyString(formDataURI(mimeType, encoding, text));
  }

  /**
   * Save image as.
   */
  async saveImageAs(id, url) {
    let responseContent = await this.props.connector.requestData(id, "responseContent");
    let { encoding, text } = responseContent.content;
    let fileName = getUrlBaseName(url);
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
  }

  /**
   * Copy response data as a string.
   */
  async copyResponse(id) {
    let responseContent = await this.props.connector.requestData(id, "responseContent");
    copyString(responseContent.content.text);
  }

  /**
   * Copy HAR from the network panel content to the clipboard.
   */
  copyAllAsHar(sortedRequests) {
    return HarExporter.copy(this.getDefaultHarOptions(sortedRequests));
  }

  /**
   * Save HAR from the network panel content to a file.
   */
  saveAllAsHar(sortedRequests) {
    // This will not work in launchpad
    // document.execCommand(‘cut’/‘copy’) was denied because it was not called from
    // inside a short running user-generated event handler.
    // https://developer.mozilla.org/en-US/Add-ons/WebExtensions/Interact_with_the_clipboard
    return HarExporter.save(this.getDefaultHarOptions(sortedRequests));
  }

  getDefaultHarOptions(sortedRequests) {
    let { getLongString, getTabTarget, requestData } = this.props.connector;
    let { form: { title, url } } = getTabTarget();

    return {
      getString: getLongString,
      items: sortedRequests,
      requestData,
      title: title || url,
    };
  }
}

module.exports = RequestListContextMenu;
