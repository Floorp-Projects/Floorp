/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const Services = require("Services");
const { gDevTools } = require("devtools/client/framework/devtools");
const { L10N } = require("../utils/l10n");
const {
  formDataURI,
  getUrlQuery,
  getUrlBaseName,
  parseQueryString,
} = require("../utils/request-utils");

loader.lazyRequireGetter(this, "Curl", "devtools/client/shared/curl", true);
loader.lazyRequireGetter(
  this,
  "saveAs",
  "devtools/client/shared/file-saver",
  true
);
loader.lazyRequireGetter(
  this,
  "copyString",
  "devtools/shared/platform/clipboard",
  true
);
loader.lazyRequireGetter(
  this,
  "showMenu",
  "devtools/client/shared/components/menu/utils",
  true
);
loader.lazyRequireGetter(
  this,
  "HarMenuUtils",
  "devtools/client/netmonitor/src/har/har-menu-utils",
  true
);

class RequestListContextMenu {
  constructor(props) {
    this.props = props;
  }

  /* eslint-disable complexity */
  open(event, clickedRequest, requests) {
    const {
      id,
      blockedReason,
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
      responseContent,
      responseContentAvailable,
      url,
    } = clickedRequest;
    const {
      blockSelectedRequestURL,
      connector,
      cloneRequest,
      openDetailsPanelTab,
      sendCustomRequest,
      openStatistics,
      openRequestInTab,
      unblockSelectedRequestURL,
    } = this.props;
    const menu = [];
    const copySubmenu = [];

    copySubmenu.push({
      id: "request-list-context-copy-url",
      label: L10N.getStr("netmonitor.context.copyUrl"),
      accesskey: L10N.getStr("netmonitor.context.copyUrl.accesskey"),
      visible: !!clickedRequest,
      click: () => this.copyUrl(url),
    });

    copySubmenu.push({
      id: "request-list-context-copy-url-params",
      label: L10N.getStr("netmonitor.context.copyUrlParams"),
      accesskey: L10N.getStr("netmonitor.context.copyUrlParams.accesskey"),
      visible: !!(clickedRequest && getUrlQuery(url)),
      click: () => this.copyUrlParams(url),
    });

    copySubmenu.push({
      id: "request-list-context-copy-post-data",
      label: L10N.getFormatStr("netmonitor.context.copyRequestData", method),
      accesskey: L10N.getStr("netmonitor.context.copyRequestData.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(
        clickedRequest &&
        (requestPostDataAvailable || requestPostData)
      ),
      click: () => this.copyPostData(id, formDataSections, requestPostData),
    });

    copySubmenu.push({
      id: "request-list-context-copy-as-curl",
      label: L10N.getStr("netmonitor.context.copyAsCurl"),
      accesskey: L10N.getStr("netmonitor.context.copyAsCurl.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!clickedRequest,
      click: () =>
        this.copyAsCurl(
          id,
          url,
          method,
          httpVersion,
          requestHeaders,
          requestPostData
        ),
    });

    copySubmenu.push({
      id: "request-list-context-copy-as-fetch",
      label: L10N.getStr("netmonitor.context.copyAsFetch"),
      accesskey: L10N.getStr("netmonitor.context.copyAsFetch.accesskey"),
      visible: !!clickedRequest,
      click: () =>
        this.copyAsFetch(id, url, method, requestHeaders, requestPostData),
    });

    copySubmenu.push({
      type: "separator",
      visible: copySubmenu.slice(0, 4).some(subMenu => subMenu.visible),
    });

    copySubmenu.push({
      id: "request-list-context-copy-request-headers",
      label: L10N.getStr("netmonitor.context.copyRequestHeaders"),
      accesskey: L10N.getStr("netmonitor.context.copyRequestHeaders.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(
        clickedRequest &&
        (requestHeadersAvailable || requestHeaders)
      ),
      click: () => this.copyRequestHeaders(id, requestHeaders),
    });

    copySubmenu.push({
      id: "response-list-context-copy-response-headers",
      label: L10N.getStr("netmonitor.context.copyResponseHeaders"),
      accesskey: L10N.getStr(
        "netmonitor.context.copyResponseHeaders.accesskey"
      ),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(
        clickedRequest &&
        (responseHeadersAvailable || responseHeaders)
      ),
      click: () => this.copyResponseHeaders(id, responseHeaders),
    });

    copySubmenu.push({
      id: "request-list-context-copy-response",
      label: L10N.getStr("netmonitor.context.copyResponse"),
      accesskey: L10N.getStr("netmonitor.context.copyResponse.accesskey"),
      // Menu item will be visible even if data hasn't arrived, so we need to check
      // *Available property and then fetch data lazily once user triggers the action.
      visible: !!(
        clickedRequest &&
        (responseContentAvailable || responseContent)
      ),
      click: () => this.copyResponse(id, responseContent),
    });

    copySubmenu.push({
      id: "request-list-context-copy-image-as-data-uri",
      label: L10N.getStr("netmonitor.context.copyImageAsDataUri"),
      accesskey: L10N.getStr("netmonitor.context.copyImageAsDataUri.accesskey"),
      visible: !!(
        clickedRequest &&
        (responseContentAvailable || responseContent) &&
        mimeType &&
        mimeType.includes("image/")
      ),
      click: () => this.copyImageAsDataUri(id, mimeType, responseContent),
    });

    copySubmenu.push({
      type: "separator",
      visible: copySubmenu.slice(5, 9).some(subMenu => subMenu.visible),
    });

    copySubmenu.push({
      id: "request-list-context-copy-all-as-har",
      label: L10N.getStr("netmonitor.context.copyAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.copyAllAsHar.accesskey"),
      visible: requests.length > 0,
      click: () => HarMenuUtils.copyAllAsHar(requests, connector),
    });

    menu.push({
      label: L10N.getStr("netmonitor.context.copy"),
      accesskey: L10N.getStr("netmonitor.context.copy.accesskey"),
      visible: !!clickedRequest,
      submenu: copySubmenu,
    });

    menu.push({
      id: "request-list-context-save-all-as-har",
      label: L10N.getStr("netmonitor.context.saveAllAsHar"),
      accesskey: L10N.getStr("netmonitor.context.saveAllAsHar.accesskey"),
      visible: requests.length > 0,
      click: () => HarMenuUtils.saveAllAsHar(requests, connector),
    });

    menu.push({
      id: "request-list-context-save-image-as",
      label: L10N.getStr("netmonitor.context.saveImageAs"),
      accesskey: L10N.getStr("netmonitor.context.saveImageAs.accesskey"),
      visible: !!(
        clickedRequest &&
        (responseContentAvailable || responseContent) &&
        mimeType &&
        mimeType.includes("image/")
      ),
      click: () => this.saveImageAs(id, url, responseContent),
    });

    menu.push({
      type: "separator",
      visible: copySubmenu.slice(10, 14).some(subMenu => subMenu.visible),
    });

    menu.push({
      id: "request-list-context-resend-only",
      label: L10N.getStr("netmonitor.context.resend.label"),
      accesskey: L10N.getStr("netmonitor.context.resend.accesskey"),
      visible: !!(clickedRequest && !isCustom),
      click: () => {
        cloneRequest(id);
        sendCustomRequest();
      },
    });

    menu.push({
      id: "request-list-context-resend",
      label: L10N.getStr("netmonitor.context.editAndResend"),
      accesskey: L10N.getStr("netmonitor.context.editAndResend.accesskey"),
      visible: !!(clickedRequest && !isCustom),
      click: () => {
        this.fetchRequestHeaders(id).then(() => {
          cloneRequest(id);
          openDetailsPanelTab();
        });
      },
    });

    menu.push({
      id: "request-list-context-block-url",
      label: L10N.getStr("netmonitor.context.blockURL"),
      visible: !!(clickedRequest && !blockedReason),
      click: () => {
        blockSelectedRequestURL(clickedRequest);
      },
    });

    menu.push({
      id: "request-list-context-unblock-url",
      label: L10N.getStr("netmonitor.context.unblockURL"),
      visible: !!(clickedRequest && blockedReason),
      click: () => {
        unblockSelectedRequestURL(clickedRequest);
      },
    });

    menu.push({
      type: "separator",
      visible: copySubmenu.slice(15, 16).some(subMenu => subMenu.visible),
    });

    menu.push({
      id: "request-list-context-newtab",
      label: L10N.getStr("netmonitor.context.newTab"),
      accesskey: L10N.getStr("netmonitor.context.newTab.accesskey"),
      visible: !!clickedRequest,
      click: () => openRequestInTab(id, url, requestHeaders, requestPostData),
    });

    menu.push({
      id: "request-list-context-open-in-debugger",
      label: L10N.getStr("netmonitor.context.openInDebugger"),
      accesskey: L10N.getStr("netmonitor.context.openInDebugger.accesskey"),
      visible: !!(
        clickedRequest &&
        mimeType &&
        mimeType.includes("javascript")
      ),
      click: () => this.openInDebugger(url),
    });

    menu.push({
      id: "request-list-context-open-in-style-editor",
      label: L10N.getStr("netmonitor.context.openInStyleEditor"),
      accesskey: L10N.getStr("netmonitor.context.openInStyleEditor.accesskey"),
      visible: !!(
        clickedRequest &&
        Services.prefs.getBoolPref("devtools.styleeditor.enabled") &&
        mimeType &&
        mimeType.includes("css")
      ),
      click: () => this.openInStyleEditor(url),
    });

    menu.push({
      id: "request-list-context-perf",
      label: L10N.getStr("netmonitor.context.perfTools"),
      accesskey: L10N.getStr("netmonitor.context.perfTools.accesskey"),
      visible: requests.size > 0,
      click: () => openStatistics(true),
    });

    menu.push({
      type: "separator",
    });

    menu.push({
      id: "request-list-context-use-as-fetch",
      label: L10N.getStr("netmonitor.context.useAsFetch"),
      accesskey: L10N.getStr("netmonitor.context.useAsFetch.accesskey"),
      visible: !!clickedRequest,
      click: () =>
        this.useAsFetch(id, url, method, requestHeaders, requestPostData),
    });

    if (Services.prefs.getBoolPref("devtools.netmonitor.features.search")) {
      const { toggleSearchPanel, panelOpen } = this.props;

      menu.push({
        type: "separator",
      });

      menu.push({
        id: "request-list-context-search",
        label: "Search...", // TODO localization
        accesskey: "S", // TODO localization
        type: "checkbox",
        checked: panelOpen,
        visible: !!clickedRequest,
        click: () => toggleSearchPanel(),
      });
    }

    showMenu(menu, {
      screenX: event.screenX,
      screenY: event.screenY,
    });
  }

  /**
   * Opens selected item in the debugger
   */
  openInDebugger(url) {
    const toolbox = gDevTools.getToolbox(this.props.connector.getTabTarget());
    toolbox.viewSourceInDebugger(url, 0);
  }

  /**
   * Opens selected item in the style editor
   */
  openInStyleEditor(url) {
    const toolbox = gDevTools.getToolbox(this.props.connector.getTabTarget());
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
    const params = getUrlQuery(url).split("&");
    copyString(params.join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n"));
  }

  /**
   * Copy the request form data parameters (or raw payload) from
   * the currently selected item.
   */
  async copyPostData(id, formDataSections, requestPostData) {
    let params = [];
    // Try to extract any form data parameters if formDataSections is already
    // available, which is only true if ParamsPanel has ever been mounted before.
    if (formDataSections) {
      formDataSections.forEach(section => {
        const paramsArray = parseQueryString(section);
        if (paramsArray) {
          params = [...params, ...paramsArray];
        }
      });
    }

    let string = params
      .map(param => param.name + (param.value ? "=" + param.value : ""))
      .join(Services.appinfo.OS === "WINNT" ? "\r\n" : "\n");

    // Fall back to raw payload.
    if (!string) {
      requestPostData =
        requestPostData ||
        (await this.props.connector.requestData(id, "requestPostData"));

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
  async copyAsCurl(
    id,
    url,
    method,
    httpVersion,
    requestHeaders,
    requestPostData
  ) {
    requestHeaders =
      requestHeaders ||
      (await this.props.connector.requestData(id, "requestHeaders"));

    requestPostData =
      requestPostData ||
      (await this.props.connector.requestData(id, "requestPostData"));

    // Create a sanitized object for the Curl command generator.
    const data = {
      url,
      method,
      headers: requestHeaders.headers,
      httpVersion,
      postDataText: requestPostData ? requestPostData.postData.text : "",
    };
    copyString(Curl.generateCommand(data));
  }

  /**
   * Generate fetch string
   */
  async generateFetchString(id, url, method, requestHeaders, requestPostData) {
    requestHeaders =
      requestHeaders ||
      (await this.props.connector.requestData(id, "requestHeaders"));

    requestPostData =
      requestPostData ||
      (await this.props.connector.requestData(id, "requestPostData"));

    // https://fetch.spec.whatwg.org/#forbidden-header-name
    const forbiddenHeaders = {
      "accept-charset": 1,
      "accept-encoding": 1,
      "access-control-request-headers": 1,
      "access-control-request-method": 1,
      connection: 1,
      "content-length": 1,
      cookie: 1,
      cookie2: 1,
      date: 1,
      dnt: 1,
      expect: 1,
      host: 1,
      "keep-alive": 1,
      origin: 1,
      referer: 1,
      te: 1,
      trailer: 1,
      "transfer-encoding": 1,
      upgrade: 1,
      via: 1,
    };
    const credentialHeaders = { cookie: 1, authorization: 1 };

    const headers = {};
    for (const { name, value } of requestHeaders.headers) {
      if (!forbiddenHeaders[name.toLowerCase()]) {
        headers[name] = value;
      }
    }

    const referrerHeader = requestHeaders.headers.find(
      ({ name }) => name.toLowerCase() === "referer"
    );

    const referrerPolicy = requestHeaders.headers.find(
      ({ name }) => name.toLowerCase() === "referrer-policy"
    );

    const referrer = referrerHeader ? referrerHeader.value : undefined;
    const credentials = requestHeaders.headers.some(
      ({ name }) => credentialHeaders[name.toLowerCase()]
    )
      ? "include"
      : "omit";

    const fetchOptions = {
      credentials,
      headers,
      referrer,
      referrerPolicy,
      body: requestPostData.postData.text,
      method: method,
      mode: "cors",
    };

    const options = JSON.stringify(fetchOptions, null, 4);
    const fetchString = `await fetch("${url}", ${options});`;
    return fetchString;
  }

  /**
   * Copy the currently selected item as fetch request.
   */
  async copyAsFetch(id, url, method, requestHeaders, requestPostData) {
    const fetchString = await this.generateFetchString(
      id,
      url,
      method,
      requestHeaders,
      requestPostData
    );
    copyString(fetchString);
  }

  /**
   * Open split console and fill it with fetch command for selected item
   */
  async useAsFetch(id, url, method, requestHeaders, requestPostData) {
    const fetchString = await this.generateFetchString(
      id,
      url,
      method,
      requestHeaders,
      requestPostData
    );
    const toolbox = gDevTools.getToolbox(this.props.connector.getTabTarget());
    await toolbox.openSplitConsole();
    const { hud } = await toolbox.getPanel("webconsole");
    hud.setInputValue(fetchString);
  }

  /**
   * Copy the raw request headers from the currently selected item.
   */
  async copyRequestHeaders(id, requestHeaders) {
    requestHeaders =
      requestHeaders ||
      (await this.props.connector.requestData(id, "requestHeaders"));

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
    responseHeaders =
      responseHeaders ||
      (await this.props.connector.requestData(id, "responseHeaders"));

    let rawHeaders = responseHeaders.rawHeaders.trim();

    if (Services.appinfo.OS !== "WINNT") {
      rawHeaders = rawHeaders.replace(/\r/g, "");
    }
    copyString(rawHeaders);
  }

  /**
   * Copy image as data uri.
   */
  async copyImageAsDataUri(id, mimeType, responseContent) {
    responseContent =
      responseContent ||
      (await this.props.connector.requestData(id, "responseContent"));

    const { encoding, text } = responseContent.content;
    copyString(formDataURI(mimeType, encoding, text));
  }

  /**
   * Save image as.
   */
  async saveImageAs(id, url, responseContent) {
    responseContent =
      responseContent ||
      (await this.props.connector.requestData(id, "responseContent"));

    const { encoding, text } = responseContent.content;
    const fileName = getUrlBaseName(url);
    let data;
    if (encoding === "base64") {
      const decoded = atob(text);
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
  async copyResponse(id, responseContent) {
    responseContent =
      responseContent ||
      (await this.props.connector.requestData(id, "responseContent"));

    copyString(responseContent.content.text);
  }

  async fetchRequestHeaders(id) {
    await this.props.connector.requestData(id, "requestHeaders");
  }
}

module.exports = RequestListContextMenu;
