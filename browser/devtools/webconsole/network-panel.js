/* -*- js-indent-level: 2; indent-tabs-mode: nil -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const {Cc, Ci, Cu} = require("chrome");

loader.lazyGetter(this, "NetworkHelper", () => require("devtools/toolkit/webconsole/network-helper"));
loader.lazyImporter(this, "NetUtil", "resource://gre/modules/NetUtil.jsm");
loader.lazyServiceGetter(this, "mimeService", "@mozilla.org/mime;1", "nsIMIMEService");

let WebConsoleUtils = require("devtools/toolkit/webconsole/utils").Utils;

const STRINGS_URI = "chrome://browser/locale/devtools/webconsole.properties";
let l10n = new WebConsoleUtils.l10n(STRINGS_URI);


/**
 * Creates a new NetworkPanel.
 *
 * @constructor
 * @param nsIDOMNode aParent
 *        Parent node to append the created panel to.
 * @param object aHttpActivity
 *        HttpActivity to display in the panel.
 * @param object aWebConsoleFrame
 *        The parent WebConsoleFrame object that owns this network panel
 *        instance.
 */
function NetworkPanel(aParent, aHttpActivity, aWebConsoleFrame)
{
  let doc = aParent.ownerDocument;
  this.httpActivity = aHttpActivity;
  this.webconsole = aWebConsoleFrame;
  this._responseBodyFetch = this._responseBodyFetch.bind(this);
  this._requestBodyFetch = this._requestBodyFetch.bind(this);

  // Create the underlaying panel
  this.panel = createElement(doc, "panel", {
    label: l10n.getStr("NetworkPanel.label"),
    titlebar: "normal",
    noautofocus: "true",
    noautohide: "true",
    close: "true"
  });

  // Create the iframe that displays the NetworkPanel XHTML.
  this.iframe = createAndAppendElement(this.panel, "iframe", {
    src: "chrome://browser/content/devtools/NetworkPanel.xhtml",
    type: "content",
    flex: "1"
  });

  let self = this;

  // Destroy the panel when it's closed.
  this.panel.addEventListener("popuphidden", function onPopupHide() {
    self.panel.removeEventListener("popuphidden", onPopupHide, false);
    self.panel.parentNode.removeChild(self.panel);
    self.panel = null;
    self.iframe = null;
    self.httpActivity = null;
    self.webconsole = null;

    if (self.linkNode) {
      self.linkNode._panelOpen = false;
      self.linkNode = null;
    }
  }, false);

  // Set the document object and update the content once the panel is loaded.
  this.iframe.addEventListener("load", function onLoad() {
    if (!self.iframe) {
      return;
    }

    self.iframe.removeEventListener("load", onLoad, true);
    self.update();
  }, true);

  this.panel.addEventListener("popupshown", function onPopupShown() {
    self.panel.removeEventListener("popupshown", onPopupShown, true);
    self.update();
  }, true);

  // Create the footer.
  let footer = createElement(doc, "hbox", { align: "end" });
  createAndAppendElement(footer, "spacer", { flex: 1 });

  createAndAppendElement(footer, "resizer", { dir: "bottomend" });
  this.panel.appendChild(footer);

  aParent.appendChild(this.panel);
}
exports.NetworkPanel = NetworkPanel;

NetworkPanel.prototype =
{
  /**
   * The current state of the output.
   */
  _state: 0,

  /**
   * State variables.
   */
  _INIT: 0,
  _DISPLAYED_REQUEST_HEADER: 1,
  _DISPLAYED_REQUEST_BODY: 2,
  _DISPLAYED_RESPONSE_HEADER: 3,
  _TRANSITION_CLOSED: 4,

  _fromDataRegExp: /Content-Type\:\s*application\/x-www-form-urlencoded/,

  _contentType: null,

  /**
   * Function callback invoked whenever the panel content is updated. This is
   * used only by tests.
   *
   * @private
   * @type function
   */
  _onUpdate: null,

  get document() {
    return this.iframe && this.iframe.contentWindow ?
           this.iframe.contentWindow.document : null;
  },

  /**
   * Small helper function that is nearly equal to l10n.getFormatStr
   * except that it prefixes aName with "NetworkPanel.".
   *
   * @param string aName
   *        The name of an i10n string to format. This string is prefixed with
   *        "NetworkPanel." before calling the HUDService.getFormatStr function.
   * @param array aArray
   *        Values used as placeholder for the i10n string.
   * @returns string
   *          The i10n formated string.
   */
  _format: function NP_format(aName, aArray)
  {
    return l10n.getFormatStr("NetworkPanel." + aName, aArray);
  },

  /**
   * Returns the content type of the response body. This is based on the
   * response.content.mimeType property. If this value is not available, then
   * the content type is guessed by the file extension of the request URL.
   *
   * @return string
   *         Content type or empty string if no content type could be figured
   *         out.
   */
  get contentType()
  {
    if (this._contentType) {
      return this._contentType;
    }

    let request = this.httpActivity.request;
    let response = this.httpActivity.response;

    let contentType = "";
    let types = response.content ?
                (response.content.mimeType || "").split(/,|;/) : [];
    for (let i = 0; i < types.length; i++) {
      if (types[i] in NetworkHelper.mimeCategoryMap) {
        contentType = types[i];
        break;
      }
    }

    if (contentType) {
      this._contentType = contentType;
      return contentType;
    }

    // Try to get the content type from the request file extension.
    let uri = NetUtil.newURI(request.url);
    if ((uri instanceof Ci.nsIURL) && uri.fileExtension) {
      try {
         contentType = mimeService.getTypeFromExtension(uri.fileExtension);
      }
      catch(ex) {
        // Added to prevent failures on OS X 64. No Flash?
        Cu.reportError(ex);
      }
    }

    this._contentType = contentType;
    return contentType;
  },

  /**
   *
   * @returns boolean
   *          True if the response is an image, false otherwise.
   */
  get _responseIsImage()
  {
    return this.contentType &&
           NetworkHelper.mimeCategoryMap[this.contentType] == "image";
  },

  /**
   *
   * @returns boolean
   *          True if the response body contains text, false otherwise.
   */
  get _isResponseBodyTextData()
  {
    return this.contentType ?
           NetworkHelper.isTextMimeType(this.contentType) : false;
  },

  /**
   * Tells if the server response is cached.
   *
   * @returns boolean
   *          Returns true if the server responded that the request is already
   *          in the browser's cache, false otherwise.
   */
  get _isResponseCached()
  {
    return this.httpActivity.response.status == 304;
  },

  /**
   * Tells if the request body includes form data.
   *
   * @returns boolean
   *          Returns true if the posted body contains form data.
   */
  get _isRequestBodyFormData()
  {
    let requestBody = this.httpActivity.request.postData.text;
    if (typeof requestBody == "object" && requestBody.type == "longString") {
      requestBody = requestBody.initial;
    }
    return this._fromDataRegExp.test(requestBody);
  },

  /**
   * Appends the node with id=aId by the text aValue.
   *
   * @private
   * @param string aId
   * @param string aValue
   * @return nsIDOMElement
   *         The DOM element with id=aId.
   */
  _appendTextNode: function NP__appendTextNode(aId, aValue)
  {
    let textNode = this.document.createTextNode(aValue);
    let elem = this.document.getElementById(aId);
    elem.appendChild(textNode);
    return elem;
  },

  /**
   * Generates some HTML to display the key-value pair of the aList data. The
   * generated HTML is added to node with id=aParentId.
   *
   * @param string aParentId
   *        Id of the parent node to append the list to.
   * @oaram array aList
   *        Array that holds the objects you want to display. Each object must
   *        have two properties: name and value.
   * @param boolean aIgnoreCookie
   *        If true, the key-value named "Cookie" is not added to the list.
   * @returns void
   */
  _appendList: function NP_appendList(aParentId, aList, aIgnoreCookie)
  {
    let parent = this.document.getElementById(aParentId);
    let doc = this.document;

    aList.sort(function(a, b) {
      return a.name.toLowerCase() < b.name.toLowerCase();
    });

    aList.forEach((aItem) => {
      let name = aItem.name;
      if (aIgnoreCookie && (name == "Cookie" || name == "Set-Cookie")) {
        return;
      }

      let value = aItem.value;
      let longString = null;
      if (typeof value == "object" && value.type == "longString") {
        value = value.initial;
        longString = true;
      }

      /**
       * The following code creates the HTML:
       * <tr>
       * <th scope="row" class="property-name">${line}:</th>
       * <td class="property-value">${aList[line]}</td>
       * </tr>
       * and adds it to parent.
       */
      let row = doc.createElement("tr");
      let textNode = doc.createTextNode(name + ":");
      let th = doc.createElement("th");
      th.setAttribute("scope", "row");
      th.setAttribute("class", "property-name");
      th.appendChild(textNode);
      row.appendChild(th);

      textNode = doc.createTextNode(value);
      let td = doc.createElement("td");
      td.setAttribute("class", "property-value");
      td.appendChild(textNode);

      if (longString) {
        let a = doc.createElement("a");
        a.href = "#";
        a.className = "longStringEllipsis";
        a.addEventListener("mousedown", this._longStringClick.bind(this, aItem));
        a.textContent = l10n.getStr("longStringEllipsis");
        td.appendChild(a);
      }

      row.appendChild(td);

      parent.appendChild(row);
    });
  },

  /**
   * The click event handler for the ellipsis which allows the user to retrieve
   * the full header value.
   *
   * @private
   * @param object aHeader
   *        The header object with the |name| and |value| properties.
   * @param nsIDOMEvent aEvent
   *        The DOM click event object.
   */
  _longStringClick: function NP__longStringClick(aHeader, aEvent)
  {
    aEvent.preventDefault();

    let longString = this.webconsole.webConsoleClient.longString(aHeader.value);

    longString.substring(longString.initial.length, longString.length,
      function NP__onLongStringSubstring(aResponse)
      {
        if (aResponse.error) {
          Cu.reportError("NP__onLongStringSubstring error: " + aResponse.error);
          return;
        }

        aHeader.value = aHeader.value.initial + aResponse.substring;

        let textNode = aEvent.target.previousSibling;
        textNode.textContent += aResponse.substring;
        textNode.parentNode.removeChild(aEvent.target);
      });
  },

  /**
   * Displays the node with id=aId.
   *
   * @private
   * @param string aId
   * @return nsIDOMElement
   *         The element with id=aId.
   */
  _displayNode: function NP__displayNode(aId)
  {
    let elem = this.document.getElementById(aId);
    elem.style.display = "block";
  },

  /**
   * Sets the request URL, request method, the timing information when the
   * request started and the request header content on the NetworkPanel.
   * If the request header contains cookie data, a list of sent cookies is
   * generated and a special sent cookie section is displayed + the cookie list
   * added to it.
   *
   * @returns void
   */
  _displayRequestHeader: function NP__displayRequestHeader()
  {
    let request = this.httpActivity.request;
    let requestTime = new Date(this.httpActivity.startedDateTime);

    this._appendTextNode("headUrl", request.url);
    this._appendTextNode("headMethod", request.method);
    this._appendTextNode("requestHeadersInfo",
                         l10n.timestampString(requestTime));

    this._appendList("requestHeadersContent", request.headers, true);

    if (request.cookies.length > 0) {
      this._displayNode("requestCookie");
      this._appendList("requestCookieContent", request.cookies);
    }
  },

  /**
   * Displays the request body section of the NetworkPanel and set the request
   * body content on the NetworkPanel.
   *
   * @returns void
   */
  _displayRequestBody: function NP__displayRequestBody()
  {
    let postData = this.httpActivity.request.postData;
    this._displayNode("requestBody");
    this._appendTextNode("requestBodyContent", postData.text);
  },

  /*
   * Displays the `sent form data` section. Parses the request header for the
   * submitted form data displays it inside of the `sent form data` section.
   *
   * @returns void
   */
  _displayRequestForm: function NP__processRequestForm()
  {
    let postData = this.httpActivity.request.postData.text;
    let requestBodyLines = postData.split("\n");
    let formData = requestBodyLines[requestBodyLines.length - 1].
                      replace(/\+/g, " ").split("&");

    function unescapeText(aText)
    {
      try {
        return decodeURIComponent(aText);
      }
      catch (ex) {
        return decodeURIComponent(unescape(aText));
      }
    }

    let formDataArray = [];
    for (let i = 0; i < formData.length; i++) {
      let data = formData[i];
      let idx = data.indexOf("=");
      let key = data.substring(0, idx);
      let value = data.substring(idx + 1);
      formDataArray.push({
        name: unescapeText(key),
        value: unescapeText(value)
      });
    }

    this._appendList("requestFormDataContent", formDataArray);
    this._displayNode("requestFormData");
  },

  /**
   * Displays the response section of the NetworkPanel, sets the response status,
   * the duration between the start of the request and the receiving of the
   * response header as well as the response header content on the the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseHeader: function NP__displayResponseHeader()
  {
    let timing = this.httpActivity.timings;
    let response = this.httpActivity.response;

    this._appendTextNode("headStatus",
                         [response.httpVersion, response.status,
                          response.statusText].join(" "));

    // Calculate how much time it took from the request start, until the
    // response started to be received.
    let deltaDuration = 0;
    ["dns", "connect", "send", "wait"].forEach(function (aValue) {
      let ms = timing[aValue];
      if (ms > -1) {
        deltaDuration += ms;
      }
    });

    this._appendTextNode("responseHeadersInfo",
      this._format("durationMS", [deltaDuration]));

    this._displayNode("responseContainer");
    this._appendList("responseHeadersContent", response.headers, true);

    if (response.cookies.length > 0) {
      this._displayNode("responseCookie");
      this._appendList("responseCookieContent", response.cookies);
    }
  },

  /**
   * Displays the respones image section, sets the source of the image displayed
   * in the image response section to the request URL and the duration between
   * the receiving of the response header and the end of the request. Once the
   * image is loaded, the size of the requested image is set.
   *
   * @returns void
   */
  _displayResponseImage: function NP__displayResponseImage()
  {
    let self = this;
    let timing = this.httpActivity.timings;
    let request = this.httpActivity.request;
    let response = this.httpActivity.response;
    let cached = "";

    if (this._isResponseCached) {
      cached = "Cached";
    }

    let imageNode = this.document.getElementById("responseImage" +
                                                 cached + "Node");

    let text = response.content.text;
    if (typeof text == "object" && text.type == "longString") {
      this._showResponseBodyFetchLink();
    }
    else {
      imageNode.setAttribute("src",
        "data:" + this.contentType + ";base64," + text);
    }

    // This function is called to set the imageInfo.
    function setImageInfo() {
      self._appendTextNode("responseImage" + cached + "Info",
        self._format("imageSizeDeltaDurationMS",
          [ imageNode.width, imageNode.height, timing.receive ]
        )
      );
    }

    // Check if the image is already loaded.
    if (imageNode.width != 0) {
      setImageInfo();
    }
    else {
      // Image is not loaded yet therefore add a load event.
      imageNode.addEventListener("load", function imageNodeLoad() {
        imageNode.removeEventListener("load", imageNodeLoad, false);
        setImageInfo();
      }, false);
    }

    this._displayNode("responseImage" + cached);
  },

  /**
   * Displays the response body section, sets the the duration between
   * the receiving of the response header and the end of the request as well as
   * the content of the response body on the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseBody: function NP__displayResponseBody()
  {
    let timing = this.httpActivity.timings;
    let response = this.httpActivity.response;
    let cached =  this._isResponseCached ? "Cached" : "";

    this._appendTextNode("responseBody" + cached + "Info",
      this._format("durationMS", [timing.receive]));

    this._displayNode("responseBody" + cached);

    let text = response.content.text;
    if (typeof text == "object") {
      text = text.initial;
      this._showResponseBodyFetchLink();
    }

    this._appendTextNode("responseBody" + cached + "Content", text);
  },

  /**
   * Show the "fetch response body" link.
   * @private
   */
  _showResponseBodyFetchLink: function NP__showResponseBodyFetchLink()
  {
    let content = this.httpActivity.response.content;

    let elem = this._appendTextNode("responseBodyFetchLink",
      this._format("fetchRemainingResponseContentLink",
                   [content.text.length - content.text.initial.length]));

    elem.style.display = "block";
    elem.addEventListener("mousedown", this._responseBodyFetch);
  },

  /**
   * Click event handler for the link that allows users to fetch the remaining
   * response body.
   *
   * @private
   * @param nsIDOMEvent aEvent
   */
  _responseBodyFetch: function NP__responseBodyFetch(aEvent)
  {
    aEvent.target.style.display = "none";
    aEvent.target.removeEventListener("mousedown", this._responseBodyFetch);

    let content = this.httpActivity.response.content;
    let longString = this.webconsole.webConsoleClient.longString(content.text);
    longString.substring(longString.initial.length, longString.length,
      (aResponse) =>
      {
        if (aResponse.error) {
          Cu.reportError("NP__onLongStringSubstring error: " + aResponse.error);
          return;
        }

        content.text = content.text.initial + aResponse.substring;
        let cached =  this._isResponseCached ? "Cached" : "";

        if (this._responseIsImage) {
          let imageNode = this.document.getElementById("responseImage" +
                                                       cached + "Node");
          imageNode.src =
            "data:" + this.contentType + ";base64," + content.text;
        }
        else {
          this._appendTextNode("responseBody" + cached + "Content",
                               aResponse.substring);
        }
      });
  },

  /**
   * Displays the `Unknown Content-Type hint` and sets the duration between the
   * receiving of the response header on the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseBodyUnknownType: function NP__displayResponseBodyUnknownType()
  {
    let timing = this.httpActivity.timings;

    this._displayNode("responseBodyUnknownType");
    this._appendTextNode("responseBodyUnknownTypeInfo",
      this._format("durationMS", [timing.receive]));

    this._appendTextNode("responseBodyUnknownTypeContent",
      this._format("responseBodyUnableToDisplay.content", [this.contentType]));
  },

  /**
   * Displays the `no response body` section and sets the the duration between
   * the receiving of the response header and the end of the request.
   *
   * @returns void
   */
  _displayNoResponseBody: function NP_displayNoResponseBody()
  {
    let timing = this.httpActivity.timings;

    this._displayNode("responseNoBody");
    this._appendTextNode("responseNoBodyInfo",
      this._format("durationMS", [timing.receive]));
  },

  /**
   * Updates the content of the NetworkPanel's iframe.
   *
   * @returns void
   */
  update: function NP_update()
  {
    if (!this.document || this.document.readyState != "complete") {
      return;
    }

    let updates = this.httpActivity.updates;
    let timing = this.httpActivity.timings;
    let request = this.httpActivity.request;
    let response = this.httpActivity.response;

    switch (this._state) {
      case this._INIT:
        this._displayRequestHeader();
        this._state = this._DISPLAYED_REQUEST_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_HEADER:
        // Process the request body if there is one.
        if (!this.httpActivity.discardRequestBody && request.postData.text) {
          this._updateRequestBody();
          this._state = this._DISPLAYED_REQUEST_BODY;
        }
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_BODY:
        if (!response.headers.length || !Object.keys(timing).length) {
          break;
        }
        this._displayResponseHeader();
        this._state = this._DISPLAYED_RESPONSE_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_RESPONSE_HEADER:
        if (updates.indexOf("responseContent") == -1 ||
            updates.indexOf("eventTimings") == -1) {
          break;
        }

        this._state = this._TRANSITION_CLOSED;
        if (this.httpActivity.discardResponseBody) {
          break;
        }

        if (!response.content || !response.content.text) {
          this._displayNoResponseBody();
        }
        else if (this._responseIsImage) {
          this._displayResponseImage();
        }
        else if (!this._isResponseBodyTextData) {
          this._displayResponseBodyUnknownType();
        }
        else if (response.content.text) {
          this._displayResponseBody();
        }
        break;
    }

    if (this._onUpdate) {
      this._onUpdate();
    }
  },

  /**
   * Update the panel to hold the current information we have about the request
   * body.
   * @private
   */
  _updateRequestBody: function NP__updateRequestBody()
  {
    let postData = this.httpActivity.request.postData;
    if (typeof postData.text == "object" && postData.text.type == "longString") {
      let elem = this._appendTextNode("requestBodyFetchLink",
        this._format("fetchRemainingRequestContentLink",
                     [postData.text.length - postData.text.initial.length]));

      elem.style.display = "block";
      elem.addEventListener("mousedown", this._requestBodyFetch);
      return;
    }

    // Check if we send some form data. If so, display the form data special.
    if (this._isRequestBodyFormData) {
      this._displayRequestForm();
    }
    else {
      this._displayRequestBody();
    }
  },

  /**
   * Click event handler for the link that allows users to fetch the remaining
   * request body.
   *
   * @private
   * @param nsIDOMEvent aEvent
   */
  _requestBodyFetch: function NP__requestBodyFetch(aEvent)
  {
    aEvent.target.style.display = "none";
    aEvent.target.removeEventListener("mousedown", this._responseBodyFetch);

    let postData = this.httpActivity.request.postData;
    let longString = this.webconsole.webConsoleClient.longString(postData.text);
    longString.substring(longString.initial.length, longString.length,
       (aResponse) =>
      {
        if (aResponse.error) {
          Cu.reportError("NP__onLongStringSubstring error: " + aResponse.error);
          return;
        }

        postData.text = postData.text.initial + aResponse.substring;
        this._updateRequestBody();
      });
  },
};

/**
 * Creates a DOMNode and sets all the attributes of aAttributes on the created
 * element.
 *
 * @param nsIDOMDocument aDocument
 *        Document to create the new DOMNode.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 *
 * @returns nsIDOMNode
 */
function createElement(aDocument, aTag, aAttributes)
{
  let node = aDocument.createElement(aTag);
  if (aAttributes) {
    for (let attr in aAttributes) {
      node.setAttribute(attr, aAttributes[attr]);
    }
  }
  return node;
}

/**
 * Creates a new DOMNode and appends it to aParent.
 *
 * @param nsIDOMNode aParent
 *        A parent node to append the created element.
 * @param string aTag
 *        Name of the tag for the DOMNode.
 * @param object aAttributes
 *        Attributes set on the created DOMNode.
 *
 * @returns nsIDOMNode
 */
function createAndAppendElement(aParent, aTag, aAttributes)
{
  let node = createElement(aParent.ownerDocument, aTag, aAttributes);
  aParent.appendChild(node);
  return node;
}
