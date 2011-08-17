/* -*- Mode: js2; js2-basic-offset: 2; indent-tabs-mode: nil; -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is DevTools (HeadsUpDisplay) Console Code
 *
 * The Initial Developer of the Original Code is
 *   Mozilla Foundation
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   David Dahl <ddahl@mozilla.com> (original author)
 *   Rob Campbell <rcampbell@mozilla.com>
 *   Johnathan Nightingale <jnightingale@mozilla.com>
 *   Patrick Walton <pcwalton@mozilla.com>
 *   Julian Viereck <jviereck@mozilla.com>
 *   Mihai Șucan <mihai.sucan@gmail.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cu = Components.utils;

const CONSOLEAPI_CLASS_ID = "{b49c18f8-3379-4fc0-8c90-d7772c1a9ff3}";

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource:///modules/NetworkHelper.jsm");
Cu.import("resource:///modules/PropertyPanel.jsm");

var EXPORTED_SYMBOLS = ["HUDService", "ConsoleUtils"];

XPCOMUtils.defineLazyServiceGetter(this, "scriptError",
                                   "@mozilla.org/scripterror;1",
                                   "nsIScriptError");

XPCOMUtils.defineLazyServiceGetter(this, "activityDistributor",
                                   "@mozilla.org/network/http-activity-distributor;1",
                                   "nsIHttpActivityDistributor");

XPCOMUtils.defineLazyServiceGetter(this, "mimeService",
                                   "@mozilla.org/mime;1",
                                   "nsIMIMEService");

XPCOMUtils.defineLazyServiceGetter(this, "clipboardHelper",
                                   "@mozilla.org/widget/clipboardhelper;1",
                                   "nsIClipboardHelper");

XPCOMUtils.defineLazyGetter(this, "NetUtil", function () {
  var obj = {};
  Cu.import("resource://gre/modules/NetUtil.jsm", obj);
  return obj.NetUtil;
});

XPCOMUtils.defineLazyGetter(this, "PropertyPanel", function () {
  var obj = {};
  try {
    Cu.import("resource:///modules/PropertyPanel.jsm", obj);
  } catch (err) {
    Cu.reportError(err);
  }
  return obj.PropertyPanel;
});

XPCOMUtils.defineLazyGetter(this, "AutocompletePopup", function () {
  var obj = {};
  try {
    Cu.import("resource://gre/modules/AutocompletePopup.jsm", obj);
  }
  catch (err) {
    Cu.reportError(err);
  }
  return obj.AutocompletePopup;
});

XPCOMUtils.defineLazyGetter(this, "namesAndValuesOf", function () {
  var obj = {};
  Cu.import("resource:///modules/PropertyPanel.jsm", obj);
  return obj.namesAndValuesOf;
});

function LogFactory(aMessagePrefix)
{
  function log(aMessage) {
    var _msg = aMessagePrefix + " " + aMessage + "\n";
    dump(_msg);
  }
  return log;
}

let log = LogFactory("*** HUDService:");

const HUD_STRINGS_URI = "chrome://global/locale/headsUpDisplay.properties";

XPCOMUtils.defineLazyGetter(this, "stringBundle", function () {
  return Services.strings.createBundle(HUD_STRINGS_URI);
});

// The amount of time in milliseconds that must pass between messages to
// trigger the display of a new group.
const NEW_GROUP_DELAY = 5000;

// The amount of time in milliseconds that we wait before performing a live
// search.
const SEARCH_DELAY = 200;

// The number of lines that are displayed in the console output by default, for
// each category. The user can change this number by adjusting the hidden
// "devtools.hud.loglimit.{network,cssparser,exception,console}" preferences.
const DEFAULT_LOG_LIMIT = 200;

// The various categories of messages. We start numbering at zero so we can
// use these as indexes into the MESSAGE_PREFERENCE_KEYS matrix below.
const CATEGORY_NETWORK = 0;
const CATEGORY_CSS = 1;
const CATEGORY_JS = 2;
const CATEGORY_WEBDEV = 3;
const CATEGORY_INPUT = 4;   // always on
const CATEGORY_OUTPUT = 5;  // always on

// The possible message severities. As before, we start at zero so we can use
// these as indexes into MESSAGE_PREFERENCE_KEYS.
const SEVERITY_ERROR = 0;
const SEVERITY_WARNING = 1;
const SEVERITY_INFO = 2;
const SEVERITY_LOG = 3;

// A mapping from the console API log event levels to the Web Console
// severities.
const LEVELS = {
  error: SEVERITY_ERROR,
  warn: SEVERITY_WARNING,
  info: SEVERITY_INFO,
  log: SEVERITY_LOG,
  trace: SEVERITY_LOG,
  dir: SEVERITY_LOG
};

// The lowest HTTP response code (inclusive) that is considered an error.
const MIN_HTTP_ERROR_CODE = 400;
// The highest HTTP response code (exclusive) that is considered an error.
const MAX_HTTP_ERROR_CODE = 600;

// HTTP status codes.
const HTTP_MOVED_PERMANENTLY = 301;
const HTTP_FOUND = 302;
const HTTP_SEE_OTHER = 303;
const HTTP_TEMPORARY_REDIRECT = 307;

// The HTML namespace.
const HTML_NS = "http://www.w3.org/1999/xhtml";

// The XUL namespace.
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

// The fragment of a CSS class name that identifies each category.
const CATEGORY_CLASS_FRAGMENTS = [
  "network",
  "cssparser",
  "exception",
  "console",
  "input",
  "output",
];

// The fragment of a CSS class name that identifies each severity.
const SEVERITY_CLASS_FRAGMENTS = [
  "error",
  "warn",
  "info",
  "log",
];

// The preference keys to use for each category/severity combination, indexed
// first by category (rows) and then by severity (columns).
//
// Most of these rather idiosyncratic names are historical and predate the
// division of message type into "category" and "severity".
const MESSAGE_PREFERENCE_KEYS = [
//  Error         Warning   Info    Log
  [ "network",    null,         null,   "networkinfo", ],  // Network
  [ "csserror",   "cssparser",  null,   null,          ],  // CSS
  [ "exception",  "jswarn",     null,   null,          ],  // JS
  [ "error",      "warn",       "info", "log",         ],  // Web Developer
  [ null,         null,         null,   null,          ],  // Input
  [ null,         null,         null,   null,          ],  // Output
];

// Possible directions that can be passed to HUDService.animate().
const ANIMATE_OUT = 0;
const ANIMATE_IN = 1;

// Constants used for defining the direction of JSTerm input history navigation.
const HISTORY_BACK = -1;
const HISTORY_FORWARD = 1;

// The maximum number of bytes a Network ResponseListener can hold.
const RESPONSE_BODY_LIMIT = 1024*1024; // 1 MB

// The maximum uint32 value.
const PR_UINT32_MAX = 4294967295;

// Minimum console height, in pixels.
const MINIMUM_CONSOLE_HEIGHT = 150;

// Minimum page height, in pixels. This prevents the Web Console from
// remembering a height that covers the whole page.
const MINIMUM_PAGE_HEIGHT = 50;

// The default console height, as a ratio from the content window inner height.
const DEFAULT_CONSOLE_HEIGHT = 0.33;

// Constant used when checking the typeof objects.
const TYPEOF_FUNCTION = "function";

const ERRORS = { LOG_MESSAGE_MISSING_ARGS:
                 "Missing arguments: aMessage, aConsoleNode and aMessageNode are required.",
                 CANNOT_GET_HUD: "Cannot getHeads Up Display with provided ID",
                 MISSING_ARGS: "Missing arguments",
                 LOG_OUTPUT_FAILED: "Log Failure: Could not append messageNode to outputNode",
};

/**
 * Implements the nsIStreamListener and nsIRequestObserver interface. Used
 * within the HS_httpObserverFactory function to get the response body of
 * requests.
 *
 * The code is mostly based on code listings from:
 *
 *   http://www.softwareishard.com/blog/firebug/
 *      nsitraceablechannel-intercept-http-traffic/
 *
 * @param object aHttpActivity
 *        HttpActivity object associated with this request (see
 *        HS_httpObserverFactory). As the response is done, the response header,
 *        body and status is stored on aHttpActivity.
 */
function ResponseListener(aHttpActivity) {
  this.receivedData = "";
  this.httpActivity = aHttpActivity;
}

ResponseListener.prototype =
{
  /**
   * The response will be written into the outputStream of this nsIPipe.
   * Both ends of the pipe must be blocking.
   */
  sink: null,

  /**
   * The HttpActivity object associated with this response.
   */
  httpActivity: null,

  /**
   * Stores the received data as a string.
   */
  receivedData: null,

  /**
   * The nsIRequest we are started for.
   */
  request: null,

  /**
   * Sets the httpActivity object's response header if it isn't set already.
   *
   * @param nsIRequest aRequest
   */
  setResponseHeader: function RL_setResponseHeader(aRequest)
  {
    let httpActivity = this.httpActivity;
    // Check if the header isn't set yet.
    if (!httpActivity.response.header) {
      if (aRequest instanceof Ci.nsIHttpChannel) {
      httpActivity.response.header = {};
        try {
        aRequest.visitResponseHeaders({
          visitHeader: function(aName, aValue) {
            httpActivity.response.header[aName] = aValue;
          }
        });
      }
        // Accessing the response header can throw an NS_ERROR_NOT_AVAILABLE
        // exception. Catch it and stop it to make it not show up in the.
        // This can happen if the response is not finished yet and the user
        // reloades the page.
        catch (ex) {
          delete httpActivity.response.header;
        }
      }
    }
  },

  /**
   * Set the async listener for the given nsIAsyncInputStream. This allows us to
   * wait asynchronously for any data coming from the stream.
   *
   * @param nsIAsyncInputStream aStream
   *        The input stream from where we are waiting for data to come in.
   *
   * @param nsIInputStreamCallback aListener
   *        The input stream callback you want. This is an object that must have
   *        the onInputStreamReady() method. If the argument is null, then the
   *        current callback is removed.
   *
   * @returns void
   */
  setAsyncListener: function RL_setAsyncListener(aStream, aListener)
  {
    // Asynchronously wait for the stream to be readable or closed.
    aStream.asyncWait(aListener, 0, 0, Services.tm.mainThread);
  },

  /**
   * Stores the received data, if request/response body logging is enabled. It
   * also does limit the number of stored bytes, based on the
   * RESPONSE_BODY_LIMIT constant.
   *
   * Learn more about nsIStreamListener at:
   * https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIStreamListener
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   * @param nsIInputStream aInputStream
   * @param unsigned long aOffset
   * @param unsigned long aCount
   */
  onDataAvailable: function RL_onDataAvailable(aRequest, aContext, aInputStream,
                                               aOffset, aCount)
  {
    this.setResponseHeader(aRequest);

    let data = NetUtil.readInputStreamToString(aInputStream, aCount);

    if (!this.httpActivity.response.bodyDiscarded &&
        this.receivedData.length < RESPONSE_BODY_LIMIT) {
      this.receivedData += NetworkHelper.
                           convertToUnicode(data, aRequest.contentCharset);
    }
  },

  /**
   * See documentation at
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   * @param nsISupports aContext
   */
  onStartRequest: function RL_onStartRequest(aRequest, aContext)
  {
    this.request = aRequest;

    // Always discard the response body if logging is not enabled in the Web
    // Console.
    this.httpActivity.response.bodyDiscarded =
      !HUDService.saveRequestAndResponseBodies;

    // Check response status and discard the body for redirects.
    if (!this.httpActivity.response.bodyDiscarded &&
        this.httpActivity.channel instanceof Ci.nsIHttpChannel) {
      switch (this.httpActivity.channel.responseStatus) {
        case HTTP_MOVED_PERMANENTLY:
        case HTTP_FOUND:
        case HTTP_SEE_OTHER:
        case HTTP_TEMPORARY_REDIRECT:
          this.httpActivity.response.bodyDiscarded = true;
          break;
      }
    }

    // Asynchronously wait for the data coming from the request.
    this.setAsyncListener(this.sink.inputStream, this);
  },

  /**
   * Handle the onStopRequest by storing the response header is stored on the
   * httpActivity object. The sink output stream is also closed.
   *
   * For more documentation about nsIRequestObserver go to:
   * https://developer.mozilla.org/En/NsIRequestObserver
   *
   * @param nsIRequest aRequest
   *        The request we are observing.
   * @param nsISupports aContext
   * @param nsresult aStatusCode
   */
  onStopRequest: function RL_onStopRequest(aRequest, aContext, aStatusCode)
  {
    // Retrieve the response headers, as they are, from the server.
    let response = null;
    for each (let item in HUDService.openResponseHeaders) {
      if (item.channel === this.httpActivity.channel) {
        response = item;
        break;
      }
    }

    if (response) {
      this.httpActivity.response.header = response.headers;
      delete HUDService.openResponseHeaders[response.id];
    }
    else {
      this.setResponseHeader(aRequest);
    }

    this.sink.outputStream.close();
  },

  /**
   * Clean up the response listener once the response input stream is closed.
   * This is called from onStopRequest() or from onInputStreamReady() when the
   * stream is closed.
   *
   * @returns void
   */
  onStreamClose: function RL_onStreamClose()
  {
    if (!this.httpActivity) {
      return;
    }

    // Remove our listener from the request input stream.
    this.setAsyncListener(this.sink.inputStream, null);

    if (!this.httpActivity.response.bodyDiscarded &&
        HUDService.saveRequestAndResponseBodies) {
      this.httpActivity.response.body = this.receivedData;
    }

    if (HUDService.lastFinishedRequestCallback) {
      HUDService.lastFinishedRequestCallback(this.httpActivity);
    }

    // Call update on all panels.
    this.httpActivity.panels.forEach(function(weakRef) {
      let panel = weakRef.get();
      if (panel) {
        panel.update();
      }
    });
    this.httpActivity.response.isDone = true;
    this.httpActivity = null;
    this.receivedData = "";
    this.request = null;
    this.sink = null;
    this.inputStream = null;
  },

  /**
   * The nsIInputStreamCallback for when the request input stream is ready -
   * either it has more data or it is closed.
   *
   * @param nsIAsyncInputStream aStream
   *        The sink input stream from which data is coming.
   *
   * @returns void
   */
  onInputStreamReady: function RL_onInputStreamReady(aStream)
  {
    if (!(aStream instanceof Ci.nsIAsyncInputStream) || !this.httpActivity) {
      return;
    }

    let available = -1;
    try {
      // This may throw if the stream is closed normally or due to an error.
      available = aStream.available();
    }
    catch (ex) { }

    if (available != -1) {
      if (available != 0) {
        // Note that passing 0 as the offset here is wrong, but the
        // onDataAvailable() method does not use the offset, so it does not
        // matter.
        this.onDataAvailable(this.request, null, aStream, 0, available);
      }
      this.setAsyncListener(aStream, this);
    }
    else {
      this.onStreamClose();
    }
  },

  QueryInterface: XPCOMUtils.generateQI([
    Ci.nsIStreamListener,
    Ci.nsIInputStreamCallback,
    Ci.nsIRequestObserver,
    Ci.nsISupports,
  ])
}

///////////////////////////////////////////////////////////////////////////
//// Helper for creating the network panel.

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

/**
 * Convenience function to unwrap a wrapped object.
 *
 * @param aObject the object to unwrap
 */

function unwrap(aObject)
{
  try {
    return XPCNativeWrapper.unwrap(aObject);
  } catch(e) {
    return aObject;
  }
}

///////////////////////////////////////////////////////////////////////////
//// NetworkPanel

/**
 * Creates a new NetworkPanel.
 *
 * @param nsIDOMNode aParent
 *        Parent node to append the created panel to.
 * @param object aHttpActivity
 *        HttpActivity to display in the panel.
 */
function NetworkPanel(aParent, aHttpActivity)
{
  let doc = aParent.ownerDocument;
  this.httpActivity = aHttpActivity;

  // Create the underlaying panel
  this.panel = createElement(doc, "panel", {
    label: HUDService.getStr("NetworkPanel.label"),
    titlebar: "normal",
    noautofocus: "true",
    noautohide: "true",
    close: "true"
  });

  // Create the iframe that displays the NetworkPanel XHTML.
  this.iframe = createAndAppendElement(this.panel, "iframe", {
    src: "chrome://browser/content/NetworkPanel.xhtml",
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
    self.document = null;
    self.httpActivity = null;

    if (self.linkNode) {
      self.linkNode._panelOpen = false;
      self.linkNode = null;
    }
  }, false);

  // Set the document object and update the content once the panel is loaded.
  this.panel.addEventListener("load", function onLoad() {
    self.panel.removeEventListener("load", onLoad, true);
    self.document = self.iframe.contentWindow.document;
    self.update();
  }, true);

  // Create the footer.
  let footer = createElement(doc, "hbox", { align: "end" });
  createAndAppendElement(footer, "spacer", { flex: 1 });

  createAndAppendElement(footer, "resizer", { dir: "bottomend" });
  this.panel.appendChild(footer);

  aParent.appendChild(this.panel);
}

NetworkPanel.prototype =
{
  /**
   * Callback is called once the NetworkPanel is processed completly. Used by
   * unit tests.
   */
  isDoneCallback: null,

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

  /**
   * Small helper function that is nearly equal to  HUDService.getFormatStr
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
    return HUDService.getFormatStr("NetworkPanel." + aName, aArray);
  },

  /**
   * Returns the content type of the response body. This is based on the
   * response.header["Content-Type"] info. If this value is not available, then
   * the content type is tried to be estimated by the url file ending.
   *
   * @returns string or null
   *          Content type or null if no content type could be figured out.
   */
  get _contentType()
  {
    let response = this.httpActivity.response;
    let contentTypeValue = null;

    if (response.header && response.header["Content-Type"]) {
      let types = response.header["Content-Type"].split(/,|;/);
      for (let i = 0; i < types.length; i++) {
        let type = NetworkHelper.mimeCategoryMap[types[i]];
        if (type) {
          return types[i];
        }
      }
    }

    // Try to get the content type from the request file extension.
    let uri = NetUtil.newURI(this.httpActivity.url);
    let mimeType = null;
    if ((uri instanceof Ci.nsIURL) && uri.fileExtension) {
      try {
        mimeType = mimeService.getTypeFromExtension(uri.fileExtension);
      } catch(e) {
        // Added to prevent failures on OS X 64. No Flash?
        Cu.reportError(e);
        // Return empty string to pass unittests.
        return "";
      }
    }
    return mimeType;
  },

  /**
   *
   * @returns boolean
   *          True if the response is an image, false otherwise.
   */
  get _responseIsImage()
  {
    return NetworkHelper.mimeCategoryMap[this._contentType] == "image";
  },

  /**
   *
   * @returns boolean
   *          True if the response body contains text, false otherwise.
   */
  get _isResponseBodyTextData()
  {
    let contentType = this._contentType;

    if (!contentType)
      return false;

    if (contentType.indexOf("text/") == 0) {
      return true;
    }

    switch (NetworkHelper.mimeCategoryMap[contentType]) {
      case "txt":
      case "js":
      case "json":
      case "css":
      case "html":
      case "svg":
      case "xml":
        return true;

      default:
        return false;
    }
  },

  /**
   *
   * @returns boolean
   *          Returns true if the server responded that the request is already
   *          in the browser's cache, false otherwise.
   */
  get _isResponseCached()
  {
    return this.httpActivity.response.status.indexOf("304") != -1;
  },

  /**
   *
   * @returns boolean
   *          Returns true if the posted body contains form data.
   */
  get _isRequestBodyFormData()
  {
    let requestBody = this.httpActivity.request.body;
    return this._fromDataRegExp.test(requestBody);
  },

  /**
   * Appends the node with id=aId by the text aValue.
   *
   * @param string aId
   * @param string aValue
   * @returns void
   */
  _appendTextNode: function NP_appendTextNode(aId, aValue)
  {
    let textNode = this.document.createTextNode(aValue);
    this.document.getElementById(aId).appendChild(textNode);
  },

  /**
   * Generates some HTML to display the key-value pair of the aList data. The
   * generated HTML is added to node with id=aParentId.
   *
   * @param string aParentId
   *        Id of the parent node to append the list to.
   * @oaram object aList
   *        Object that holds the key-value information to display in aParentId.
   * @param boolean aIgnoreCookie
   *        If true, the key-value named "Cookie" is not added to the list.
   * @returns void
   */
  _appendList: function NP_appendList(aParentId, aList, aIgnoreCookie)
  {
    let parent = this.document.getElementById(aParentId);
    let doc = this.document;

    let sortedList = {};
    Object.keys(aList).sort().forEach(function(aKey) {
      sortedList[aKey] = aList[aKey];
    });

    for (let key in sortedList) {
      if (aIgnoreCookie && key == "Cookie") {
        continue;
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
      let textNode = doc.createTextNode(key + ":");
      let th = doc.createElement("th");
      th.setAttribute("scope", "row");
      th.setAttribute("class", "property-name");
      th.appendChild(textNode);
      row.appendChild(th);

      textNode = doc.createTextNode(sortedList[key]);
      let td = doc.createElement("td");
      td.setAttribute("class", "property-value");
      td.appendChild(textNode);
      row.appendChild(td);

      parent.appendChild(row);
    }
  },

  /**
   * Displays the node with id=aId.
   *
   * @param string aId
   * @returns void
   */
  _displayNode: function NP_displayNode(aId)
  {
    this.document.getElementById(aId).style.display = "block";
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
  _displayRequestHeader: function NP_displayRequestHeader()
  {
    let timing = this.httpActivity.timing;
    let request = this.httpActivity.request;

    this._appendTextNode("headUrl", this.httpActivity.url);
    this._appendTextNode("headMethod", this.httpActivity.method);

    this._appendTextNode("requestHeadersInfo",
      ConsoleUtils.timestampString(timing.REQUEST_HEADER/1000));

    this._appendList("requestHeadersContent", request.header, true);

    if ("Cookie" in request.header) {
      this._displayNode("requestCookie");

      let cookies = request.header.Cookie.split(";");
      let cookieList = {};
      let cookieListSorted = {};
      cookies.forEach(function(cookie) {
        let name, value;
        [name, value] = cookie.trim().split("=");
        cookieList[name] = value;
      });
      this._appendList("requestCookieContent", cookieList);
    }
  },

  /**
   * Displays the request body section of the NetworkPanel and set the request
   * body content on the NetworkPanel.
   *
   * @returns void
   */
  _displayRequestBody: function NP_displayRequestBody() {
    this._displayNode("requestBody");
    this._appendTextNode("requestBodyContent", this.httpActivity.request.body);
  },

  /*
   * Displays the `sent form data` section. Parses the request header for the
   * submitted form data displays it inside of the `sent form data` section.
   *
   * @returns void
   */
  _displayRequestForm: function NP_processRequestForm() {
    let requestBodyLines = this.httpActivity.request.body.split("\n");
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

    let formDataObj = {};
    for (let i = 0; i < formData.length; i++) {
      let data = formData[i];
      let idx = data.indexOf("=");
      let key = data.substring(0, idx);
      let value = data.substring(idx + 1);
      formDataObj[unescapeText(key)] = unescapeText(value);
    }

    this._appendList("requestFormDataContent", formDataObj);
    this._displayNode("requestFormData");
  },

  /**
   * Displays the response section of the NetworkPanel, sets the response status,
   * the duration between the start of the request and the receiving of the
   * response header as well as the response header content on the the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseHeader: function NP_displayResponseHeader()
  {
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;

    this._appendTextNode("headStatus", response.status);

    let deltaDuration =
      Math.round((timing.RESPONSE_HEADER - timing.REQUEST_HEADER) / 1000);
    this._appendTextNode("responseHeadersInfo",
      this._format("durationMS", [deltaDuration]));

    this._displayNode("responseContainer");
    this._appendList("responseHeadersContent", response.header);
  },

  /**
   * Displays the respones image section, sets the source of the image displayed
   * in the image response section to the request URL and the duration between
   * the receiving of the response header and the end of the request. Once the
   * image is loaded, the size of the requested image is set.
   *
   * @returns void
   */
  _displayResponseImage: function NP_displayResponseImage()
  {
    let self = this;
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;
    let cached = "";

    if (this._isResponseCached) {
      cached = "Cached";
    }

    let imageNode = this.document.getElementById("responseImage" + cached +"Node");
    imageNode.setAttribute("src", this.httpActivity.url);

    // This function is called to set the imageInfo.
    function setImageInfo() {
      let deltaDuration =
        Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
      self._appendTextNode("responseImage" + cached + "Info",
        self._format("imageSizeDeltaDurationMS", [
          imageNode.width, imageNode.height, deltaDuration
        ]
      ));
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
   * @param [optional] string aCachedContent
   *        Cached content for this request. If this argument is set, the
   *        responseBodyCached section is displayed.
   * @returns void
   */
  _displayResponseBody: function NP_displayResponseBody(aCachedContent)
  {
    let timing = this.httpActivity.timing;
    let response = this.httpActivity.response;
    let cached =  "";
    if (aCachedContent) {
      cached = "Cached";
    }

    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseBody" + cached + "Info",
      this._format("durationMS", [deltaDuration]));

    this._displayNode("responseBody" + cached);
    this._appendTextNode("responseBody" + cached + "Content",
                            aCachedContent || response.body);
  },

  /**
   * Displays the `Unknown Content-Type hint` and sets the duration between the
   * receiving of the response header on the NetworkPanel.
   *
   * @returns void
   */
  _displayResponseBodyUnknownType: function NP_displayResponseBodyUnknownType()
  {
    let timing = this.httpActivity.timing;

    this._displayNode("responseBodyUnknownType");
    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseBodyUnknownTypeInfo",
      this._format("durationMS", [deltaDuration]));

    this._appendTextNode("responseBodyUnknownTypeContent",
      this._format("responseBodyUnableToDisplay.content", [this._contentType]));
  },

  /**
   * Displays the `no response body` section and sets the the duration between
   * the receiving of the response header and the end of the request.
   *
   * @returns void
   */
  _displayNoResponseBody: function NP_displayNoResponseBody()
  {
    let timing = this.httpActivity.timing;

    this._displayNode("responseNoBody");
    let deltaDuration =
      Math.round((timing.RESPONSE_COMPLETE - timing.RESPONSE_HEADER) / 1000);
    this._appendTextNode("responseNoBodyInfo",
      this._format("durationMS", [deltaDuration]));
  },

  /*
   * Calls the isDoneCallback function if one is specified.
   */
  _callIsDone: function() {
    if (this.isDoneCallback) {
      this.isDoneCallback();
    }
  },

  /**
   * Updates the content of the NetworkPanel's iframe.
   *
   * @returns void
   */
  update: function NP_update()
  {
    /**
     * After the iframe's contentWindow is ready, the document object is set.
     * If the document object isn't set yet, then the page is loaded and nothing
     * can be updated.
     */
    if (!this.document) {
      return;
    }

    let timing = this.httpActivity.timing;
    let request = this.httpActivity.request;
    let response = this.httpActivity.response;

    switch (this._state) {
      case this._INIT:
        this._displayRequestHeader();
        this._state = this._DISPLAYED_REQUEST_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_HEADER:
        // Process the request body if there is one.
        if (!request.bodyDiscarded && request.body) {
          // Check if we send some form data. If so, display the form data special.
          if (this._isRequestBodyFormData) {
            this._displayRequestForm();
          }
          else {
            this._displayRequestBody();
          }
          this._state = this._DISPLAYED_REQUEST_BODY;
        }
        // FALL THROUGH

      case this._DISPLAYED_REQUEST_BODY:
        // There is always a response header. Therefore we can skip here if
        // we don't have a response header yet and don't have to try updating
        // anything else in the NetworkPanel.
        if (!response.header) {
          break
        }
        this._displayResponseHeader();
        this._state = this._DISPLAYED_RESPONSE_HEADER;
        // FALL THROUGH

      case this._DISPLAYED_RESPONSE_HEADER:
        // Check if the transition is done.
        if (timing.TRANSACTION_CLOSE && response.isDone) {
          if (response.bodyDiscarded) {
            this._callIsDone();
          }
          else if (this._responseIsImage) {
            this._displayResponseImage();
            this._callIsDone();
          }
          else if (!this._isResponseBodyTextData) {
            this._displayResponseBodyUnknownType();
            this._callIsDone();
          }
          else if (response.body) {
            this._displayResponseBody();
            this._callIsDone();
          }
          else if (this._isResponseCached) {
            let self = this;
            NetworkHelper.loadFromCache(this.httpActivity.url,
                                        this.httpActivity.charset,
                                        function(aContent) {
              // If some content could be loaded from the cache, then display
              // the body.
              if (aContent) {
                self._displayResponseBody(aContent);
                self._callIsDone();
              }
              // Otherwise, show the "There is no response body" hint.
              else {
                self._displayNoResponseBody();
                self._callIsDone();
              }
            });
          }
          else {
            this._displayNoResponseBody();
            this._callIsDone();
          }
          this._state = this._TRANSITION_CLOSED;
        }
        break;
    }
  }
}

///////////////////////////////////////////////////////////////////////////
//// Private utility functions for the HUD service

/**
 * Ensures that the number of message nodes of type aCategory don't exceed that
 * category's line limit by removing old messages as needed.
 *
 * @param aHUDId aHUDId
 *        The HeadsUpDisplay ID.
 * @param integer aCategory
 *        The category of message nodes to limit.
 * @return number
 *         The current user-selected log limit.
 */
function pruneConsoleOutputIfNecessary(aHUDId, aCategory)
{
  // Get the log limit, either from the pref or from the constant.
  let logLimit;
  try {
    let prefName = CATEGORY_CLASS_FRAGMENTS[aCategory];
    logLimit = Services.prefs.getIntPref("devtools.hud.loglimit." + prefName);
  } catch (e) {
    logLimit = DEFAULT_LOG_LIMIT;
  }

  let hudRef = HUDService.getHudReferenceById(aHUDId);
  let outputNode = hudRef.outputNode;

  let scrollBox = outputNode.scrollBoxObject.element;
  let oldScrollHeight = scrollBox.scrollHeight;
  let scrolledToBottom = ConsoleUtils.isOutputScrolledToBottom(outputNode);

  // Prune the nodes.
  let messageNodes = outputNode.querySelectorAll(".webconsole-msg-" +
      CATEGORY_CLASS_FRAGMENTS[aCategory]);
  let removeNodes = messageNodes.length - logLimit;
  for (let i = 0; i < removeNodes; i++) {
    if (messageNodes[i].classList.contains("webconsole-msg-cssparser")) {
      let desc = messageNodes[i].childNodes[2].textContent;
      let location = "";
      if (messageNodes[i].childNodes[4]) {
        location = messageNodes[i].childNodes[4].getAttribute("title");
      }
      delete hudRef.cssNodes[desc + location];
    }
    else if (messageNodes[i].classList.contains("webconsole-msg-inspector")) {
      hudRef.pruneConsoleDirNode(messageNodes[i]);
      continue;
    }
    messageNodes[i].parentNode.removeChild(messageNodes[i]);
  }

  if (!scrolledToBottom && removeNodes > 0 &&
      oldScrollHeight != scrollBox.scrollHeight) {
    scrollBox.scrollTop -= oldScrollHeight - scrollBox.scrollHeight;
  }

  return logLimit;
}

///////////////////////////////////////////////////////////////////////////
//// The HUD service

function HUD_SERVICE()
{
  // TODO: provide mixins for FENNEC: bug 568621
  if (appName() == "FIREFOX") {
    var mixins = new FirefoxApplicationHooks();
  }
  else {
    throw new Error("Unsupported Application");
  }

  this.mixins = mixins;

  // These methods access the "this" object, but they're registered as
  // event listeners. So we hammer in the "this" binding.
  this.onTabClose = this.onTabClose.bind(this);
  this.onWindowUnload = this.onWindowUnload.bind(this);

  // Remembers the last console height, in pixels.
  this.lastConsoleHeight = Services.prefs.getIntPref("devtools.hud.height");

  // Network response bodies are piped through a buffer of the given size (in
  // bytes).
  this.responsePipeSegmentSize =
    Services.prefs.getIntPref("network.buffer.cache.size");

  /**
   * Collection of HUDIds that map to the tabs/windows/contexts
   * that a HeadsUpDisplay can be activated for.
   */
  this.activatedContexts = [];

  /**
   * Collection of outer window IDs mapping to HUD IDs.
   */
  this.windowIds = {};

  /**
   * Each HeadsUpDisplay has a set of filter preferences
   */
  this.filterPrefs = {};

  /**
   * Keeps a reference for each HeadsUpDisplay that is created
   */
  this.hudReferences = {};

  /**
   * Requests that haven't finished yet.
   */
  this.openRequests = {};

  /**
   * Response headers for requests that haven't finished yet.
   */
  this.openResponseHeaders = {};
};

HUD_SERVICE.prototype =
{
  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HS_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns (format) string
   */
  getFormatStr: function HS_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * getter for UI commands to be used by the frontend
   *
   * @returns object
   */
  get consoleUI() {
    return HeadsUpDisplayUICommands;
  },

  /**
   * The sequencer is a generator (after initialization) that returns unique
   * integers
   */
  sequencer: null,

  /**
   * Gets the ID of the outer window of this DOM window
   *
   * @param nsIDOMWindow aWindow
   * @returns integer
   */
  getWindowId: function HS_getWindowId(aWindow)
  {
    return aWindow.QueryInterface(Ci.nsIInterfaceRequestor).getInterface(Ci.nsIDOMWindowUtils).outerWindowID;
  },

  /**
   * Gets the top level content window that has an outer window with
   * the given ID or returns null if no such content window exists
   *
   * @param integer aId
   * @returns nsIDOMWindow
   */
  getWindowByWindowId: function HS_getWindowByWindowId(aId)
  {
    // In the future (post-Electrolysis), getOuterWindowWithId() could
    // return null, because the originating window could have gone away
    // while we were in the process of receiving and/or processing a
    // message. For future-proofing purposes, we do a null check here.

    let someWindow = Services.wm.getMostRecentWindow(null);
    let content = null;

    if (someWindow) {
      let windowUtils = someWindow.QueryInterface(Ci.nsIInterfaceRequestor)
                                  .getInterface(Ci.nsIDOMWindowUtils);
      content = windowUtils.getOuterWindowWithId(aId);
    }

    return content;
  },

  /**
   * Whether to save the bodies of network requests and responses. Disabled by
   * default to save memory.
   */
  saveRequestAndResponseBodies: false,

  /**
   * Tell the HUDService that a HeadsUpDisplay can be activated
   * for the window or context that has 'aContextDOMId' node id
   *
   * @param string aContextDOMId
   * @return void
   */
  registerActiveContext: function HS_registerActiveContext(aContextDOMId)
  {
    this.activatedContexts.push(aContextDOMId);
  },

  /**
   * Firefox-specific current tab getter
   *
   * @returns nsIDOMWindow
   */
  currentContext: function HS_currentContext() {
    return this.mixins.getCurrentContext();
  },

  /**
   * Tell the HUDService that a HeadsUpDisplay should be deactivated
   *
   * @param string aContextDOMId
   * @return void
   */
  unregisterActiveContext: function HS_deregisterActiveContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    var idx = this.activatedContexts.indexOf(domId);
    if (idx > -1) {
      this.activatedContexts.splice(idx, 1);
    }
  },

  /**
   * Tells callers that a HeadsUpDisplay can be activated for the context
   *
   * @param string aContextDOMId
   * @return boolean
   */
  canActivateContext: function HS_canActivateContext(aContextDOMId)
  {
    var domId = aContextDOMId.split("_")[1];
    for (var idx in this.activatedContexts) {
      if (this.activatedContexts[idx] == domId){
        return true;
      }
    }
    return false;
  },

  /**
   * Activate a HeadsUpDisplay for the given tab context.
   *
   * @param Element aContext the tab element.
   * @param boolean aAnimated animate opening the Web Console?
   * @returns void
   */
  activateHUDForContext: function HS_activateHUDForContext(aContext, aAnimated)
  {
    this.wakeup();

    let window = aContext.linkedBrowser.contentWindow;
    let nBox = aContext.ownerDocument.defaultView.
      getNotificationBox(window);
    this.registerActiveContext(nBox.id);
    this.windowInitializer(window);

    let hudId = "hud_" + nBox.id;
    let hudRef = this.hudReferences[hudId];

    if (!aAnimated || hudRef.consolePanel) {
      this.disableAnimation(hudId);
    }
  },

  /**
   * Deactivate a HeadsUpDisplay for the given tab context.
   *
   * @param nsIDOMWindow aContext
   * @param aAnimated animate closing the web console?
   * @returns void
   */
  deactivateHUDForContext: function HS_deactivateHUDForContext(aContext, aAnimated)
  {
    let browser = aContext.linkedBrowser;
    let window = browser.contentWindow;
    let chromeDocument = aContext.ownerDocument;
    let nBox = chromeDocument.defaultView.getNotificationBox(window);
    let hudId = "hud_" + nBox.id;
    let displayNode = chromeDocument.getElementById(hudId);

    if (hudId in this.hudReferences && displayNode) {
      if (!aAnimated) {
        this.storeHeight(hudId);
      }

      let hud = this.hudReferences[hudId];
      browser.webProgress.removeProgressListener(hud.progressListener);
      delete hud.progressListener;

      this.unregisterDisplay(hudId);

      window.focus();
    }
  },

  /**
   * get a unique ID from the sequence generator
   *
   * @returns integer
   */
  sequenceId: function HS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer(-1);
    }
    return this.sequencer.next();
  },

  /**
   * get the default filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getDefaultFilterPrefs: function HS_getDefaultFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the current filter prefs
   *
   * @param string aHUDId
   * @returns JS Object
   */
  getFilterPrefs: function HS_getFilterPrefs(aHUDId) {
    return this.filterPrefs[aHUDId];
  },

  /**
   * get the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @returns boolean
   */
  getFilterState: function HS_getFilterState(aHUDId, aToggleType)
  {
    if (!aHUDId) {
      return false;
    }
    try {
      var bool = this.filterPrefs[aHUDId][aToggleType];
      return bool;
    }
    catch (ex) {
      return false;
    }
  },

  /**
   * set the filter state for a specific toggle button on a heads up display
   *
   * @param string aHUDId
   * @param string aToggleType
   * @param boolean aState
   * @returns void
   */
  setFilterState: function HS_setFilterState(aHUDId, aToggleType, aState)
  {
    this.filterPrefs[aHUDId][aToggleType] = aState;
    this.adjustVisibilityForMessageType(aHUDId, aToggleType, aState);
  },

  /**
   * Splits the given console messages into groups based on their timestamps.
   *
   * @param nsIDOMNode aOutputNode
   *        The output node to alter.
   * @returns void
   */
  regroupOutput: function HS_regroupOutput(aOutputNode)
  {
    // Go through the nodes and adjust the placement of "webconsole-new-group"
    // classes.

    let nodes = aOutputNode.querySelectorAll(".hud-msg-node" +
      ":not(.hud-filtered-by-string):not(.hud-filtered-by-type)");
    let lastTimestamp;
    for (let i = 0; i < nodes.length; i++) {
      let thisTimestamp = nodes[i].timestamp;
      if (lastTimestamp != null &&
          thisTimestamp >= lastTimestamp + NEW_GROUP_DELAY) {
        nodes[i].classList.add("webconsole-new-group");
      }
      else {
        nodes[i].classList.remove("webconsole-new-group");
      }
      lastTimestamp = thisTimestamp;
    }
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the message type filter named by @aPrefKey.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aPrefKey
   *        The preference key for the message type being filtered: one of the
   *        values in the MESSAGE_PREFERENCE_KEYS table.
   * @param boolean aState
   *        True if the filter named by @aMessageType is being turned on; false
   *        otherwise.
   * @returns void
   */
  adjustVisibilityForMessageType:
  function HS_adjustVisibilityForMessageType(aHUDId, aPrefKey, aState)
  {
    let outputNode = this.getHudReferenceById(aHUDId).outputNode;
    let doc = outputNode.ownerDocument;

    // Look for message nodes ("hud-msg-node") with the given preference key
    // ("hud-msg-error", "hud-msg-cssparser", etc.) and add or remove the
    // "hud-filtered-by-type" class, which turns on or off the display.

    let xpath = ".//*[contains(@class, 'hud-msg-node') and " +
      "contains(concat(@class, ' '), 'hud-" + aPrefKey + " ')]";
    let result = doc.evaluate(xpath, outputNode, null,
      Ci.nsIDOMXPathResult.UNORDERED_NODE_SNAPSHOT_TYPE, null);
    for (let i = 0; i < result.snapshotLength; i++) {
      let node = result.snapshotItem(i);
      if (aState) {
        node.classList.remove("hud-filtered-by-type");
      }
      else {
        node.classList.add("hud-filtered-by-type");
      }
    }

    this.regroupOutput(outputNode);
  },

  /**
   * Check that the passed string matches the filter arguments.
   *
   * @param String aString
   *        to search for filter words in.
   * @param String aFilter
   *        is a string containing all of the words to filter on.
   * @returns boolean
   */
  stringMatchesFilters: function stringMatchesFilters(aString, aFilter)
  {
    if (!aFilter || !aString) {
      return true;
    }

    let searchStr = aString.toLowerCase();
    let filterStrings = aFilter.toLowerCase().split(/\s+/);
    return !filterStrings.some(function (f) {
      return searchStr.indexOf(f) == -1;
    });
  },

  /**
   * Turns the display of log nodes on and off appropriately to reflect the
   * adjustment of the search string.
   *
   * @param string aHUDId
   *        The ID of the HUD to alter.
   * @param string aSearchString
   *        The new search string.
   * @returns void
   */
  adjustVisibilityOnSearchStringChange:
  function HS_adjustVisibilityOnSearchStringChange(aHUDId, aSearchString)
  {
    let outputNode = this.getHudReferenceById(aHUDId).outputNode;

    let nodes = outputNode.querySelectorAll(".hud-msg-node");

    for (let i = 0; i < nodes.length; ++i) {
      let node = nodes[i];

      // hide nodes that match the strings
      let text = node.clipboardText;

      // if the text matches the words in aSearchString...
      if (this.stringMatchesFilters(text, aSearchString)) {
        node.classList.remove("hud-filtered-by-string");
      }
      else {
        node.classList.add("hud-filtered-by-string");
      }
    }

    this.regroupOutput(outputNode);
  },

  /**
   * Register a reference of each HeadsUpDisplay that is created
   */
  registerHUDReference:
  function HS_registerHUDReference(aHUD)
  {
    this.hudReferences[aHUD.hudId] = aHUD;

    let id = ConsoleUtils.supString(aHUD.hudId);
    Services.obs.notifyObservers(id, "web-console-created", null);
  },

  /**
   * Register a new Heads Up Display
   *
   * @returns void
   */
  registerDisplay: function HS_registerDisplay(aHUDId)
  {
    // register a display DOM node Id with the service.
    if (!aHUDId){
      throw new Error(ERRORS.MISSING_ARGS);
    }
    this.filterPrefs[aHUDId] = this.defaultFilterPrefs;
    // init storage objects:
    this.storage.createDisplay(aHUDId);
  },

  /**
   * When a display is being destroyed, unregister it first
   *
   * @param string aHUDId
   *        The ID of a HUD.
   * @returns void
   */
  unregisterDisplay: function HS_unregisterDisplay(aHUDId)
  {
    let hud = this.getHudReferenceById(aHUDId);

    // Remove children from the output. If the output is not cleared, there can
    // be leaks as some nodes has node.onclick = function; set and GC can't
    // remove the nodes then.
    hud.jsterm.clearOutput();

    hud.destroy();

    // Make sure that the console panel does not try to call
    // deactivateHUDForContext() again.
    hud.consoleWindowUnregisterOnHide = false;

    // Remove the HUDBox and the consolePanel if the Web Console is inside a
    // floating panel.
    hud.HUDBox.parentNode.removeChild(hud.HUDBox);
    if (hud.consolePanel) {
      hud.consolePanel.parentNode.removeChild(hud.consolePanel);
    }

    if (hud.splitter.parentNode) {
      hud.splitter.parentNode.removeChild(hud.splitter);
    }

    hud.jsterm.autocompletePopup.destroy();

    delete this.hudReferences[aHUDId];

    // remove the related storage object
    this.storage.removeDisplay(aHUDId);

    for (let windowID in this.windowIds) {
      if (this.windowIds[windowID] == aHUDId) {
        delete this.windowIds[windowID];
      }
    }

    this.unregisterActiveContext(aHUDId);

    let popupset = hud.chromeDocument.getElementById("mainPopupSet");
    let panels = popupset.querySelectorAll("panel[hudId=" + aHUDId + "]");
    for (let i = 0; i < panels.length; i++) {
      panels[i].hidePopup();
    }

    let id = ConsoleUtils.supString(aHUDId);
    Services.obs.notifyObservers(id, "web-console-destroyed", null);

    if (Object.keys(this.hudReferences).length == 0) {
      let autocompletePopup = hud.chromeDocument.
                              getElementById("webConsole_autocompletePopup");
      if (autocompletePopup) {
        autocompletePopup.parentNode.removeChild(autocompletePopup);
      }

      this.suspend();
    }
  },

  /**
   * "Wake up" the Web Console activity. This is called when the first Web
   * Console is open. This method initializes the various observers we have.
   *
   * @returns void
   */
  wakeup: function HS_wakeup()
  {
    if (Object.keys(this.hudReferences).length > 0) {
      return;
    }

    this.storage = new ConsoleStorage();
    this.defaultFilterPrefs = this.storage.defaultDisplayPrefs;
    this.defaultGlobalConsolePrefs = this.storage.defaultGlobalConsolePrefs;

    // begin observing HTTP traffic
    this.startHTTPObservation();

    HUDWindowObserver.init();
    HUDConsoleObserver.init();
    ConsoleAPIObserver.init();
  },

  /**
   * Suspend Web Console activity. This is called when all Web Consoles are
   * closed.
   *
   * @returns void
   */
  suspend: function HS_suspend()
  {
    activityDistributor.removeObserver(this.httpObserver);
    delete this.httpObserver;

    Services.obs.removeObserver(this.httpResponseExaminer,
                                "http-on-examine-response");

    this.openRequests = {};
    this.openResponseHeaders = {};

    // delete the storage as it holds onto channels
    delete this.storage;
    delete this.defaultFilterPrefs;
    delete this.defaultGlobalConsolePrefs;

    delete this.lastFinishedRequestCallback;

    HUDWindowObserver.uninit();
    HUDConsoleObserver.uninit();
    ConsoleAPIObserver.shutdown();
  },

  /**
   * Shutdown all HeadsUpDisplays on xpcom-shutdown
   *
   * @returns void
   */
  shutdown: function HS_shutdown()
  {
    for (let hudId in this.hudReferences) {
      this.deactivateHUDForContext(this.hudReferences[hudId].tab, false);
    }
  },

  /**
   * Returns the HeadsUpDisplay object associated to a content window.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns object
   */
  getHudByWindow: function HS_getHudByWindow(aContentWindow)
  {
    let hudId = this.getHudIdByWindow(aContentWindow);
    return hudId ? this.hudReferences[hudId] : null;
  },

  /**
   * Returns the hudId that is corresponding to the hud activated for the
   * passed aContentWindow. If there is no matching hudId null is returned.
   *
   * @param nsIDOMWindow aContentWindow
   * @returns string or null
   */
  getHudIdByWindow: function HS_getHudIdByWindow(aContentWindow)
  {
    let windowId = this.getWindowId(aContentWindow);
    return this.getHudIdByWindowId(windowId);
  },

  /**
   * Returns the hudReference for a given id.
   *
   * @param string aId
   * @returns Object
   */
  getHudReferenceById: function HS_getHudReferenceById(aId)
  {
    return aId in this.hudReferences ? this.hudReferences[aId] : null;
  },

  /**
   * Returns the hudId that is corresponding to the given outer window ID.
   *
   * @param number aWindowId
   *        the outer window ID
   * @returns string the hudId
   */
  getHudIdByWindowId: function HS_getHudIdByWindowId(aWindowId)
  {
    return this.windowIds[aWindowId];
  },

  /**
   * Get the current filter string for the HeadsUpDisplay
   *
   * @param string aHUDId
   * @returns string
   */
  getFilterStringByHUDId: function HS_getFilterStringbyHUDId(aHUDId) {
    return this.getHudReferenceById(aHUDId).filterBox.value;
  },

  /**
   * Update the filter text in the internal tracking object for all
   * filter strings
   *
   * @param nsIDOMNode aTextBoxNode
   * @returns void
   */
  updateFilterText: function HS_updateFiltertext(aTextBoxNode)
  {
    var hudId = aTextBoxNode.getAttribute("hudId");
    this.adjustVisibilityOnSearchStringChange(hudId, aTextBoxNode.value);
  },

  /**
   * Logs a message to the Web Console that originates from the window.console
   * service ("console-api-log-event" notifications).
   *
   * @param string aHUDId
   *        The ID of the Web Console to which to send the message.
   * @param object aMessage
   *        The message reported by the console service.
   * @return void
   */
  logConsoleAPIMessage: function HS_logConsoleAPIMessage(aHUDId, aMessage)
  {
    // Pipe the message to createMessageNode().
    let hud = HUDService.hudReferences[aHUDId];
    function formatResult(x) {
      return (typeof(x) == "string") ? x : hud.jsterm.formatResult(x);
    }

    let body = null;
    let clipboardText = null;
    let sourceURL = null;
    let sourceLine = 0;
    let level = aMessage.level;
    let args = aMessage.arguments;

    switch (level) {
      case "log":
      case "info":
      case "warn":
      case "error":
      case "debug":
        let mappedArguments = Array.map(args, formatResult);
        body = Array.join(mappedArguments, " ");
        sourceURL = aMessage.filename;
        sourceLine = aMessage.lineNumber;
        break;

      case "trace":
        let filename = ConsoleUtils.abbreviateSourceURL(args[0].filename);
        let functionName = args[0].functionName ||
                           this.getStr("stacktrace.anonymousFunction");
        let lineNumber = args[0].lineNumber;

        body = this.getFormatStr("stacktrace.outputMessage",
                                 [filename, functionName, lineNumber]);

        sourceURL = args[0].filename;
        sourceLine = args[0].lineNumber;

        clipboardText = "";

        args.forEach(function(aFrame) {
          clipboardText += aFrame.filename + " :: " +
                           aFrame.functionName + " :: " +
                           aFrame.lineNumber + "\n";
        });

        clipboardText = clipboardText.trimRight();
        break;

      case "dir":
        body = unwrap(args[0]);
        clipboardText = body.toString();
        sourceURL = aMessage.filename;
        sourceLine = aMessage.lineNumber;
        break;

      default:
        Cu.reportError("Unknown Console API log level: " + level);
        return;
    }

    let node = ConsoleUtils.createMessageNode(hud.outputNode.ownerDocument,
                                              CATEGORY_WEBDEV,
                                              LEVELS[level],
                                              body,
                                              sourceURL,
                                              sourceLine,
                                              clipboardText,
                                              level);

    // Make the node bring up the property panel, to allow the user to inspect
    // the stack trace.
    if (level == "trace") {
      node._stacktrace = args;

      let linkNode = node.querySelector(".webconsole-msg-body");
      linkNode.classList.add("hud-clickable");
      linkNode.setAttribute("aria-haspopup", "true");

      node.addEventListener("mousedown", function(aEvent) {
        this._startX = aEvent.clientX;
        this._startY = aEvent.clientY;
      }, false);

      node.addEventListener("click", function(aEvent) {
        if (aEvent.detail != 1 || aEvent.button != 0 ||
            (this._startX != aEvent.clientX &&
             this._startY != aEvent.clientY)) {
          return;
        }

        if (!this._panelOpen) {
          let propPanel = hud.jsterm.openPropertyPanel(null,
                                                       node._stacktrace,
                                                       this);
          propPanel.panel.setAttribute("hudId", aHUDId);
          this._panelOpen = true;
        }
      }, false);
    }

    ConsoleUtils.outputMessageNode(node, aHUDId);

    if (level == "dir") {
      // Initialize the inspector message node, by setting the PropertyTreeView
      // object on the tree view. This has to be done *after* the node is
      // shown, because the tree binding must be attached first.
      let tree = node.querySelector("tree");
      tree.view = node.propertyTreeView;
    }
  },

  /**
   * Inform user that the Web Console API has been replaced by a script
   * in a content page.
   *
   * @param string aHUDId
   *        The ID of the Web Console to which to send the message.
   * @return void
   */
  logWarningAboutReplacedAPI:
  function HS_logWarningAboutReplacedAPI(aHUDId)
  {
    let hud = this.hudReferences[aHUDId];
    let chromeDocument = hud.HUDBox.ownerDocument;
    let message = stringBundle.GetStringFromName("ConsoleAPIDisabled");
    let node = ConsoleUtils.createMessageNode(chromeDocument, CATEGORY_JS,
                                              SEVERITY_WARNING, message);
    ConsoleUtils.outputMessageNode(node, aHUDId);
  },

  /**
   * Reports an error in the page source, either JavaScript or CSS.
   *
   * @param number aCategory
   *        The category of the message; either CATEGORY_CSS or CATEGORY_JS.
   * @param nsIScriptError aScriptError
   *        The error message to report.
   * @return void
   */
  reportPageError: function HS_reportPageError(aCategory, aScriptError)
  {
    if (aCategory != CATEGORY_CSS && aCategory != CATEGORY_JS) {
      throw Components.Exception("Unsupported category (must be one of CSS " +
                                 "or JS)", Cr.NS_ERROR_INVALID_ARG,
                                 Components.stack.caller);
    }

    // Warnings and legacy strict errors become warnings; other types become
    // errors.
    let severity = SEVERITY_ERROR;
    if ((aScriptError.flags & aScriptError.warningFlag) ||
        (aScriptError.flags & aScriptError.strictFlag)) {
      severity = SEVERITY_WARNING;
    }

    let window = HUDService.getWindowByWindowId(aScriptError.outerWindowID);
    if (window) {
      let hudId = HUDService.getHudIdByWindow(window.top);
      if (hudId) {
        let outputNode = this.hudReferences[hudId].outputNode;
        let chromeDocument = outputNode.ownerDocument;

        let node = ConsoleUtils.createMessageNode(chromeDocument,
                                                  aCategory,
                                                  severity,
                                                  aScriptError.errorMessage,
                                                  aScriptError.sourceName,
                                                  aScriptError.lineNumber);

        ConsoleUtils.outputMessageNode(node, hudId);
      }
    }
  },

  /**
   * Register a Gecko app's specialized ApplicationHooks object
   *
   * @returns void or throws "UNSUPPORTED APPLICATION" error
   */
  registerApplicationHooks:
  function HS_registerApplications(aAppName, aHooksObject)
  {
    switch(aAppName) {
      case "FIREFOX":
        this.applicationHooks = aHooksObject;
        return;
      default:
        throw new Error("MOZ APPLICATION UNSUPPORTED");
    }
  },

  /**
   * Registry of ApplicationHooks used by specified Gecko Apps
   *
   * @returns Specific Gecko 'ApplicationHooks' Object/Mixin
   */
  applicationHooks: null,

  /**
   * Assign a function to this property to listen for finished httpRequests.
   * Used by unit tests.
   */
  lastFinishedRequestCallback: null,

  /**
   * Opens a NetworkPanel.
   *
   * @param nsIDOMNode aNode
   *        DOMNode to display the panel next to.
   * @param object aHttpActivity
   *        httpActivity object. The data of this object is displayed in the
   *        NetworkPanel.
   * @returns NetworkPanel
   */
  openNetworkPanel: function HS_openNetworkPanel(aNode, aHttpActivity)
  {
    let doc = aNode.ownerDocument;
    let parent = doc.getElementById("mainPopupSet");
    let netPanel = new NetworkPanel(parent, aHttpActivity);
    netPanel.linkNode = aNode;

    let panel = netPanel.panel;
    panel.openPopup(aNode, "after_pointer", 0, 0, false, false);
    panel.sizeTo(450, 500);
    panel.setAttribute("hudId", aHttpActivity.hudId);
    aHttpActivity.panels.push(Cu.getWeakReference(netPanel));
    return netPanel;
  },

  /**
   * Begin observing HTTP traffic that we care about,
   * namely traffic that originates inside any context that a Heads Up Display
   * is active for.
   */
  startHTTPObservation: function HS_httpObserverFactory()
  {
    // creates an observer for http traffic
    var self = this;
    var httpObserver = {
      observeActivity :
      function HS_SHO_observeActivity(aChannel,
                                      aActivityType,
                                      aActivitySubtype,
                                      aTimestamp,
                                      aExtraSizeData,
                                      aExtraStringData)
      {
        if (aActivityType ==
              activityDistributor.ACTIVITY_TYPE_HTTP_TRANSACTION ||
            aActivityType ==
              activityDistributor.ACTIVITY_TYPE_SOCKET_TRANSPORT) {

          aChannel = aChannel.QueryInterface(Ci.nsIHttpChannel);

          let transCodes = this.httpTransactionCodes;
          let hudId;

          if (aActivitySubtype ==
              activityDistributor.ACTIVITY_SUBTYPE_REQUEST_HEADER ) {
            // Try to get the source window of the request.
            let win = NetworkHelper.getWindowForRequest(aChannel);
            if (!win) {
              return;
            }

            // Try to get the hudId that is associated to the window.
            hudId = self.getHudIdByWindow(win.top);
            if (!hudId) {
              return;
            }

            // The httpActivity object will hold all information concerning
            // this request and later response.

            let httpActivity = {
              id: self.sequenceId(),
              hudId: hudId,
              url: aChannel.URI.spec,
              method: aChannel.requestMethod,
              channel: aChannel,
              charset: win.document.characterSet,

              panels: [],
              request: {
                header: { }
              },
              response: {
                header: null
              },
              timing: {
                "REQUEST_HEADER": aTimestamp
              }
            };

            // Add a new output entry.
            let loggedNode = self.logNetActivity(httpActivity);

            // In some cases loggedNode can be undefined (e.g. if an image was
            // requested). Don't continue in such a case.
            if (!loggedNode) {
              return;
            }

            aChannel.QueryInterface(Ci.nsITraceableChannel);

            // The response will be written into the outputStream of this pipe.
            // This allows us to buffer the data we are receiving and read it
            // asynchronously.
            // Both ends of the pipe must be blocking.
            let sink = Cc["@mozilla.org/pipe;1"].createInstance(Ci.nsIPipe);

            // The streams need to be blocking because this is required by the
            // stream tee.
            sink.init(false, false, HUDService.responsePipeSegmentSize,
                      PR_UINT32_MAX, null);

            // Add listener for the response body.
            let newListener = new ResponseListener(httpActivity);

            // Remember the input stream, so it isn't released by GC.
            newListener.inputStream = sink.inputStream;
            newListener.sink = sink;

            let tee = Cc["@mozilla.org/network/stream-listener-tee;1"].
                      createInstance(Ci.nsIStreamListenerTee);

            let originalListener = aChannel.setNewListener(tee);

            tee.init(originalListener, sink.outputStream, newListener);

            // Copy the request header data.
            aChannel.visitRequestHeaders({
              visitHeader: function(aName, aValue) {
                httpActivity.request.header[aName] = aValue;
              }
            });

            // Store the loggedNode and the httpActivity object for later reuse.
            let linkNode = loggedNode.querySelector(".webconsole-msg-link");

            httpActivity.messageObject = {
              messageNode: loggedNode,
              linkNode:    linkNode
            };
            self.openRequests[httpActivity.id] = httpActivity;

            // Make the network span clickable.
            linkNode.setAttribute("aria-haspopup", "true");
            linkNode.addEventListener("mousedown", function(aEvent) {
              this._startX = aEvent.clientX;
              this._startY = aEvent.clientY;
            }, false);

            linkNode.addEventListener("click", function(aEvent) {
              if (aEvent.detail != 1 || aEvent.button != 0 ||
                  (this._startX != aEvent.clientX &&
                   this._startY != aEvent.clientY)) {
                return;
              }

              if (!this._panelOpen) {
                self.openNetworkPanel(this, httpActivity);
                this._panelOpen = true;
              }
            }, false);
          }
          else {
            // Iterate over all currently ongoing requests. If aChannel can't
            // be found within them, then exit this function.
            let httpActivity = null;
            for each (let item in self.openRequests) {
              if (item.channel !== aChannel) {
                continue;
              }
              httpActivity = item;
              break;
            }

            if (!httpActivity) {
              return;
            }

            hudId = httpActivity.hudId;
            let msgObject = httpActivity.messageObject;

            let updatePanel = false;
            let data;
            // Store the time information for this activity subtype.
            httpActivity.timing[transCodes[aActivitySubtype]] = aTimestamp;

            switch (aActivitySubtype) {
              case activityDistributor.ACTIVITY_SUBTYPE_REQUEST_BODY_SENT: {
                if (!self.saveRequestAndResponseBodies) {
                  httpActivity.request.bodyDiscarded = true;
                  break;
                }

                let gBrowser = msgObject.messageNode.ownerDocument.
                               defaultView.gBrowser;
                let HUD = HUDService.hudReferences[hudId];
                let browser = gBrowser.
                              getBrowserForDocument(HUD.contentDocument);

                let sentBody = NetworkHelper.
                               readPostTextFromRequest(aChannel, browser);
                if (!sentBody) {
                  // If the request URL is the same as the current page url, then
                  // we can try to get the posted text from the page directly.
                  // This check is necessary as otherwise the
                  //   NetworkHelper.readPostTextFromPage
                  // function is called for image requests as well but these
                  // are not web pages and as such don't store the posted text
                  // in the cache of the webpage.
                  if (httpActivity.url == browser.contentWindow.location.href) {
                    sentBody = NetworkHelper.readPostTextFromPage(browser);
                  }
                  if (!sentBody) {
                    sentBody = "";
                  }
                }
                httpActivity.request.body = sentBody;
                break;
              }

              case activityDistributor.ACTIVITY_SUBTYPE_RESPONSE_HEADER: {
                // aExtraStringData contains the response header. The first line
                // contains the response status (e.g. HTTP/1.1 200 OK).
                //
                // Note: The response header is not saved here. Calling the
                //       aChannel.visitResponseHeaders at this point sometimes
                //       causes an NS_ERROR_NOT_AVAILABLE exception. Therefore,
                //       the response header and response body is stored on the
                //       httpActivity object within the RL_onStopRequest function.
                httpActivity.response.status =
                  aExtraStringData.split(/\r\n|\n|\r/)[0];

                // Add the response status.
                let linkNode = msgObject.linkNode;
                let statusNode = linkNode.
                  querySelector(".webconsole-msg-status");
                let statusText = "[" + httpActivity.response.status + "]";
                statusNode.setAttribute("value", statusText);

                let clipboardTextPieces =
                  [ httpActivity.method, httpActivity.url, statusText ];
                msgObject.messageNode.clipboardText =
                  clipboardTextPieces.join(" ");

                let status = parseInt(httpActivity.response.status.
                  replace(/^HTTP\/\d\.\d (\d+).+$/, "$1"));

                if (status >= MIN_HTTP_ERROR_CODE &&
                    status < MAX_HTTP_ERROR_CODE) {
                  ConsoleUtils.setMessageType(msgObject.messageNode,
                                              CATEGORY_NETWORK,
                                              SEVERITY_ERROR);
                }

                break;
              }

              case activityDistributor.ACTIVITY_SUBTYPE_TRANSACTION_CLOSE: {
                let timing = httpActivity.timing;
                let requestDuration =
                  Math.round((timing.RESPONSE_COMPLETE -
                                timing.REQUEST_HEADER) / 1000);

                // Add the request duration.
                let linkNode = msgObject.linkNode;
                let statusNode = linkNode.
                  querySelector(".webconsole-msg-status");

                let statusText = httpActivity.response.status;
                let timeText = self.getFormatStr("NetworkPanel.durationMS",
                                                 [ requestDuration ]);
                let fullStatusText = "[" + statusText + " " + timeText + "]";
                statusNode.setAttribute("value", fullStatusText);

                let clipboardTextPieces =
                  [ httpActivity.method, httpActivity.url, fullStatusText ];
                msgObject.messageNode.clipboardText =
                  clipboardTextPieces.join(" ");

                delete httpActivity.messageObject;
                delete self.openRequests[httpActivity.id];
                updatePanel = true;
                break;
              }
            }

            if (updatePanel) {
              httpActivity.panels.forEach(function(weakRef) {
                let panel = weakRef.get();
                if (panel) {
                  panel.update();
                }
              });
            }
          }
        }
      },

      httpTransactionCodes: {
        0x5001: "REQUEST_HEADER",
        0x5002: "REQUEST_BODY_SENT",
        0x5003: "RESPONSE_START",
        0x5004: "RESPONSE_HEADER",
        0x5005: "RESPONSE_COMPLETE",
        0x5006: "TRANSACTION_CLOSE",

        0x804b0003: "STATUS_RESOLVING",
        0x804b000b: "STATUS_RESOLVED",
        0x804b0007: "STATUS_CONNECTING_TO",
        0x804b0004: "STATUS_CONNECTED_TO",
        0x804b0005: "STATUS_SENDING_TO",
        0x804b000a: "STATUS_WAITING_FOR",
        0x804b0006: "STATUS_RECEIVING_FROM"
      }
    };

    this.httpObserver = httpObserver;

    activityDistributor.addObserver(httpObserver);

    // This is used to find the correct HTTP response headers.
    Services.obs.addObserver(this.httpResponseExaminer,
                             "http-on-examine-response", false);
  },

  /**
   * Observe notifications for the http-on-examine-response topic, coming from
   * the nsIObserver service.
   *
   * @param string aTopic
   * @param nsIHttpChannel aSubject
   * @returns void
   */
  httpResponseExaminer: function HS_httpResponseExaminer(aSubject, aTopic)
  {
    if (aTopic != "http-on-examine-response" ||
        !(aSubject instanceof Ci.nsIHttpChannel)) {
      return;
    }

    let channel = aSubject.QueryInterface(Ci.nsIHttpChannel);
    let win = NetworkHelper.getWindowForRequest(channel);
    if (!win) {
      return;
    }
    let hudId = HUDService.getHudIdByWindow(win);
    if (!hudId) {
      return;
    }

    let response = {
      id: HUDService.sequenceId(),
      hudId: hudId,
      channel: channel,
      headers: {},
    };

    try {
      channel.visitResponseHeaders({
        visitHeader: function(aName, aValue) {
          response.headers[aName] = aValue;
        }
      });
    }
    catch (ex) {
      delete response.headers;
    }

    if (response.headers) {
      HUDService.openResponseHeaders[response.id] = response;
    }
  },

  /**
   * Logs network activity.
   *
   * @param object aActivityObject
   *        The activity to log.
   * @returns void
   */
  logNetActivity: function HS_logNetActivity(aActivityObject)
  {
    let hudId = aActivityObject.hudId;
    let outputNode = this.hudReferences[hudId].outputNode;

    let chromeDocument = outputNode.ownerDocument;
    let msgNode = chromeDocument.createElementNS(XUL_NS, "hbox");

    let methodNode = chromeDocument.createElementNS(XUL_NS, "label");
    methodNode.setAttribute("value", aActivityObject.method);
    methodNode.classList.add("webconsole-msg-body-piece");
    msgNode.appendChild(methodNode);

    let linkNode = chromeDocument.createElementNS(XUL_NS, "hbox");
    linkNode.setAttribute("flex", "1");
    linkNode.classList.add("webconsole-msg-body-piece");
    linkNode.classList.add("webconsole-msg-link");
    msgNode.appendChild(linkNode);

    let urlNode = chromeDocument.createElementNS(XUL_NS, "label");
    urlNode.setAttribute("crop", "center");
    urlNode.setAttribute("flex", "1");
    urlNode.setAttribute("title", aActivityObject.url);
    urlNode.setAttribute("value", aActivityObject.url);
    urlNode.classList.add("hud-clickable");
    urlNode.classList.add("webconsole-msg-body-piece");
    urlNode.classList.add("webconsole-msg-url");
    linkNode.appendChild(urlNode);

    let statusNode = chromeDocument.createElementNS(XUL_NS, "label");
    statusNode.setAttribute("value", "");
    statusNode.classList.add("hud-clickable");
    statusNode.classList.add("webconsole-msg-body-piece");
    statusNode.classList.add("webconsole-msg-status");
    linkNode.appendChild(statusNode);

    let clipboardText = aActivityObject.method + " " + aActivityObject.url;

    let messageNode = ConsoleUtils.createMessageNode(chromeDocument,
                                                     CATEGORY_NETWORK,
                                                     SEVERITY_LOG,
                                                     msgNode,
                                                     null,
                                                     null,
                                                     clipboardText);

    ConsoleUtils.outputMessageNode(messageNode, aActivityObject.hudId);
    return messageNode;
  },

  /**
   * Initialize the JSTerm object to create a JS Workspace by attaching the UI
   * into the given parent node, using the mixin.
   *
   * @param nsIDOMWindow aContext the context used for evaluating user input
   * @param nsIDOMNode aParentNode where to attach the JSTerm
   * @param object aConsole
   *        Console object used within the JSTerm instance to report errors
   *        and log data (by calling console.error(), console.log(), etc).
   */
  initializeJSTerm: function HS_initializeJSTerm(aContext, aParentNode, aConsole)
  {
    // create Initial JS Workspace:
    var context = Cu.getWeakReference(aContext);

    // Attach the UI into the target parent node using the mixin.
    var firefoxMixin = new JSTermFirefoxMixin(context, aParentNode);
    var jsTerm = new JSTerm(context, aParentNode, firefoxMixin, aConsole);

    // TODO: injection of additional functionality needs re-thinking/api
    // see bug 559748
  },

  /**
   * Creates a generator that always returns a unique number for use in the
   * indexes
   *
   * @returns Generator
   */
  createSequencer: function HS_createSequencer(aInt)
  {
    function sequencer(aInt)
    {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(aInt);
  },

  // See jsapi.h (JSErrorReport flags):
  // http://mxr.mozilla.org/mozilla-central/source/js/src/jsapi.h#3429
  scriptErrorFlags: {
    0: "error", // JSREPORT_ERROR
    1: "warn", // JSREPORT_WARNING
    2: "exception", // JSREPORT_EXCEPTION
    4: "error", // JSREPORT_STRICT | JSREPORT_ERROR
    5: "warn", // JSREPORT_STRICT | JSREPORT_WARNING
    8: "error", // JSREPORT_STRICT_MODE_ERROR
    13: "warn", // JSREPORT_STRICT_MODE_ERROR | JSREPORT_WARNING | JSREPORT_ERROR
  },

  /**
   * replacement strings (L10N)
   */
  scriptMsgLogLevel: {
    0: "typeError", // JSREPORT_ERROR
    1: "typeWarning", // JSREPORT_WARNING
    2: "typeException", // JSREPORT_EXCEPTION
    4: "typeError", // JSREPORT_STRICT | JSREPORT_ERROR
    5: "typeStrict", // JSREPORT_STRICT | JSREPORT_WARNING
    8: "typeError", // JSREPORT_STRICT_MODE_ERROR
    13: "typeWarning", // JSREPORT_STRICT_MODE_ERROR | JSREPORT_WARNING | JSREPORT_ERROR
  },

  /**
   * onTabClose event handler function
   *
   * @param aEvent
   * @returns void
   */
  onTabClose: function HS_onTabClose(aEvent)
  {
    this.deactivateHUDForContext(aEvent.target, false);
  },

  /**
   * Called whenever a browser window closes. Cleans up any consoles still
   * around.
   *
   * @param nsIDOMEvent aEvent
   *        The dispatched event.
   * @returns void
   */
  onWindowUnload: function HS_onWindowUnload(aEvent)
  {
    let window = aEvent.target.defaultView;

    window.removeEventListener("unload", this.onWindowUnload, false);

    let gBrowser = window.gBrowser;
    let tabContainer = gBrowser.tabContainer;

    tabContainer.removeEventListener("TabClose", this.onTabClose, false);

    let tab = tabContainer.firstChild;
    while (tab != null) {
      this.deactivateHUDForContext(tab, false);
      tab = tab.nextSibling;
    }

    if (window.webConsoleCommandController) {
      window.controllers.removeController(window.webConsoleCommandController);
      window.webConsoleCommandController = null;
    }
  },

  /**
   * windowInitializer - checks what Gecko app is running and inits the HUD
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  windowInitializer: function HS_WindowInitalizer(aContentWindow)
  {
    var xulWindow = aContentWindow.QueryInterface(Ci.nsIInterfaceRequestor)
      .getInterface(Ci.nsIWebNavigation)
                      .QueryInterface(Ci.nsIDocShell)
                      .chromeEventHandler.ownerDocument.defaultView;

    let xulWindow = unwrap(xulWindow);

    let docElem = xulWindow.document.documentElement;
    if (!docElem || docElem.getAttribute("windowtype") != "navigator:browser" ||
        !xulWindow.gBrowser) {
      // Do not do anything unless we have a browser window.
      // This may be a view-source window or other type of non-browser window.
      return;
    }

    let gBrowser = xulWindow.gBrowser;

    let _browser = gBrowser.
      getBrowserForDocument(aContentWindow.top.document);

    // ignore newly created documents that don't belong to a tab's browser
    if (!_browser) {
      return;
    }

    let nBox = gBrowser.getNotificationBox(_browser);
    let nBoxId = nBox.getAttribute("id");
    let hudId = "hud_" + nBoxId;
    let windowUI = nBox.ownerDocument.getElementById("console_window_" + hudId);
    if (windowUI) {
      // The Web Console popup is already open, no need to continue.
      if (aContentWindow == aContentWindow.top) {
        let hud = this.hudReferences[hudId];
        hud.reattachConsole(aContentWindow);
      }
      return;
    }

    if (!this.canActivateContext(hudId)) {
      return;
    }

    xulWindow.addEventListener("unload", this.onWindowUnload, false);
    gBrowser.tabContainer.addEventListener("TabClose", this.onTabClose, false);

    this.registerDisplay(hudId);

    let hudNode;
    let childNodes = nBox.childNodes;

    for (let i = 0; i < childNodes.length; i++) {
      let id = childNodes[i].getAttribute("id");
      // `id` is a string with the format "hud_<number>".
      if (id.split("_")[0] == "hud") {
        hudNode = childNodes[i];
        break;
      }
    }

    let hud;
    // If there is no HUD for this tab create a new one.
    if (!hudNode) {
      // get nBox object and call new HUD
      let config = { parentNode: nBox,
                     contentWindow: aContentWindow
                   };

      hud = new HeadsUpDisplay(config);

      HUDService.registerHUDReference(hud);
      let windowId = this.getWindowId(aContentWindow.top);
      this.windowIds[windowId] = hudId;

      hud.progressListener = new ConsoleProgressListener(hudId);

      _browser.webProgress.addProgressListener(hud.progressListener,
        Ci.nsIWebProgress.NOTIFY_STATE_ALL);
    }
    else {
      hud = this.hudReferences[hudId];
      if (aContentWindow == aContentWindow.top) {
        // TODO: name change?? doesn't actually re-attach the console
        hud.reattachConsole(aContentWindow);
      }
    }

    // Need to detect that the console component has been paved over.
    let consoleObject = unwrap(aContentWindow).console;
    if (!("__mozillaConsole__" in consoleObject))
      this.logWarningAboutReplacedAPI(hudId);

    // register the controller to handle "select all" properly
    this.createController(xulWindow);
  },

  /**
   * Adds the command controller to the XUL window if it's not already present.
   *
   * @param nsIDOMWindow aWindow
   *        The browser XUL window.
   * @returns void
   */
  createController: function HUD_createController(aWindow)
  {
    if (aWindow.webConsoleCommandController == null) {
      aWindow.webConsoleCommandController = new CommandController(aWindow);
      aWindow.controllers.insertControllerAt(0,
        aWindow.webConsoleCommandController);
    }
  },

  /**
   * Animates the Console appropriately.
   *
   * @param string aHUDId The ID of the console.
   * @param string aDirection Whether to animate the console appearing
   *        (ANIMATE_IN) or disappearing (ANIMATE_OUT).
   * @param function aCallback An optional callback, which will be called with
   *        the "transitionend" event passed as a parameter once the animation
   *        finishes.
   */
  animate: function HS_animate(aHUDId, aDirection, aCallback)
  {
    let hudBox = this.getHudReferenceById(aHUDId).HUDBox;
    if (!hudBox.classList.contains("animated")) {
      if (aCallback) {
        aCallback();
      }
      return;
    }

    switch (aDirection) {
      case ANIMATE_OUT:
        hudBox.style.height = 0;
        break;
      case ANIMATE_IN:
        this.resetHeight(aHUDId);
        break;
    }

    if (aCallback) {
      hudBox.addEventListener("transitionend", aCallback, false);
    }
  },

  /**
   * Disables all animation for a console, for unit testing. After this call,
   * the console will instantly take on a reasonable height, and the close
   * animation will not occur.
   *
   * @param string aHUDId The ID of the console.
   */
  disableAnimation: function HS_disableAnimation(aHUDId)
  {
    let hudBox = HUDService.hudReferences[aHUDId].HUDBox;
    if (hudBox.classList.contains("animated")) {
      hudBox.classList.remove("animated");
      this.resetHeight(aHUDId);
    }
  },

  /**
   * Reset the height of the Web Console.
   *
   * @param string aHUDId The ID of the Web Console.
   */
  resetHeight: function HS_resetHeight(aHUDId)
  {
    let HUD = this.hudReferences[aHUDId];
    let innerHeight = HUD.contentWindow.innerHeight;
    let chromeWindow = HUD.chromeDocument.defaultView;
    if (!HUD.consolePanel) {
      let splitterStyle = chromeWindow.getComputedStyle(HUD.splitter, null);
      innerHeight += parseInt(splitterStyle.height) +
                     parseInt(splitterStyle.borderTopWidth) +
                     parseInt(splitterStyle.borderBottomWidth);
    }

    let boxStyle = chromeWindow.getComputedStyle(HUD.HUDBox, null);
    innerHeight += parseInt(boxStyle.height) +
                   parseInt(boxStyle.borderTopWidth) +
                   parseInt(boxStyle.borderBottomWidth);

    let height = this.lastConsoleHeight > 0 ? this.lastConsoleHeight :
      Math.ceil(innerHeight * DEFAULT_CONSOLE_HEIGHT);

    if ((innerHeight - height) < MINIMUM_PAGE_HEIGHT) {
      height = innerHeight - MINIMUM_PAGE_HEIGHT;
    }
    else if (height < MINIMUM_CONSOLE_HEIGHT) {
      height = MINIMUM_CONSOLE_HEIGHT;
    }

    HUD.HUDBox.style.height = height + "px";
  },

  /**
   * Remember the height of the given Web Console, such that it can later be
   * reused when other Web Consoles are open.
   *
   * @param string aHUDId The ID of the Web Console.
   */
  storeHeight: function HS_storeHeight(aHUDId)
  {
    let hudBox = this.hudReferences[aHUDId].HUDBox;
    let window = hudBox.ownerDocument.defaultView;
    let style = window.getComputedStyle(hudBox, null);
    let height = parseInt(style.height);
    height += parseInt(style.borderTopWidth);
    height += parseInt(style.borderBottomWidth);
    this.lastConsoleHeight = height;

    let pref = Services.prefs.getIntPref("devtools.hud.height");
    if (pref > -1) {
      Services.prefs.setIntPref("devtools.hud.height", height);
    }
  },

  /**
   * Copies the selected items to the system clipboard.
   *
   * @param nsIDOMNode aOutputNode
   *        The output node.
   * @returns void
   */
  copySelectedItems: function HS_copySelectedItems(aOutputNode)
  {
    // Gather up the selected items and concatenate their clipboard text.

    let strings = [];
    let newGroup = false;

    let children = aOutputNode.children;

    for (let i = 0; i < children.length; i++) {
      let item = children[i];
      if (!item.selected) {
        continue;
      }

      // Add dashes between groups so that group boundaries show up in the
      // copied output.
      if (i > 0 && item.classList.contains("webconsole-new-group")) {
        newGroup = true;
      }

      // Ensure the selected item hasn't been filtered by type or string.
      if (!item.classList.contains("hud-filtered-by-type") &&
          !item.classList.contains("hud-filtered-by-string")) {
        let timestampString = ConsoleUtils.timestampString(item.timestamp);
        if (newGroup) {
          strings.push("--");
          newGroup = false;
        }
        strings.push("[" + timestampString + "] " + item.clipboardText);
      }
    }
    clipboardHelper.copyString(strings.join("\n"));
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplay
//////////////////////////////////////////////////////////////////////////

/*
 * HeadsUpDisplay is an interactive console initialized *per tab*  that
 * displays console log data as well as provides an interactive terminal to
 * manipulate the current tab's document content.
 * */
function HeadsUpDisplay(aConfig)
{
  // sample config: { parentNode: aDOMNode,
  //                  // or
  //                  parentNodeId: "myHUDParent123",
  //
  //                  placement: "appendChild"
  //                  // or
  //                  placement: "insertBefore",
  //                  placementChildNodeIndex: 0,
  //                }

  this.HUDBox = null;

  if (aConfig.parentNode) {
    // TODO: need to replace these DOM calls with internal functions
    // that operate on each application's node structure
    // better yet, we keep these functions in a "bridgeModule" or the HUDService
    // to keep a registry of nodeGetters for each application
    // see bug 568647
    this.parentNode = aConfig.parentNode;
    this.notificationBox = aConfig.parentNode;
    this.chromeDocument = aConfig.parentNode.ownerDocument;
    this.contentWindow = aConfig.contentWindow;
    this.uriSpec = aConfig.contentWindow.location.href;
    this.hudId = "hud_" + aConfig.parentNode.getAttribute("id");
  }
  else {
    // parentNodeId is the node's id where we attach the HUD
    // TODO: is the "navigator:browser" below used in all Gecko Apps?
    // see bug 568647
    let windowEnum = Services.wm.getEnumerator("navigator:browser");
    let parentNode;
    let contentDocument;
    let contentWindow;
    let chromeDocument;

    // TODO: the following  part is still very Firefox specific
    // see bug 568647

    while (windowEnum.hasMoreElements()) {
      let window = windowEnum.getNext();
      try {
        let gBrowser = window.gBrowser;
        let _browsers = gBrowser.browsers;
        let browserLen = _browsers.length;

        for (var i = 0; i < browserLen; i++) {
          var _notificationBox = gBrowser.getNotificationBox(_browsers[i]);
          this.notificationBox = _notificationBox;

          if (_notificationBox.getAttribute("id") == aConfig.parentNodeId) {
            this.parentNodeId = _notificationBox.getAttribute("id");
            this.hudId = "hud_" + this.parentNodeId;

            parentNode = _notificationBox;

            this.contentDocument =
              _notificationBox.childNodes[0].contentDocument;
            this.contentWindow =
              _notificationBox.childNodes[0].contentWindow;
            this.uriSpec = aConfig.contentWindow.location.href;

            this.chromeDocument =
              _notificationBox.ownerDocument;

            break;
          }
        }
      }
      catch (ex) {
        Cu.reportError(ex);
      }

      if (parentNode) {
        break;
      }
    }
    if (!parentNode) {
      throw new Error(this.ERRORS.PARENTNODE_NOT_FOUND);
    }
    this.parentNode = parentNode;
    this.notificationBox = parentNode;
  }

  // create textNode Factory:
  this.textFactory = NodeFactory("text", "xul", this.chromeDocument);

  this.chromeWindow = this.chromeDocument.defaultView;

  // create a panel dynamically and attach to the parentNode
  this.createHUD();

  this.HUDBox.lastTimestamp = 0;
  // create the JSTerm input element
  try {
    this.createConsoleInput(this.contentWindow, this.consoleWrap, this.outputNode);
    this.jsterm.inputNode.focus();
  }
  catch (ex) {
    Cu.reportError(ex);
  }

  // A cache for tracking repeated CSS Nodes.
  this.cssNodes = {};
}

HeadsUpDisplay.prototype = {

  consolePanel: null,

  get mainPopupSet()
  {
    return this.chromeDocument.getElementById("mainPopupSet");
  },

  /**
   * Get the tab associated to the HeadsUpDisplay object.
   */
  get tab()
  {
    // TODO: we should only keep a reference to the tab object and use
    // getters to determine the rest of objects we need - the chrome window,
    // document, etc. We should simplify the entire code to use only a single
    // tab object ref. See bug 656231.
    let tab = null;
    let id = this.notificationBox.id;
    Array.some(this.chromeDocument.defaultView.gBrowser.tabs, function(aTab) {
      if (aTab.linkedPanel == id) {
        tab = aTab;
        return true;
      }
    });

    return tab;
  },

  /**
   * Create a panel to open the web console if it should float above
   * the content in its own window.
   */
  createOwnWindowPanel: function HUD_createOwnWindowPanel()
  {
    if (this.uiInOwnWindow) {
      return this.consolePanel;
    }

    let width = 0;
    try {
      width = Services.prefs.getIntPref("devtools.webconsole.width");
    }
    catch (ex) {}

    if (width < 1) {
      width = this.HUDBox.clientWidth || this.contentWindow.innerWidth;
    }

    let height = this.HUDBox.clientHeight;

    let top = 0;
    try {
      top = Services.prefs.getIntPref("devtools.webconsole.top");
    }
    catch (ex) {}

    let left = 0;
    try {
      left = Services.prefs.getIntPref("devtools.webconsole.left");
    }
    catch (ex) {}

    let panel = this.chromeDocument.createElementNS(XUL_NS, "panel");

    let config = { id: "console_window_" + this.hudId,
                   label: this.getPanelTitle(),
                   titlebar: "normal",
                   noautohide: "true",
                   norestorefocus: "true",
                   close: "true",
                   flex: "1",
                   hudId: this.hudId,
                   width: width,
                   position: "overlap",
                   top: top,
                   left: left,
                 };

    for (let attr in config) {
      panel.setAttribute(attr, config[attr]);
    }

    panel.classList.add("web-console-panel");

    let onPopupShown = (function HUD_onPopupShown() {
      panel.removeEventListener("popupshown", onPopupShown, false);

      // Make sure that the HUDBox size updates when the panel is resized.

      let height = panel.clientHeight;

      this.HUDBox.style.height = "auto";
      this.HUDBox.setAttribute("flex", "1");

      panel.setAttribute("height", height);

      // Scroll the outputNode back to the last location.
      if (lastIndex > -1 && lastIndex < this.outputNode.getRowCount()) {
        this.outputNode.ensureIndexIsVisible(lastIndex);
      }

      if (this.jsterm) {
        this.jsterm.inputNode.focus();
      }
    }).bind(this);

    panel.addEventListener("popupshown", onPopupShown,false);

    let onPopupHidden = (function HUD_onPopupHidden(aEvent) {
      if (aEvent.target != panel) {
        return;
      }

      panel.removeEventListener("popuphidden", onPopupHidden, false);
      if (panel.parentNode) {
        panel.parentNode.removeChild(panel);
      }

      let width = 0;
      try {
        width = Services.prefs.getIntPref("devtools.webconsole.width");
      }
      catch (ex) { }

      if (width > 0) {
        Services.prefs.setIntPref("devtools.webconsole.width", panel.clientWidth);
      }

      /*
       * Removed because of bug 674562
       * Services.prefs.setIntPref("devtools.webconsole.top", panel.panelBox.y);
       * Services.prefs.setIntPref("devtools.webconsole.left", panel.panelBox.x);
       */

      // Make sure we are not going to close again, drop the hudId reference of
      // the panel.
      panel.removeAttribute("hudId");

      if (this.consoleWindowUnregisterOnHide) {
        HUDService.deactivateHUDForContext(this.tab, false);
      }
      else {
        this.consoleWindowUnregisterOnHide = true;
      }

      this.consolePanel = null;
    }).bind(this);

    panel.addEventListener("popuphidden", onPopupHidden, false);

    let lastIndex = -1;

    if (this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }
    panel.appendChild(this.HUDBox);

    let space = this.chromeDocument.createElement("spacer");
    space.setAttribute("flex", "1");

    let bottomBox = this.chromeDocument.createElement("hbox");
    space.setAttribute("flex", "1");

    let resizer = this.chromeDocument.createElement("resizer");
    resizer.setAttribute("dir", "bottomend");
    resizer.setAttribute("element", config.id);

    bottomBox.appendChild(space);
    bottomBox.appendChild(resizer);

    panel.appendChild(bottomBox);

    this.mainPopupSet.appendChild(panel);

    Services.prefs.setCharPref("devtools.webconsole.position", "window");

    panel.openPopup(null, "overlay", left, top, false, false);

    this.consolePanel = panel;
    this.consoleWindowUnregisterOnHide = true;

    return panel;
  },

  /**
   * Retrieve the Web Console panel title.
   *
   * @return string
   *         The Web Console panel title.
   */
  getPanelTitle: function HUD_getPanelTitle()
  {
    return this.getFormatStr("webConsoleWindowTitleAndURL", [this.uriSpec]);
  },

  positions: {
    above: 0, // the childNode index
    below: 2,
    window: null
  },

  consoleWindowUnregisterOnHide: true,

  /**
   * Re-position the console
   */
  positionConsole: function HUD_positionConsole(aPosition)
  {
    if (!(aPosition in this.positions)) {
      throw new Error("Incorrect argument: " + aPosition +
        ". Cannot position Web Console");
    }

    if (aPosition == "window") {
      let closeButton = this.consoleFilterToolbar.
        querySelector(".webconsole-close-button");
      closeButton.setAttribute("hidden", "true");
      this.createOwnWindowPanel();
      this.positionMenuitems.window.setAttribute("checked", true);
      if (this.positionMenuitems.last) {
        this.positionMenuitems.last.setAttribute("checked", false);
      }
      this.positionMenuitems.last = this.positionMenuitems[aPosition];
      this.uiInOwnWindow = true;
      return;
    }

    let height = this.HUDBox.clientHeight;

    // get the node position index
    let nodeIdx = this.positions[aPosition];
    let nBox = this.notificationBox;
    let node = nBox.childNodes[nodeIdx];

    // check to see if console is already positioned in aPosition
    if (node == this.HUDBox) {
      return;
    }

    let lastIndex = -1;

    if (this.outputNode.getIndexOfFirstVisibleRow) {
      lastIndex = this.outputNode.getIndexOfFirstVisibleRow() +
                  this.outputNode.getNumberOfVisibleRows() - 1;
    }

    // remove the console and splitter and reposition
    if (this.splitter.parentNode) {
      this.splitter.parentNode.removeChild(this.splitter);
    }

    if (aPosition == "below") {
      nBox.appendChild(this.splitter);
      nBox.appendChild(this.HUDBox);
    }
    else {
      nBox.insertBefore(this.splitter, node);
      nBox.insertBefore(this.HUDBox, this.splitter);
    }

    this.positionMenuitems[aPosition].setAttribute("checked", true);
    if (this.positionMenuitems.last) {
      this.positionMenuitems.last.setAttribute("checked", false);
    }
    this.positionMenuitems.last = this.positionMenuitems[aPosition];

    Services.prefs.setCharPref("devtools.webconsole.position", aPosition);

    if (lastIndex > -1 && lastIndex < this.outputNode.getRowCount()) {
      this.outputNode.ensureIndexIsVisible(lastIndex);
    }

    let closeButton = this.consoleFilterToolbar.
      getElementsByClassName("webconsole-close-button")[0];
    closeButton.removeAttribute("hidden");

    this.uiInOwnWindow = false;
    if (this.consolePanel) {
      this.HUDBox.removeAttribute("flex");
      this.HUDBox.removeAttribute("height");
      this.HUDBox.style.height = height + "px";

      // must destroy the consolePanel
      this.consoleWindowUnregisterOnHide = false;
      this.consolePanel.hidePopup();
    }

    if (this.jsterm) {
      this.jsterm.inputNode.focus();
    }
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @returns string
   */
  getStr: function HUD_getStr(aName)
  {
    return stringBundle.GetStringFromName(aName);
  },

  /**
   * L10N shortcut function
   *
   * @param string aName
   * @param array aArray
   * @returns string
   */
  getFormatStr: function HUD_getFormatStr(aName, aArray)
  {
    return stringBundle.formatStringFromName(aName, aArray, aArray.length);
  },

  /**
   * The JSTerm object that contains the console's inputNode
   *
   */
  jsterm: null,

  /**
   * creates and attaches the console input node
   *
   * @param nsIDOMWindow aWindow
   * @returns void
   */
  createConsoleInput:
  function HUD_createConsoleInput(aWindow, aParentNode, aExistingConsole)
  {
    var context = Cu.getWeakReference(aWindow);

    if (appName() == "FIREFOX") {
      let mixin = new JSTermFirefoxMixin(context, aParentNode,
                                         aExistingConsole);
      this.jsterm = new JSTerm(context, aParentNode, mixin, this.console);
    }
    else {
      throw new Error("Unsupported Gecko Application");
    }
  },

  /**
   * Re-attaches a console when the contentWindow is recreated
   *
   * @param nsIDOMWindow aContentWindow
   * @returns void
   */
  reattachConsole: function HUD_reattachConsole(aContentWindow)
  {
    this.contentWindow = aContentWindow;
    this.contentDocument = this.contentWindow.document;
    this.uriSpec = this.contentWindow.location.href;

    if (this.consolePanel) {
      this.consolePanel.label = this.getPanelTitle();
    }

    if (!this.jsterm) {
      this.createConsoleInput(this.contentWindow, this.consoleWrap, this.outputNode);
    }
    else {
      this.jsterm.context = Cu.getWeakReference(this.contentWindow);
      this.jsterm.console = this.console;
      this.jsterm.createSandbox();
    }
  },

  /**
   * Shortcut to make XUL nodes
   *
   * @param string aTag
   * @returns nsIDOMNode
   */
  makeXULNode:
  function HUD_makeXULNode(aTag)
  {
    return this.chromeDocument.createElement(aTag);
  },

  /**
   * Build the UI of each HeadsUpDisplay
   *
   * @returns nsIDOMNode
   */
  makeHUDNodes: function HUD_makeHUDNodes()
  {
    let self = this;

    this.splitter = this.makeXULNode("splitter");
    this.splitter.setAttribute("class", "hud-splitter");

    this.HUDBox = this.makeXULNode("vbox");
    this.HUDBox.setAttribute("id", this.hudId);
    this.HUDBox.setAttribute("class", "hud-box animated");
    this.HUDBox.style.height = 0;

    let outerWrap = this.makeXULNode("vbox");
    outerWrap.setAttribute("class", "hud-outer-wrapper");
    outerWrap.setAttribute("flex", "1");

    let consoleCommandSet = this.makeXULNode("commandset");
    outerWrap.appendChild(consoleCommandSet);

    let consoleWrap = this.makeXULNode("vbox");
    this.consoleWrap = consoleWrap;
    consoleWrap.setAttribute("class", "hud-console-wrapper");
    consoleWrap.setAttribute("flex", "1");

    this.outputNode = this.makeXULNode("richlistbox");
    this.outputNode.setAttribute("class", "hud-output-node");
    this.outputNode.setAttribute("flex", "1");
    this.outputNode.setAttribute("orient", "vertical");
    this.outputNode.setAttribute("context", this.hudId + "-output-contextmenu");
    this.outputNode.setAttribute("style", "direction: ltr;");
    this.outputNode.setAttribute("seltype", "multiple");

    this.filterSpacer = this.makeXULNode("spacer");
    this.filterSpacer.setAttribute("flex", "1");

    this.filterBox = this.makeXULNode("textbox");
    this.filterBox.setAttribute("class", "compact hud-filter-box");
    this.filterBox.setAttribute("hudId", this.hudId);
    this.filterBox.setAttribute("placeholder", this.getStr("stringFilter"));
    this.filterBox.setAttribute("type", "search");

    this.setFilterTextBoxEvents();

    this.createConsoleMenu(this.consoleWrap);

    this.filterPrefs = HUDService.getDefaultFilterPrefs(this.hudId);

    let consoleFilterToolbar = this.makeFilterToolbar();
    consoleFilterToolbar.setAttribute("id", "viewGroup");
    this.consoleFilterToolbar = consoleFilterToolbar;
    consoleWrap.appendChild(consoleFilterToolbar);

    consoleWrap.appendChild(this.outputNode);

    outerWrap.appendChild(consoleWrap);

    this.HUDBox.lastTimestamp = 0;

    this.jsTermParentNode = outerWrap;
    this.HUDBox.appendChild(outerWrap);

    return this.HUDBox;
  },

  /**
   * sets the click events for all binary toggle filter buttons
   *
   * @returns void
   */
  setFilterTextBoxEvents: function HUD_setFilterTextBoxEvents()
  {
    var filterBox = this.filterBox;
    function onChange()
    {
      // To improve responsiveness, we let the user finish typing before we
      // perform the search.

      if (this.timer == null) {
        let timerClass = Cc["@mozilla.org/timer;1"];
        this.timer = timerClass.createInstance(Ci.nsITimer);
      } else {
        this.timer.cancel();
      }

      let timerEvent = {
        notify: function setFilterTextBoxEvents_timerEvent_notify() {
          HUDService.updateFilterText(filterBox);
        }
      };

      this.timer.initWithCallback(timerEvent, SEARCH_DELAY,
        Ci.nsITimer.TYPE_ONE_SHOT);
    }

    filterBox.addEventListener("command", onChange, false);
    filterBox.addEventListener("input", onChange, false);
  },

  /**
   * Make the filter toolbar where we can toggle logging filters
   *
   * @returns nsIDOMNode
   */
  makeFilterToolbar: function HUD_makeFilterToolbar()
  {
    const BUTTONS = [
      {
        name: "PageNet",
        category: "net",
        severities: [
          { name: "ConsoleErrors", prefKey: "network" },
          { name: "ConsoleLog", prefKey: "networkinfo" }
        ]
      },
      {
        name: "PageCSS",
        category: "css",
        severities: [
          { name: "ConsoleErrors", prefKey: "csserror" },
          { name: "ConsoleWarnings", prefKey: "cssparser" }
        ]
      },
      {
        name: "PageJS",
        category: "js",
        severities: [
          { name: "ConsoleErrors", prefKey: "exception" },
          { name: "ConsoleWarnings", prefKey: "jswarn" }
        ]
      },
      {
        name: "PageWebDeveloper",
        category: "webdev",
        severities: [
          { name: "ConsoleErrors", prefKey: "error" },
          { name: "ConsoleWarnings", prefKey: "warn" },
          { name: "ConsoleInfo", prefKey: "info" },
          { name: "ConsoleLog", prefKey: "log" }
        ]
      }
    ];

    let toolbar = this.makeXULNode("toolbar");
    toolbar.setAttribute("class", "hud-console-filter-toolbar");
    toolbar.setAttribute("mode", "full");

    this.makeCloseButton(toolbar);

    for (let i = 0; i < BUTTONS.length; i++) {
      this.makeFilterButton(toolbar, BUTTONS[i]);
    }

    toolbar.appendChild(this.filterSpacer);

    let positionUI = this.createPositionUI();
    toolbar.appendChild(positionUI);

    toolbar.appendChild(this.filterBox);
    this.makeClearConsoleButton(toolbar);

    return toolbar;
  },

  /**
   * Creates the UI for re-positioning the console
   *
   * @return nsIDOMNode
   *         The toolbarbutton which holds the menu that allows the user to
   *         change the console position.
   */
  createPositionUI: function HUD_createPositionUI()
  {
    this._positionConsoleAbove = (function HUD_positionAbove() {
      this.positionConsole("above");
    }).bind(this);

    this._positionConsoleBelow = (function HUD_positionBelow() {
      this.positionConsole("below");
    }).bind(this);
    this._positionConsoleWindow = (function HUD_positionWindow() {
      this.positionConsole("window");
    }).bind(this);

    let button = this.makeXULNode("toolbarbutton");
    button.setAttribute("type", "menu");
    button.setAttribute("label", this.getStr("webConsolePosition"));
    button.setAttribute("tooltip", this.getStr("webConsolePositionTooltip"));

    let menuPopup = this.makeXULNode("menupopup");
    button.appendChild(menuPopup);

    let itemAbove = this.makeXULNode("menuitem");
    itemAbove.setAttribute("label", this.getStr("webConsolePositionAbove"));
    itemAbove.setAttribute("type", "checkbox");
    itemAbove.setAttribute("autocheck", "false");
    itemAbove.addEventListener("command", this._positionConsoleAbove, false);
    menuPopup.appendChild(itemAbove);

    let itemBelow = this.makeXULNode("menuitem");
    itemBelow.setAttribute("label", this.getStr("webConsolePositionBelow"));
    itemBelow.setAttribute("type", "checkbox");
    itemBelow.setAttribute("autocheck", "false");
    itemBelow.addEventListener("command", this._positionConsoleBelow, false);
    menuPopup.appendChild(itemBelow);

    let itemWindow = this.makeXULNode("menuitem");
    itemWindow.setAttribute("label", this.getStr("webConsolePositionWindow"));
    itemWindow.setAttribute("type", "checkbox");
    itemWindow.setAttribute("autocheck", "false");
    itemWindow.addEventListener("command", this._positionConsoleWindow, false);
    menuPopup.appendChild(itemWindow);

    this.positionMenuitems = {
      last: null,
      above: itemAbove,
      below: itemBelow,
      window: itemWindow,
    };

    return button;
  },

  /**
   * Creates the context menu on the console.
   *
   * @param nsIDOMNode aOutputNode
   *        The console output DOM node.
   * @returns void
   */
  createConsoleMenu: function HUD_createConsoleMenu(aConsoleWrapper) {
    let menuPopup = this.makeXULNode("menupopup");
    let id = this.hudId + "-output-contextmenu";
    menuPopup.setAttribute("id", id);
    menuPopup.addEventListener("popupshowing", function() {
      saveBodiesItem.setAttribute("checked",
        HUDService.saveRequestAndResponseBodies);
    }, true);

    let saveBodiesItem = this.makeXULNode("menuitem");
    saveBodiesItem.setAttribute("label", this.getStr("saveBodies.label"));
    saveBodiesItem.setAttribute("accesskey",
                                 this.getStr("saveBodies.accesskey"));
    saveBodiesItem.setAttribute("type", "checkbox");
    saveBodiesItem.setAttribute("buttonType", "saveBodies");
    saveBodiesItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(saveBodiesItem);

    menuPopup.appendChild(this.makeXULNode("menuseparator"));

    let copyItem = this.makeXULNode("menuitem");
    copyItem.setAttribute("label", this.getStr("copyCmd.label"));
    copyItem.setAttribute("accesskey", this.getStr("copyCmd.accesskey"));
    copyItem.setAttribute("key", "key_copy");
    copyItem.setAttribute("command", "cmd_copy");
    copyItem.setAttribute("buttonType", "copy");
    menuPopup.appendChild(copyItem);

    let selectAllItem = this.makeXULNode("menuitem");
    selectAllItem.setAttribute("label", this.getStr("selectAllCmd.label"));
    selectAllItem.setAttribute("accesskey",
                               this.getStr("selectAllCmd.accesskey"));
    selectAllItem.setAttribute("hudId", this.hudId);
    selectAllItem.setAttribute("buttonType", "selectAll");
    selectAllItem.setAttribute("oncommand", "HUDConsoleUI.command(this);");
    menuPopup.appendChild(selectAllItem);

    aConsoleWrapper.appendChild(menuPopup);
    aConsoleWrapper.setAttribute("context", id);
  },

  /**
   * Creates one of the filter buttons on the toolbar.
   *
   * @param nsIDOMNode aParent
   *        The node to which the filter button should be appended.
   * @param object aDescriptor
   *        A descriptor that contains info about the button. Contains "name",
   *        "category", and "prefKey" properties, and optionally a "severities"
   *        property.
   * @return void
   */
  makeFilterButton: function HUD_makeFilterButton(aParent, aDescriptor)
  {
    let toolbarButton = this.makeXULNode("toolbarbutton");
    aParent.appendChild(toolbarButton);

    let toggleFilter = HeadsUpDisplayUICommands.toggleFilter;
    toolbarButton.addEventListener("click", toggleFilter, false);

    let name = aDescriptor.name;
    toolbarButton.setAttribute("type", "menu-button");
    toolbarButton.setAttribute("label", this.getStr("btn" + name));
    toolbarButton.setAttribute("tooltip", this.getStr("tip" + name));
    toolbarButton.setAttribute("category", aDescriptor.category);
    toolbarButton.setAttribute("hudId", this.hudId);
    toolbarButton.classList.add("webconsole-filter-button");

    let menuPopup = this.makeXULNode("menupopup");
    toolbarButton.appendChild(menuPopup);

    let allChecked = true;
    for (let i = 0; i < aDescriptor.severities.length; i++) {
      let severity = aDescriptor.severities[i];
      let menuItem = this.makeXULNode("menuitem");
      menuItem.setAttribute("label", this.getStr("btn" + severity.name));
      menuItem.setAttribute("type", "checkbox");
      menuItem.setAttribute("autocheck", "false");
      menuItem.setAttribute("hudId", this.hudId);

      let prefKey = severity.prefKey;
      menuItem.setAttribute("prefKey", prefKey);

      let checked = this.filterPrefs[prefKey];
      menuItem.setAttribute("checked", checked);
      if (!checked) {
        allChecked = false;
      }

      menuItem.addEventListener("command", toggleFilter, false);

      menuPopup.appendChild(menuItem);
    }

    toolbarButton.setAttribute("checked", allChecked);
  },

  /**
   * Creates the close button on the toolbar.
   *
   * @param nsIDOMNode aParent
   *        The toolbar to attach the close button to.
   * @return void
   */
  makeCloseButton: function HUD_makeCloseButton(aToolbar)
  {
    this.closeButtonOnCommand = (function HUD_closeButton_onCommand() {
      HUDService.animate(this.hudId, ANIMATE_OUT, (function() {
        HUDService.deactivateHUDForContext(this.tab, true);
      }).bind(this));
    }).bind(this);

    this.closeButton = this.makeXULNode("toolbarbutton");
    this.closeButton.classList.add("webconsole-close-button");
    this.closeButton.addEventListener("command",
      this.closeButtonOnCommand, false);
    aToolbar.appendChild(this.closeButton);
  },

  /**
   * Creates the "Clear Console" button.
   *
   * @param nsIDOMNode aParent
   *        The toolbar to attach the "Clear Console" button to.
   * @param string aHUDId
   *        The ID of the console.
   * @return void
   */
  makeClearConsoleButton: function HUD_makeClearConsoleButton(aToolbar)
  {
    let hudId = this.hudId;
    function HUD_clearButton_onCommand() {
      HUDService.getHudReferenceById(hudId).jsterm.clearOutput();
    }

    let clearButton = this.makeXULNode("toolbarbutton");
    clearButton.setAttribute("label", this.getStr("btnClear"));
    clearButton.classList.add("webconsole-clear-console-button");
    clearButton.addEventListener("command", HUD_clearButton_onCommand, false);

    aToolbar.appendChild(clearButton);
  },

  /**
   * Destroy the property inspector message node. This performs the necessary
   * cleanup for the tree widget and removes it from the DOM.
   *
   * @param nsIDOMNode aMessageNode
   *        The message node that contains the property inspector from a
   *        console.dir call.
   */
  pruneConsoleDirNode: function HUD_pruneConsoleDirNode(aMessageNode)
  {
    aMessageNode.parentNode.removeChild(aMessageNode);
    let tree = aMessageNode.querySelector("tree");
    tree.parentNode.removeChild(tree);
    aMessageNode.propertyTreeView = null;
    tree.view = null;
    tree = null;
  },

  /**
   * Create the Web Console UI.
   *
   * @return nsIDOMNode
   *         The Web Console container element (HUDBox).
   */
  createHUD: function HUD_createHUD()
  {
    if (!this.HUDBox) {
      this.makeHUDNodes();
      let positionPref = Services.prefs.getCharPref("devtools.webconsole.position");
      this.positionConsole(positionPref);
    }
    return this.HUDBox;
  },

  uiInOwnWindow: false,

  get console() { return this.contentWindow.wrappedJSObject.console; },

  getLogCount: function HUD_getLogCount()
  {
    return this.outputNode.childNodes.length;
  },

  getLogNodes: function HUD_getLogNodes()
  {
    return this.outputNode.childNodes;
  },

  ERRORS: {
    HUD_BOX_DOES_NOT_EXIST: "Heads Up Display does not exist",
    TAB_ID_REQUIRED: "Tab DOM ID is required",
    PARENTNODE_NOT_FOUND: "parentNode element not found"
  },

  /**
   * Destroy the HUD object. Call this method to avoid memory leaks when the Web
   * Console is closed.
   */
  destroy: function HUD_destroy()
  {
    this.jsterm.destroy();

    this.positionMenuitems.above.removeEventListener("command",
      this._positionConsoleAbove, false);
    this.positionMenuitems.below.removeEventListener("command",
      this._positionConsoleBelow, false);
    this.positionMenuitems.window.removeEventListener("command",
      this._positionConsoleWindow, false);

    this.closeButton.removeEventListener("command",
      this.closeButtonOnCommand, false);
  },
};


//////////////////////////////////////////////////////////////////////////////
// ConsoleAPIObserver
//////////////////////////////////////////////////////////////////////////////

let ConsoleAPIObserver = {

  QueryInterface: XPCOMUtils.generateQI([Ci.nsIObserver]),

  init: function CAO_init()
  {
    Services.obs.addObserver(this, "quit-application-granted", false);
    Services.obs.addObserver(this, "console-api-log-event", false);
  },

  observe: function CAO_observe(aMessage, aTopic, aData)
  {
    if (aTopic == "console-api-log-event") {
      aMessage = aMessage.wrappedJSObject;
      let windowId = parseInt(aData);
      let win = HUDService.getWindowByWindowId(windowId);
      if (!win)
        return;

      // Find the HUD ID for the topmost window
      let hudId = HUDService.getHudIdByWindow(win.top);
      if (!hudId)
        return;

      HUDService.logConsoleAPIMessage(hudId, aMessage);
    }
    else if (aTopic == "quit-application-granted") {
      HUDService.shutdown();
    }
  },

  shutdown: function CAO_shutdown()
  {
    Services.obs.removeObserver(this, "quit-application-granted");
    Services.obs.removeObserver(this, "console-api-log-event");
  }
};

/**
 * Creates a DOM Node factory for XUL nodes - as well as textNodes
 * @param aFactoryType "xul" or "text"
 * @param ignored This parameter is currently ignored, and will be removed
 * See bug 594304
 * @param aDocument The document, the factory is to generate nodes from
 * @return DOM Node Factory function
 */
function NodeFactory(aFactoryType, ignored, aDocument)
{
  // aDocument is presumed to be a XULDocument
  if (aFactoryType == "text") {
    return function factory(aText)
    {
      return aDocument.createTextNode(aText);
    }
  }
  else if (aFactoryType == "xul") {
    return function factory(aTag)
    {
      return aDocument.createElement(aTag);
    }
  }
  else {
    throw new Error('NodeFactory: Unknown factory type: ' + aFactoryType);
  }
}

//////////////////////////////////////////////////////////////////////////
// JS Completer
//////////////////////////////////////////////////////////////////////////

const STATE_NORMAL = 0;
const STATE_QUOTE = 2;
const STATE_DQUOTE = 3;

const OPEN_BODY = '{[('.split('');
const CLOSE_BODY = '}])'.split('');
const OPEN_CLOSE_BODY = {
  '{': '}',
  '[': ']',
  '(': ')'
};

/**
 * Analyses a given string to find the last statement that is interesting for
 * later completion.
 *
 * @param   string aStr
 *          A string to analyse.
 *
 * @returns object
 *          If there was an error in the string detected, then a object like
 *
 *            { err: "ErrorMesssage" }
 *
 *          is returned, otherwise a object like
 *
 *            {
 *              state: STATE_NORMAL|STATE_QUOTE|STATE_DQUOTE,
 *              startPos: index of where the last statement begins
 *            }
 */
function findCompletionBeginning(aStr)
{
  let bodyStack = [];

  let state = STATE_NORMAL;
  let start = 0;
  let c;
  for (let i = 0; i < aStr.length; i++) {
    c = aStr[i];

    switch (state) {
      // Normal JS state.
      case STATE_NORMAL:
        if (c == '"') {
          state = STATE_DQUOTE;
        }
        else if (c == '\'') {
          state = STATE_QUOTE;
        }
        else if (c == ';') {
          start = i + 1;
        }
        else if (c == ' ') {
          start = i + 1;
        }
        else if (OPEN_BODY.indexOf(c) != -1) {
          bodyStack.push({
            token: c,
            start: start
          });
          start = i + 1;
        }
        else if (CLOSE_BODY.indexOf(c) != -1) {
          var last = bodyStack.pop();
          if (!last || OPEN_CLOSE_BODY[last.token] != c) {
            return {
              err: "syntax error"
            };
          }
          if (c == '}') {
            start = i + 1;
          }
          else {
            start = last.start;
          }
        }
        break;

      // Double quote state > " <
      case STATE_DQUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
        }
        else if (c == '"') {
          state = STATE_NORMAL;
        }
        break;

      // Single quoate state > ' <
      case STATE_QUOTE:
        if (c == '\\') {
          i ++;
        }
        else if (c == '\n') {
          return {
            err: "unterminated string literal"
          };
          return;
        }
        else if (c == '\'') {
          state = STATE_NORMAL;
        }
        break;
    }
  }

  return {
    state: state,
    startPos: start
  };
}

/**
 * Provides a list of properties, that are possible matches based on the passed
 * scope and inputValue.
 *
 * @param object aScope
 *        Scope to use for the completion.
 *
 * @param string aInputValue
 *        Value that should be completed.
 *
 * @returns null or object
 *          If no completion valued could be computed, null is returned,
 *          otherwise a object with the following form is returned:
 *            {
 *              matches: [ string, string, string ],
 *              matchProp: Last part of the inputValue that was used to find
 *                         the matches-strings.
 *            }
 */
function JSPropertyProvider(aScope, aInputValue)
{
  let obj = unwrap(aScope);

  // Analyse the aInputValue and find the beginning of the last part that
  // should be completed.
  let beginning = findCompletionBeginning(aInputValue);

  // There was an error analysing the string.
  if (beginning.err) {
    return null;
  }

  // If the current state is not STATE_NORMAL, then we are inside of an string
  // which means that no completion is possible.
  if (beginning.state != STATE_NORMAL) {
    return null;
  }

  let completionPart = aInputValue.substring(beginning.startPos);

  // Don't complete on just an empty string.
  if (completionPart.trim() == "") {
    return null;
  }

  let properties = completionPart.split('.');
  let matchProp;
  if (properties.length > 1) {
    matchProp = properties.pop().trimLeft();
    for (let i = 0; i < properties.length; i++) {
      let prop = properties[i].trim();

      // If obj is undefined or null, then there is no chance to run completion
      // on it. Exit here.
      if (typeof obj === "undefined" || obj === null) {
        return null;
      }

      // Check if prop is a getter function on obj. Functions can change other
      // stuff so we can't execute them to get the next object. Stop here.
      if (isNonNativeGetter(obj, prop)) {
        return null;
      }
      try {
        obj = obj[prop];
      }
      catch (ex) {
        return null;
      }
    }
  }
  else {
    matchProp = properties[0].trimLeft();
  }

  // If obj is undefined or null, then there is no chance to run
  // completion on it. Exit here.
  if (typeof obj === "undefined" || obj === null) {
    return null;
  }

  // Skip Iterators and Generators.
  if (isIteratorOrGenerator(obj)) {
    return null;
  }

  let matches = [];
  for (let prop in obj) {
    if (prop.indexOf(matchProp) == 0) {
      matches.push(prop);
    }
  }

  return {
    matchProp: matchProp,
    matches: matches.sort(),
  };
}

function isIteratorOrGenerator(aObject)
{
  if (aObject === null) {
    return false;
  }

  if (typeof aObject == "object") {
    if (typeof aObject.__iterator__ == "function" ||
        aObject.constructor && aObject.constructor.name == "Iterator") {
      return true;
    }

    try {
      let str = aObject.toString();
      if (typeof aObject.next == "function" &&
          str.indexOf("[object Generator") == 0) {
        return true;
      }
    }
    catch (ex) {
      // window.history.next throws in the typeof check above.
      return false;
    }
  }

  return false;
}

//////////////////////////////////////////////////////////////////////////
// JSTerm
//////////////////////////////////////////////////////////////////////////

/**
 * JSTermHelper
 *
 * Defines a set of functions ("helper functions") that are available from the
 * WebConsole but not from the webpage.
 * A list of helper functions used by Firebug can be found here:
 *   http://getfirebug.com/wiki/index.php/Command_Line_API
 */
function JSTermHelper(aJSTerm)
{
  /**
   * Returns the result of document.getElementById(aId).
   *
   * @param string aId
   *        A string that is passed to window.document.getElementById.
   * @returns nsIDOMNode or null
   */
  aJSTerm.sandbox.$ = function JSTH_$(aId)
  {
    try {
      return aJSTerm._window.document.getElementById(aId);
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Returns the result of document.querySelectorAll(aSelector).
   *
   * @param string aSelector
   *        A string that is passed to window.document.querySelectorAll.
   * @returns array of nsIDOMNode
   */
  aJSTerm.sandbox.$$ = function JSTH_$$(aSelector)
  {
    try {
      return aJSTerm._window.document.querySelectorAll(aSelector);
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Runs a xPath query and returns all matched nodes.
   *
   * @param string aXPath
   *        xPath search query to execute.
   * @param [optional] nsIDOMNode aContext
   *        Context to run the xPath query on. Uses window.document if not set.
   * @returns array of nsIDOMNode
   */
  aJSTerm.sandbox.$x = function JSTH_$x(aXPath, aContext)
  {
    let nodes = [];
    let doc = aJSTerm._window.document;
    let aContext = aContext || doc;

    try {
      let results = doc.evaluate(aXPath, aContext, null,
                                  Ci.nsIDOMXPathResult.ANY_TYPE, null);

      let node;
      while (node = results.iterateNext()) {
        nodes.push(node);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }

    return nodes;
  };

  /**
   * Returns the currently selected object in the highlighter.
   *
   * @returns nsIDOMNode or null
   */
  Object.defineProperty(aJSTerm.sandbox, "$0", {
    get: function() {
      let mw = HUDService.currentContext();
      try {
        return mw.InspectorUI.selection;
      }
      catch (ex) {
        aJSTerm.console.error(ex.message);
      }
    },
    enumerable: true,
    configurable: false
  });

  /**
   * Clears the output of the JSTerm.
   */
  aJSTerm.sandbox.clear = function JSTH_clear()
  {
    aJSTerm.helperEvaluated = true;
    aJSTerm.clearOutput();
  };

  /**
   * Returns the result of Object.keys(aObject).
   *
   * @param object aObject
   *        Object to return the property names from.
   * @returns array of string
   */
  aJSTerm.sandbox.keys = function JSTH_keys(aObject)
  {
    try {
      return Object.keys(unwrap(aObject));
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
  };

  /**
   * Returns the values of all properties on aObject.
   *
   * @param object aObject
   *        Object to display the values from.
   * @returns array of string
   */
  aJSTerm.sandbox.values = function JSTH_values(aObject)
  {
    let arrValues = [];
    let obj = unwrap(aObject);

    try {
      for (let prop in obj) {
        arrValues.push(obj[prop]);
      }
    }
    catch (ex) {
      aJSTerm.console.error(ex.message);
    }
    return arrValues;
  };

  /**
   * Opens a help window in MDC
   */
  aJSTerm.sandbox.help = function JSTH_help()
  {
    aJSTerm.helperEvaluated = true;
    aJSTerm._window.open(
        "https://developer.mozilla.org/AppLinks/WebConsoleHelp?locale=" +
        aJSTerm._window.navigator.language, "help", "");
  };

  /**
   * Inspects the passed aObject. This is done by opening the PropertyPanel.
   *
   * @param object aObject
   *        Object to inspect.
   * @returns void
   */
  aJSTerm.sandbox.inspect = function JSTH_inspect(aObject)
  {
    aJSTerm.helperEvaluated = true;
    let propPanel = aJSTerm.openPropertyPanel(null, unwrap(aObject));
    propPanel.panel.setAttribute("hudId", aJSTerm.hudId);
  };

  /**
   * Prints aObject to the output.
   *
   * @param object aObject
   *        Object to print to the output.
   * @returns void
   */
  aJSTerm.sandbox.pprint = function JSTH_pprint(aObject)
  {
    aJSTerm.helperEvaluated = true;
    if (aObject === null || aObject === undefined || aObject === true || aObject === false) {
      aJSTerm.console.error(HUDService.getStr("helperFuncUnsupportedTypeError"));
      return;
    }
    else if (typeof aObject === TYPEOF_FUNCTION) {
      aJSTerm.writeOutput(aObject + "\n", CATEGORY_OUTPUT, SEVERITY_LOG);
      return;
    }

    let output = [];
    let pairs = namesAndValuesOf(unwrap(aObject));

    pairs.forEach(function(pair) {
      output.push("  " + pair.display);
    });

    aJSTerm.writeOutput(output.join("\n"), CATEGORY_OUTPUT, SEVERITY_LOG);
  };

  /**
   * Print a string to the output, as-is.
   *
   * @param string aString
   *        A string you want to output.
   * @returns void
   */
  aJSTerm.sandbox.print = function JSTH_print(aString)
  {
    aJSTerm.helperEvaluated = true;
    aJSTerm.writeOutput("" + aString, CATEGORY_OUTPUT, SEVERITY_LOG);
  };
}

/**
 * JSTerm
 *
 * JavaScript Terminal: creates input nodes for console code interpretation
 * and 'JS Workspaces'
 */

/**
 * Create a JSTerminal or attach a JSTerm input node to an existing output node,
 * given by the parent node.
 *
 * @param object aContext
 *        Usually nsIDOMWindow, but doesn't have to be
 * @param nsIDOMNode aParentNode where to attach the JSTerm
 * @param object aMixin
 *        Gecko-app (or Jetpack) specific utility object
 * @param object aConsole
 *        Console object to use within the JSTerm.
 */
function JSTerm(aContext, aParentNode, aMixin, aConsole)
{
  // set the context, attach the UI by appending to aParentNode

  this.application = appName();
  this.context = aContext;
  this.parentNode = aParentNode;
  this.mixins = aMixin;
  this.console = aConsole;

  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  let node = aParentNode;
  while (!node.hasAttribute("id")) {
    node = node.parentNode;
  }
  this.hudId = node.getAttribute("id");

  this.historyIndex = 0;
  this.historyPlaceHolder = 0;  // this.history.length;
  this.log = LogFactory("*** JSTerm:");
  this.autocompletePopup = new AutocompletePopup(aParentNode.ownerDocument);
  this.autocompletePopup.onSelect = this.onAutocompleteSelect.bind(this);
  this.autocompletePopup.onClick = this.acceptProposedCompletion.bind(this);
  this.init();
}

JSTerm.prototype = {

  propertyProvider: JSPropertyProvider,

  COMPLETE_FORWARD: 0,
  COMPLETE_BACKWARD: 1,
  COMPLETE_HINT_ONLY: 2,

  init: function JST_init()
  {
    this.createSandbox();

    this.inputNode = this.mixins.inputNode;
    this.outputNode = this.mixins.outputNode;
    this.completeNode = this.mixins.completeNode;

    this._keyPress = this.keyPress.bind(this);
    this._inputEventHandler = this.inputEventHandler.bind(this);

    this.inputNode.addEventListener("keypress",
      this._keyPress, false);
    this.inputNode.addEventListener("input",
      this._inputEventHandler, false);
    this.inputNode.addEventListener("keyup",
      this._inputEventHandler, false);
  },

  get codeInputString()
  {
    return this.inputNode.value;
  },

  generateUI: function JST_generateUI()
  {
    this.mixins.generateUI();
  },

  attachUI: function JST_attachUI()
  {
    this.mixins.attachUI();
  },

  createSandbox: function JST_setupSandbox()
  {
    // create a JS Sandbox out of this.context
    this.sandbox = new Cu.Sandbox(this._window,
      { sandboxPrototype: this._window, wantXrays: false });
    this.sandbox.console = this.console;
    JSTermHelper(this);
  },

  get _window()
  {
    return this.context.get().QueryInterface(Ci.nsIDOMWindow);
  },

  /**
   * Evaluates a string in the sandbox.
   *
   * @param string aString
   *        String to evaluate in the sandbox.
   * @returns something
   *          The result of the evaluation.
   */
  evalInSandbox: function JST_evalInSandbox(aString)
  {
    // The help function needs to be easy to guess, so we make the () optional
    if (aString.trim() === "help" || aString.trim() === "?") {
      aString = "help()";
    }

    let window = unwrap(this.sandbox.window);
    let $ = null, $$ = null;

    // We prefer to execute the page-provided implementations for the $() and
    // $$() functions.
    if (typeof window.$ == "function") {
      $ = this.sandbox.$;
      delete this.sandbox.$;
    }
    if (typeof window.$$ == "function") {
      $$ = this.sandbox.$$;
      delete this.sandbox.$$;
    }

    let result = Cu.evalInSandbox(aString, this.sandbox, "1.8", "Web Console", 1);

    if ($) {
      this.sandbox.$ = $;
    }
    if ($$) {
      this.sandbox.$$ = $$;
    }

    return result;
  },


  execute: function JST_execute(aExecuteString)
  {
    // attempt to execute the content of the inputNode
    aExecuteString = aExecuteString || this.inputNode.value;
    if (!aExecuteString) {
      this.writeOutput("no value to execute", CATEGORY_OUTPUT, SEVERITY_LOG);
      return;
    }

    this.writeOutput(aExecuteString, CATEGORY_INPUT, SEVERITY_LOG);

    try {
      this.helperEvaluated = false;
      let result = this.evalInSandbox(aExecuteString);

      // Hide undefined results coming from helpers.
      let shouldShow = !(result === undefined && this.helperEvaluated);
      if (shouldShow) {
        let inspectable = this.isResultInspectable(result);
        let resultString = this.formatResult(result);

        if (inspectable) {
          this.writeOutputJS(aExecuteString, result, resultString);
        }
        else {
          this.writeOutput(resultString, CATEGORY_OUTPUT, SEVERITY_LOG);
        }
      }
    }
    catch (ex) {
      this.writeOutput("" + ex, CATEGORY_OUTPUT, SEVERITY_ERROR);
    }

    this.history.push(aExecuteString);
    this.historyIndex++;
    this.historyPlaceHolder = this.history.length;
    this.setInputValue("");
    this.clearCompletion();
  },

  /**
   * Opens a new PropertyPanel. The panel has two buttons: "Update" reexecutes
   * the passed aEvalString and places the result inside of the tree. The other
   * button closes the panel.
   *
   * @param string aEvalString
   *        String that was used to eval the aOutputObject. Used as title
   *        and to update the tree content.
   * @param object aOutputObject
   *        Object to display/inspect inside of the tree.
   * @param nsIDOMNode aAnchor
   *        A node to popup the panel next to (using "after_pointer").
   * @returns object the created and opened propertyPanel.
   */
  openPropertyPanel: function JST_openPropertyPanel(aEvalString, aOutputObject,
                                                    aAnchor)
  {
    let self = this;
    let propPanel;
    // The property panel has one button:
    //    `Update`: reexecutes the string executed on the command line. The
    //    result will be inspected by this panel.
    let buttons = [];

    // If there is a evalString passed to this function, then add a `Update`
    // button to the panel so that the evalString can be reexecuted to update
    // the content of the panel.
    if (aEvalString !== null) {
      buttons.push({
        label: HUDService.getStr("update.button"),
        accesskey: HUDService.getStr("update.accesskey"),
        oncommand: function () {
          try {
            var result = self.evalInSandbox(aEvalString);

            if (result !== undefined) {
              // TODO: This updates the value of the tree.
              // However, the states of opened nodes is not saved.
              // See bug 586246.
              propPanel.treeView.data = result;
            }
          }
          catch (ex) {
            self.console.error(ex);
          }
        }
      });
    }

    let doc = self.parentNode.ownerDocument;
    let parent = doc.getElementById("mainPopupSet");
    let title = (aEvalString
        ? HUDService.getFormatStr("jsPropertyInspectTitle", [aEvalString])
        : HUDService.getStr("jsPropertyTitle"));

    propPanel = new PropertyPanel(parent, doc, title, aOutputObject, buttons);
    propPanel.linkNode = aAnchor;

    let panel = propPanel.panel;
    panel.openPopup(aAnchor, "after_pointer", 0, 0, false, false);
    panel.sizeTo(350, 450);
    return propPanel;
  },

  /**
   * Writes a JS object to the JSTerm outputNode. If the user clicks on the
   * written object, openPropertyPanel is called to open up a panel to inspect
   * the object.
   *
   * @param string aEvalString
   *        String that was evaluated to get the aOutputObject.
   * @param object aResultObject
   *        The evaluation result object.
   * @param object aOutputString
   *        The output string to be written to the outputNode.
   */
  writeOutputJS: function JST_writeOutputJS(aEvalString, aOutputObject, aOutputString)
  {
    let node = ConsoleUtils.createMessageNode(this.parentNode.ownerDocument,
                                              CATEGORY_OUTPUT,
                                              SEVERITY_LOG,
                                              aOutputString);

    let linkNode = node.querySelector(".webconsole-msg-body");

    linkNode.classList.add("hud-clickable");
    linkNode.setAttribute("aria-haspopup", "true");

    // Make the object bring up the property panel.
    node.addEventListener("mousedown", function(aEvent) {
      this._startX = aEvent.clientX;
      this._startY = aEvent.clientY;
    }, false);

    let self = this;
    node.addEventListener("click", function(aEvent) {
      if (aEvent.detail != 1 || aEvent.button != 0 ||
          (this._startX != aEvent.clientX &&
           this._startY != aEvent.clientY)) {
        return;
      }

      if (!this._panelOpen) {
        let propPanel = self.openPropertyPanel(aEvalString, aOutputObject, this);
        propPanel.panel.setAttribute("hudId", self.hudId);
        this._panelOpen = true;
      }
    }, false);

    ConsoleUtils.outputMessageNode(node, this.hudId);
  },

  /**
   * Writes a message to the HUD that originates from the interactive
   * JavaScript console.
   *
   * @param string aOutputMessage
   *        The message to display.
   * @param number aCategory
   *        The category of message: one of the CATEGORY_ constants.
   * @param number aSeverity
   *        The severity of message: one of the SEVERITY_ constants.
   * @returns void
   */
  writeOutput: function JST_writeOutput(aOutputMessage, aCategory, aSeverity)
  {
    let node = ConsoleUtils.createMessageNode(this.parentNode.ownerDocument,
                                              aCategory, aSeverity,
                                              aOutputMessage);

    ConsoleUtils.outputMessageNode(node, this.hudId);
  },

  /**
   * Format the jsterm execution result based on its type.
   *
   * @param mixed aResult
   *        The evaluation result object you want displayed.
   * @returns string
   *          The string that can be displayed.
   */
  formatResult: function JST_formatResult(aResult)
  {
    let output = "";
    let type = this.getResultType(aResult);

    switch (type) {
      case "string":
        output = this.formatString(aResult);
        break;
      case "boolean":
      case "date":
      case "error":
      case "number":
      case "regexp":
        output = aResult.toString();
        break;
      case "null":
      case "undefined":
        output = type;
        break;
      default:
        if (aResult.toSource) {
          try {
            output = aResult.toSource();
          } catch (ex) { }
        }
        if (!output || output == "({})") {
          output = aResult.toString();
        }
        break;
    }

    return output;
  },

  /**
   * Format a string for output.
   *
   * @param string aString
   *        The string you want to display.
   * @returns string
   *          The string that can be displayed.
   */
  formatString: function JST_formatString(aString)
  {
    function isControlCode(c) {
      // See http://en.wikipedia.org/wiki/C0_and_C1_control_codes
      // C0 is 0x00-0x1F, C1 is 0x80-0x9F (inclusive).
      // We also include DEL (U+007F) and NBSP (U+00A0), which are not strictly
      // in C1 but border it.
      return (c <= 0x1F) || (0x7F <= c && c <= 0xA0);
    }

    function replaceFn(aMatch, aType, aHex) {
      // Leave control codes escaped, but unescape the rest of the characters.
      let c = parseInt(aHex, 16);
      return isControlCode(c) ? aMatch : String.fromCharCode(c);
    }

    let output = uneval(aString).replace(/\\(x)([0-9a-fA-F]{2})/g, replaceFn)
                 .replace(/\\(u)([0-9a-fA-F]{4})/g, replaceFn);

    return output;
  },

  /**
   * Determine if the jsterm execution result is inspectable or not.
   *
   * @param mixed aResult
   *        The evaluation result object you want to check if it is inspectable.
   * @returns boolean
   *          True if the object is inspectable or false otherwise.
   */
  isResultInspectable: function JST_isResultInspectable(aResult)
  {
    let isEnumerable = false;

    // Skip Iterators and Generators.
    if (isIteratorOrGenerator(aResult)) {
      return false;
    }

    for (let p in aResult) {
      isEnumerable = true;
      break;
    }

    return isEnumerable && typeof(aResult) != "string";
  },

  /**
   * Determine the type of the jsterm execution result.
   *
   * @param mixed aResult
   *        The evaluation result object you want to check.
   * @returns string
   *          Constructor name or type: string, number, boolean, regexp, date,
   *          function, object, null, undefined...
   */
  getResultType: function JST_getResultType(aResult)
  {
    let type = aResult === null ? "null" : typeof aResult;
    if (type == "object" && aResult.constructor && aResult.constructor.name) {
      type = aResult.constructor.name;
    }

    return type.toLowerCase();
  },

  clearOutput: function JST_clearOutput()
  {
    let hud = HUDService.getHudReferenceById(this.hudId);
    hud.cssNodes = {};

    let node = hud.outputNode;
    while (node.firstChild) {
      if (node.firstChild.classList &&
          node.firstChild.classList.contains("webconsole-msg-inspector")) {
        hud.pruneConsoleDirNode(node.firstChild);
      }
      else {
        hud.outputNode.removeChild(node.firstChild);
      }
    }

    hud.HUDBox.lastTimestamp = 0;
  },

  /**
   * Updates the size of the input field (command line) to fit its contents.
   *
   * @returns void
   */
  resizeInput: function JST_resizeInput()
  {
    let inputNode = this.inputNode;

    // Reset the height so that scrollHeight will reflect the natural height of
    // the contents of the input field.
    inputNode.style.height = "auto";

    // Now resize the input field to fit its contents.
    let scrollHeight = inputNode.inputField.scrollHeight;
    if (scrollHeight > 0) {
      inputNode.style.height = scrollHeight + "px";
    }
  },

  /**
   * Sets the value of the input field (command line), and resizes the field to
   * fit its contents. This method is preferred over setting "inputNode.value"
   * directly, because it correctly resizes the field.
   *
   * @param string aNewValue
   *        The new value to set.
   * @returns void
   */
  setInputValue: function JST_setInputValue(aNewValue)
  {
    this.inputNode.value = aNewValue;
    this.lastInputValue = aNewValue;
    this.completeNode.value = "";
    this.resizeInput();
  },

  /**
   * The inputNode "input" and "keyup" event handler.
   *
   * @param nsIDOMEvent aEvent
   */
  inputEventHandler: function JSTF_inputEventHandler(aEvent)
  {
    if (this.lastInputValue != this.inputNode.value) {
      this.resizeInput();
      this.complete(this.COMPLETE_HINT_ONLY);
      this.lastInputValue = this.inputNode.value;
    }
  },

  /**
   * The inputNode "keypress" event handler.
   *
   * @param nsIDOMEvent aEvent
   */
  keyPress: function JSTF_keyPress(aEvent)
  {
    if (aEvent.ctrlKey) {
      switch (aEvent.charCode) {
        case 97:
          // control-a
          this.inputNode.setSelectionRange(0, 0);
          aEvent.preventDefault();
          break;
        case 101:
          // control-e
          this.inputNode.setSelectionRange(this.inputNode.value.length,
                                           this.inputNode.value.length);
          aEvent.preventDefault();
          break;
        default:
          break;
      }
      return;
    }
    else if (aEvent.shiftKey &&
        aEvent.keyCode == Ci.nsIDOMKeyEvent.DOM_VK_RETURN) {
      // shift return
      // TODO: expand the inputNode height by one line
      return;
    }

    let inputUpdated = false;

    switch(aEvent.keyCode) {
      case Ci.nsIDOMKeyEvent.DOM_VK_ESCAPE:
        if (this.autocompletePopup.isOpen) {
          this.clearCompletion();
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_RETURN:
        if (this.autocompletePopup.isOpen) {
          this.acceptProposedCompletion();
        }
        else {
          this.execute();
        }
        aEvent.preventDefault();
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_UP:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_BACKWARD);
        }
        else if (this.canCaretGoPrevious()) {
          inputUpdated = this.historyPeruse(HISTORY_BACK);
        }
        if (inputUpdated) {
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_DOWN:
        if (this.autocompletePopup.isOpen) {
          inputUpdated = this.complete(this.COMPLETE_FORWARD);
        }
        else if (this.canCaretGoNext()) {
          inputUpdated = this.historyPeruse(HISTORY_FORWARD);
        }
        if (inputUpdated) {
          aEvent.preventDefault();
        }
        break;

      case Ci.nsIDOMKeyEvent.DOM_VK_TAB:
        // Generate a completion and accept the first proposed value.
        if (this.complete(this.COMPLETE_HINT_ONLY) &&
            this.lastCompletion &&
            this.acceptProposedCompletion()) {
          aEvent.preventDefault();
        }
        break;

      default:
        break;
    }
  },

  /**
   * Go up/down the history stack of input values.
   *
   * @param number aDirection
   *        History navigation direction: HISTORY_BACK or HISTORY_FORWARD.
   *
   * @returns boolean
   *          True if the input value changed, false otherwise.
   */
  historyPeruse: function JST_historyPeruse(aDirection)
  {
    if (!this.history.length) {
      return false;
    }

    // Up Arrow key
    if (aDirection == HISTORY_BACK) {
      if (this.historyPlaceHolder <= 0) {
        return false;
      }

      let inputVal = this.history[--this.historyPlaceHolder];
      if (inputVal){
        this.setInputValue(inputVal);
      }
    }
    // Down Arrow key
    else if (aDirection == HISTORY_FORWARD) {
      if (this.historyPlaceHolder == this.history.length - 1) {
        this.historyPlaceHolder ++;
        this.setInputValue("");
      }
      else if (this.historyPlaceHolder >= (this.history.length)) {
        return false;
      }
      else {
        let inputVal = this.history[++this.historyPlaceHolder];
        if (inputVal){
          this.setInputValue(inputVal);
        }
      }
    }
    else {
      throw new Error("Invalid argument 0");
    }

    return true;
  },

  refocus: function JSTF_refocus()
  {
    this.inputNode.blur();
    this.inputNode.focus();
  },

  /**
   * Check if the caret is at a location that allows selecting the previous item
   * in history when the user presses the Up arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the
   *         previous item in history when the user presses the Up arrow key,
   *         otherwise false.
   */
  canCaretGoPrevious: function JST_canCaretGoPrevious()
  {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == 0 ? true :
           node.selectionStart == node.value.length && !multiline;
  },

  /**
   * Check if the caret is at a location that allows selecting the next item in
   * history when the user presses the Down arrow key.
   *
   * @return boolean
   *         True if the caret is at a location that allows selecting the next
   *         item in history when the user presses the Down arrow key, otherwise
   *         false.
   */
  canCaretGoNext: function JST_canCaretGoNext()
  {
    let node = this.inputNode;
    if (node.selectionStart != node.selectionEnd) {
      return false;
    }

    let multiline = /[\r\n]/.test(node.value);
    return node.selectionStart == node.value.length ? true :
           node.selectionStart == 0 && !multiline;
  },

  history: [],

  // Stores the data for the last completion.
  lastCompletion: null,

  /**
   * Completes the current typed text in the inputNode. Completion is performed
   * only if the selection/cursor is at the end of the string. If no completion
   * is found, the current inputNode value and cursor/selection stay.
   *
   * @param int type possible values are
   *    - this.COMPLETE_FORWARD: If there is more than one possible completion
   *          and the input value stayed the same compared to the last time this
   *          function was called, then the next completion of all possible
   *          completions is used. If the value changed, then the first possible
   *          completion is used and the selection is set from the current
   *          cursor position to the end of the completed text.
   *          If there is only one possible completion, then this completion
   *          value is used and the cursor is put at the end of the completion.
   *    - this.COMPLETE_BACKWARD: Same as this.COMPLETE_FORWARD but if the
   *          value stayed the same as the last time the function was called,
   *          then the previous completion of all possible completions is used.
   *    - this.COMPLETE_HINT_ONLY: If there is more than one possible
   *          completion and the input value stayed the same compared to the
   *          last time this function was called, then the same completion is
   *          used again. If there is only one possible completion, then
   *          the inputNode.value is set to this value and the selection is set
   *          from the current cursor position to the end of the completed text.
   *
   * @returns boolean true if there existed a completion for the current input,
   *          or false otherwise.
   */
  complete: function JSTF_complete(type)
  {
    let inputNode = this.inputNode;
    let inputValue = inputNode.value;
    // If the inputNode has no value, then don't try to complete on it.
    if (!inputValue) {
      this.clearCompletion();
      return false;
    }

    // Only complete if the selection is empty and at the end of the input.
    if (inputNode.selectionStart == inputNode.selectionEnd &&
        inputNode.selectionEnd != inputValue.length) {
      this.clearCompletion();
      return false;
    }

    let popup = this.autocompletePopup;

    if (!this.lastCompletion || this.lastCompletion.value != inputValue) {
      let properties = this.propertyProvider(this.sandbox.window, inputValue);
      if (!properties || !properties.matches.length) {
        this.clearCompletion();
        return false;
      }

      let items = properties.matches.map(function(aMatch) {
        return {label: aMatch};
      });
      popup.setItems(items);
      this.lastCompletion = {value: inputValue,
                             matchProp: properties.matchProp};

      if (items.length > 1 && !popup.isOpen) {
        popup.openPopup(this.inputNode);
      }
      else if (items.length < 2 && popup.isOpen) {
        popup.hidePopup();
      }

      if (items.length > 0) {
        popup.selectedIndex = 0;
        if (items.length == 1) {
          // onSelect is not fired when the popup is not open.
          this.onAutocompleteSelect();
        }
      }
    }

    let accepted = false;

    if (type != this.COMPLETE_HINT_ONLY && popup.itemCount == 1) {
      this.acceptProposedCompletion();
      accepted = true;
    }
    else if (type == this.COMPLETE_BACKWARD) {
      this.autocompletePopup.selectPreviousItem();
    }
    else if (type == this.COMPLETE_FORWARD) {
      this.autocompletePopup.selectNextItem();
    }

    return accepted || popup.itemCount > 0;
  },

  onAutocompleteSelect: function JSTF_onAutocompleteSelect()
  {
    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion) {
      let suffix = currentItem.label.substring(this.lastCompletion.
                                               matchProp.length);
      this.updateCompleteNode(suffix);
    }
    else {
      this.updateCompleteNode("");
    }
  },

  /**
   * Clear the current completion information and close the autocomplete popup,
   * if needed.
   */
  clearCompletion: function JSTF_clearCompletion()
  {
    this.autocompletePopup.clearItems();
    this.lastCompletion = null;
    this.updateCompleteNode("");
    if (this.autocompletePopup.isOpen) {
      this.autocompletePopup.hidePopup();
    }
  },

  /**
   * Accept the proposed input completion.
   *
   * @return boolean
   *         True if there was a selected completion item and the input value
   *         was updated, false otherwise.
   */
  acceptProposedCompletion: function JSTF_acceptProposedCompletion()
  {
    let updated = false;

    let currentItem = this.autocompletePopup.selectedItem;
    if (currentItem && this.lastCompletion) {
      let suffix = currentItem.label.substring(this.lastCompletion.
                                               matchProp.length);
      this.setInputValue(this.inputNode.value + suffix);
      updated = true;
    }

    this.clearCompletion();

    return updated;
  },

  /**
   * Update the node that displays the currently selected autocomplete proposal.
   *
   * @param string aSuffix
   *        The proposed suffix for the inputNode value.
   */
  updateCompleteNode: function JSTF_updateCompleteNode(aSuffix)
  {
    // completion prefix = input, with non-control chars replaced by spaces
    let prefix = aSuffix ? this.inputNode.value.replace(/[\S]/g, " ") : "";
    this.completeNode.value = prefix + aSuffix;
  },

  /**
   * Destroy the JSTerm object. Call this method to avoid memory leaks.
   */
  destroy: function JST_destroy()
  {
    this.inputNode.removeEventListener("keypress", this._keyPress, false);
    this.inputNode.removeEventListener("input", this._inputEventHandler, false);
    this.inputNode.removeEventListener("keyup", this._inputEventHandler, false);
  },
};

/**
 * Generates and attaches the JS Terminal part of the Web Console, which
 * essentially consists of the interactive JavaScript input facility.
 *
 * @param nsWeakPtr<nsIDOMWindow> aContext
 *        A weak pointer to the DOM window that contains the Web Console.
 * @param nsIDOMNode aParentNode
 *        The Web Console wrapper node.
 * @param nsIDOMNode aExistingConsole
 *        The Web Console output node.
 * @return void
 */
function
JSTermFirefoxMixin(aContext,
                   aParentNode,
                   aExistingConsole)
{
  // aExisting Console is the existing outputNode to use in favor of
  // creating a new outputNode - this is so we can just attach the inputNode to
  // a normal HeadsUpDisplay console output, and re-use code.
  this.context = aContext;
  this.parentNode = aParentNode;
  this.existingConsoleNode = aExistingConsole;
  this.setTimeout = aParentNode.ownerDocument.defaultView.setTimeout;

  if (aParentNode.ownerDocument) {
    this.xulElementFactory =
      NodeFactory("xul", "xul", aParentNode.ownerDocument);

    this.textFactory = NodeFactory("text", "xul", aParentNode.ownerDocument);
    this.generateUI();
    this.attachUI();
  }
  else {
    throw new Error("aParentNode should be a DOM node with an ownerDocument property ");
  }
}

JSTermFirefoxMixin.prototype = {
  /**
   * Generates and attaches the UI for an entire JS Workspace or
   * just the input node used under the console output
   *
   * @returns void
   */
  generateUI: function JSTF_generateUI()
  {
    this.completeNode = this.xulElementFactory("textbox");
    this.completeNode.setAttribute("class", "jsterm-complete-node");
    this.completeNode.setAttribute("multiline", "true");
    this.completeNode.setAttribute("rows", "1");

    this.inputNode = this.xulElementFactory("textbox");
    this.inputNode.setAttribute("class", "jsterm-input-node");
    this.inputNode.setAttribute("multiline", "true");
    this.inputNode.setAttribute("rows", "1");

    let inputStack = this.xulElementFactory("stack");
    inputStack.setAttribute("class", "jsterm-stack-node");
    inputStack.setAttribute("flex", "1");
    inputStack.appendChild(this.completeNode);
    inputStack.appendChild(this.inputNode);

    if (this.existingConsoleNode == undefined) {
      this.outputNode = this.xulElementFactory("vbox");
      this.outputNode.setAttribute("class", "jsterm-output-node");

      this.term = this.xulElementFactory("vbox");
      this.term.setAttribute("class", "jsterm-wrapper-node");
      this.term.setAttribute("flex", "1");
      this.term.appendChild(this.outputNode);
    }
    else {
      this.outputNode = this.existingConsoleNode;

      this.term = this.xulElementFactory("hbox");
      this.term.setAttribute("class", "jsterm-input-container");
      this.term.setAttribute("style", "direction: ltr;");
      this.term.appendChild(inputStack);
    }
  },

  get inputValue()
  {
    return this.inputNode.value;
  },

  attachUI: function JSTF_attachUI()
  {
    this.parentNode.appendChild(this.term);
  }
};

/**
 * Firefox-specific Application Hooks.
 * Each Gecko-based application will need an object like this in
 * order to use the Heads Up Display
 */
function FirefoxApplicationHooks()
{ }

FirefoxApplicationHooks.prototype = {
  /**
   * gets the current contentWindow (Firefox-specific)
   *
   * @returns nsIDOMWindow
   */
  getCurrentContext: function FAH_getCurrentContext()
  {
    return Services.wm.getMostRecentWindow("navigator:browser");
  },
};

//////////////////////////////////////////////////////////////////////////////
// Utility functions used by multiple callers
//////////////////////////////////////////////////////////////////////////////

/**
 * ConsoleUtils: a collection of globally used functions
 *
 */

ConsoleUtils = {
  supString: function ConsoleUtils_supString(aString)
  {
    let str = Cc["@mozilla.org/supports-string;1"].
      createInstance(Ci.nsISupportsString);
    str.data = aString;
    return str;
  },

  /**
   * Generates a millisecond resolution timestamp.
   *
   * @returns integer
   */
  timestamp: function ConsoleUtils_timestamp()
  {
    return Date.now();
  },

  /**
   * Generates a formatted timestamp string for displaying in console messages.
   *
   * @param integer [ms] Optional, allows you to specify the timestamp in
   * milliseconds since the UNIX epoch.
   * @returns string The timestamp formatted for display.
   */
  timestampString: function ConsoleUtils_timestampString(ms)
  {
    var d = new Date(ms ? ms : null);
    let hours = d.getHours(), minutes = d.getMinutes();
    let seconds = d.getSeconds(), milliseconds = d.getMilliseconds();
    let parameters = [ hours, minutes, seconds, milliseconds ];
    return HUDService.getFormatStr("timestampFormat", parameters);
  },

  /**
   * Scrolls a node so that it's visible in its containing XUL "scrollbox"
   * element.
   *
   * @param nsIDOMNode aNode
   *        The node to make visible.
   * @returns void
   */
  scrollToVisible: function ConsoleUtils_scrollToVisible(aNode) {
    // Find the enclosing richlistbox node.
    let richListBoxNode = aNode.parentNode;
    while (richListBoxNode.tagName != "richlistbox") {
      richListBoxNode = richListBoxNode.parentNode;
    }

    // Use the scroll box object interface to ensure the element is visible.
    let boxObject = richListBoxNode.scrollBoxObject;
    let nsIScrollBoxObject = boxObject.QueryInterface(Ci.nsIScrollBoxObject);
    nsIScrollBoxObject.ensureElementIsVisible(aNode);
  },

  /**
   * Given a category and message body, creates a DOM node to represent an
   * incoming message. The timestamp is automatically added.
   *
   * @param nsIDOMDocument aDocument
   *        The document in which to create the node.
   * @param number aCategory
   *        The category of the message: one of the CATEGORY_* constants.
   * @param number aSeverity
   *        The severity of the message: one of the SEVERITY_* constants;
   * @param string|nsIDOMNode aBody
   *        The body of the message, either a simple string or a DOM node.
   * @param string aSourceURL [optional]
   *        The URL of the source file that emitted the error.
   * @param number aSourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @param string aClipboardText [optional]
   *        The text that should be copied to the clipboard when this node is
   *        copied. If omitted, defaults to the body text. If `aBody` is not
   *        a string, then the clipboard text must be supplied.
   * @param number aLevel [optional]
   *        The level of the console API message.
   * @return nsIDOMNode
   *         The message node: a XUL richlistitem ready to be inserted into
   *         the Web Console output node.
   */
  createMessageNode:
  function ConsoleUtils_createMessageNode(aDocument, aCategory, aSeverity,
                                          aBody, aSourceURL, aSourceLine,
                                          aClipboardText, aLevel) {
    if (aBody instanceof Ci.nsIDOMNode && aClipboardText == null) {
      throw new Error("HUDService.createMessageNode(): DOM node supplied " +
                      "without any clipboard text");
    }

    // Make the icon container, which is a vertical box. Its purpose is to
    // ensure that the icon stays anchored at the top of the message even for
    // long multi-line messages.
    let iconContainer = aDocument.createElementNS(XUL_NS, "vbox");
    iconContainer.classList.add("webconsole-msg-icon-container");

    // Make the icon node. It's sprited and the actual region of the image is
    // determined by CSS rules.
    let iconNode = aDocument.createElementNS(XUL_NS, "image");
    iconNode.classList.add("webconsole-msg-icon");
    iconContainer.appendChild(iconNode);

    // Make the spacer that positions the icon.
    let spacer = aDocument.createElementNS(XUL_NS, "spacer");
    spacer.setAttribute("flex", "1");
    iconContainer.appendChild(spacer);

    // Create the message body, which contains the actual text of the message.
    let bodyNode = aDocument.createElementNS(XUL_NS, "description");
    bodyNode.setAttribute("flex", "1");
    bodyNode.classList.add("webconsole-msg-body");

    // Store the body text, since it is needed later for the property tree
    // case.
    let body = aBody;
    // If a string was supplied for the body, turn it into a DOM node and an
    // associated clipboard string now.
    aClipboardText = aClipboardText ||
                     (aBody + (aSourceURL ? " @ " + aSourceURL : "") +
                              (aSourceLine ? ":" + aSourceLine : ""));
    aBody = aBody instanceof Ci.nsIDOMNode && !(aLevel == "dir") ?
            aBody : aDocument.createTextNode(aBody);

    bodyNode.appendChild(aBody);

    let repeatContainer = aDocument.createElementNS(XUL_NS, "hbox");
    repeatContainer.setAttribute("align", "start");
    let repeatNode = aDocument.createElementNS(XUL_NS, "label");
    repeatNode.setAttribute("value", "1");
    repeatNode.classList.add("webconsole-msg-repeat");
    repeatContainer.appendChild(repeatNode);

    // Create the timestamp.
    let timestampNode = aDocument.createElementNS(XUL_NS, "label");
    timestampNode.classList.add("webconsole-timestamp");
    let timestamp = ConsoleUtils.timestamp();
    let timestampString = ConsoleUtils.timestampString(timestamp);
    timestampNode.setAttribute("value", timestampString);

    // Create the source location (e.g. www.example.com:6) that sits on the
    // right side of the message, if applicable.
    let locationNode;
    if (aSourceURL) {
      locationNode = this.createLocationNode(aDocument, aSourceURL,
                                             aSourceLine);
    }

    // Create the containing node and append all its elements to it.
    let node = aDocument.createElementNS(XUL_NS, "richlistitem");
    node.clipboardText = aClipboardText;
    node.classList.add("hud-msg-node");

    node.timestamp = timestamp;
    ConsoleUtils.setMessageType(node, aCategory, aSeverity);

    node.appendChild(timestampNode);
    node.appendChild(iconContainer);
    // Display the object tree after the message node.
    if (aLevel == "dir") {
      // Make the body container, which is a vertical box, for grouping the text
      // and tree widgets.
      let bodyContainer = aDocument.createElement("vbox");
      bodyContainer.setAttribute("flex", "1");
      bodyContainer.appendChild(bodyNode);
      // Create the tree.
      let tree = createElement(aDocument, "tree", {
        flex: 1,
        hidecolumnpicker: "true"
      });

      let treecols = aDocument.createElement("treecols");
      let treecol = createElement(aDocument, "treecol", {
        primary: "true",
        flex: 1,
        hideheader: "true",
        ignoreincolumnpicker: "true"
      });
      treecols.appendChild(treecol);
      tree.appendChild(treecols);

      tree.appendChild(aDocument.createElement("treechildren"));

      bodyContainer.appendChild(tree);
      node.appendChild(bodyContainer);
      node.classList.add("webconsole-msg-inspector");
      // Create the treeView object.
      let treeView = node.propertyTreeView = new PropertyTreeView();
      treeView.data = body;
      tree.setAttribute("rows", treeView.rowCount);
    }
    else {
      node.appendChild(bodyNode);
    }
    node.appendChild(repeatContainer);
    if (locationNode) {
      node.appendChild(locationNode);
    }

    node.setAttribute("id", "console-msg-" + HUDService.sequenceId());

    return node;
  },

  /**
   * Adjusts the category and severity of the given message, clearing the old
   * category and severity if present.
   *
   * @param nsIDOMNode aMessageNode
   *        The message node to alter.
   * @param number aNewCategory
   *        The new category for the message; one of the CATEGORY_ constants.
   * @param number aNewSeverity
   *        The new severity for the message; one of the SEVERITY_ constants.
   * @return void
   */
  setMessageType:
  function ConsoleUtils_setMessageType(aMessageNode, aNewCategory,
                                       aNewSeverity) {
    // Remove the old CSS classes, if applicable.
    if ("category" in aMessageNode) {
      let oldCategory = aMessageNode.category;
      let oldSeverity = aMessageNode.severity;
      aMessageNode.classList.remove("webconsole-msg-" +
                                    CATEGORY_CLASS_FRAGMENTS[oldCategory]);
      aMessageNode.classList.remove("webconsole-msg-" +
                                    SEVERITY_CLASS_FRAGMENTS[oldSeverity]);
      let key = "hud-" + MESSAGE_PREFERENCE_KEYS[oldCategory][oldSeverity];
      aMessageNode.classList.remove(key);
    }

    // Add in the new CSS classes.
    aMessageNode.category = aNewCategory;
    aMessageNode.severity = aNewSeverity;
    aMessageNode.classList.add("webconsole-msg-" +
                               CATEGORY_CLASS_FRAGMENTS[aNewCategory]);
    aMessageNode.classList.add("webconsole-msg-" +
                               SEVERITY_CLASS_FRAGMENTS[aNewSeverity]);
    let key = "hud-" + MESSAGE_PREFERENCE_KEYS[aNewCategory][aNewSeverity];
    aMessageNode.classList.add(key);
  },

  /**
   * Creates the XUL label that displays the textual location of an incoming
   * message.
   *
   * @param nsIDOMDocument aDocument
   *        The document in which to create the node.
   * @param string aSourceURL
   *        The URL of the source file responsible for the error.
   * @param number aSourceLine [optional]
   *        The line number on which the error occurred. If zero or omitted,
   *        there is no line number associated with this message.
   * @return nsIDOMNode
   *         The new XUL label node, ready to be added to the message node.
   */
  createLocationNode:
  function ConsoleUtils_createLocationNode(aDocument, aSourceURL,
                                           aSourceLine) {
    let locationNode = aDocument.createElementNS(XUL_NS, "label");

    // Create the text, which consists of an abbreviated version of the URL
    // plus an optional line number.
    let text = ConsoleUtils.abbreviateSourceURL(aSourceURL);
    if (aSourceLine) {
      text += ":" + aSourceLine;
    }
    locationNode.setAttribute("value", text);

    // Style appropriately.
    locationNode.setAttribute("crop", "center");
    locationNode.setAttribute("title", aSourceURL);
    locationNode.classList.add("webconsole-location");
    locationNode.classList.add("text-link");

    // Make the location clickable.
    locationNode.addEventListener("click", function() {
      if (aSourceURL == "Scratchpad") {
        let win = Services.wm.getMostRecentWindow("devtools:scratchpad");
        if (win) {
          win.focus();
        }
        return;
      }
      let viewSourceUtils = aDocument.defaultView.gViewSourceUtils;
      viewSourceUtils.viewSource(aSourceURL, null, aDocument, aSourceLine);
    }, true);

    return locationNode;
  },

  /**
   * Applies the user's filters to a newly-created message node via CSS
   * classes.
   *
   * @param nsIDOMNode aNode
   *        The newly-created message node.
   * @param string aHUDId
   *        The ID of the HUD which this node is to be inserted into.
   */
  filterMessageNode: function ConsoleUtils_filterMessageNode(aNode, aHUDId) {
    // Filter by the message type.
    let prefKey = MESSAGE_PREFERENCE_KEYS[aNode.category][aNode.severity];
    if (prefKey && !HUDService.getFilterState(aHUDId, prefKey)) {
      // The node is filtered by type.
      aNode.classList.add("hud-filtered-by-type");
    }

    // Filter on the search string.
    let search = HUDService.getFilterStringByHUDId(aHUDId);
    let text = aNode.clipboardText;

    // if string matches the filter text
    if (!HUDService.stringMatchesFilters(text, search)) {
      aNode.classList.add("hud-filtered-by-string");
    }
  },

  /**
   * Merge the attributes of the two nodes that are about to be filtered.
   * Increment the number of repeats of aOriginal.
   *
   * @param nsIDOMNode aOriginal
   *        The Original Node. The one being merged into.
   * @param nsIDOMNode aFiltered
   *        The node being filtered out because it is repeated.
   */
  mergeFilteredMessageNode:
  function ConsoleUtils_mergeFilteredMessageNode(aOriginal, aFiltered) {
    // childNodes[3].firstChild is the node containing the number of repetitions
    // of a node.
    let repeatNode = aOriginal.childNodes[3].firstChild;
    if (!repeatNode) {
      return aOriginal; // no repeat node, return early.
    }

    let occurrences = parseInt(repeatNode.getAttribute("value")) + 1;
    repeatNode.setAttribute("value", occurrences);
  },

  /**
   * Filter the css node from the output node if it is a repeat. CSS messages
   * are merged with previous messages if they occurred in the past.
   *
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @param nsIDOMNode aOutput
   *        The outputNode of the HUD.
   * @returns boolean
   *         true if the message is filtered, false otherwise.
   */
  filterRepeatedCSS:
  function ConsoleUtils_filterRepeatedCSS(aNode, aOutput, aHUDId) {
    let hud = HUDService.getHudReferenceById(aHUDId);

    // childNodes[2] is the description node containing the text of the message.
    let description = aNode.childNodes[2].textContent;
    let location;

    // childNodes[4] represents the location (source URL) of the error message.
    // The full source URL is stored in the title attribute.
    if (aNode.childNodes[4]) {
      // browser_webconsole_bug_595934_message_categories.js
      location = aNode.childNodes[4].getAttribute("title");
    }
    else {
      location = "";
    }

    let dupe = hud.cssNodes[description + location];
    if (!dupe) {
      // no matching nodes
      hud.cssNodes[description + location] = aNode;
      return false;
    }

    this.mergeFilteredMessageNode(dupe, aNode);

    return true;
  },

  /**
   * Filter the console node from the output node if it is a repeat. Console
   * messages are filtered from the output if and only if they match the
   * immediately preceding message. The output node's last occurrence should
   * have its timestamp updated.
   *
   * @param nsIDOMNode aNode
   *        The message node to be filtered or not.
   * @param nsIDOMNode aOutput
   *        The outputNode of the HUD.
   * @return boolean
   *         true if the message is filtered, false otherwise.
   */
  filterRepeatedConsole:
  function ConsoleUtils_filterRepeatedConsole(aNode, aOutput) {
    let lastMessage = aOutput.lastChild;

    // childNodes[2] is the description element
    if (lastMessage && !aNode.classList.contains("webconsole-msg-inspector") &&
        aNode.childNodes[2].textContent ==
        lastMessage.childNodes[2].textContent) {
      this.mergeFilteredMessageNode(lastMessage, aNode);
      return true;
    }

    return false;
  },

  /**
   * Filters a node appropriately, then sends it to the output, regrouping and
   * pruning output as necessary.
   *
   * @param nsIDOMNode aNode
   *        The message node to send to the output.
   * @param string aHUDId
   *        The ID of the HUD in which to insert this node.
   */
  outputMessageNode: function ConsoleUtils_outputMessageNode(aNode, aHUDId) {
    ConsoleUtils.filterMessageNode(aNode, aHUDId);
    let outputNode = HUDService.hudReferences[aHUDId].outputNode;

    let scrolledToBottom = ConsoleUtils.isOutputScrolledToBottom(outputNode);

    let isRepeated = false;
    if (aNode.classList.contains("webconsole-msg-cssparser")) {
      isRepeated = this.filterRepeatedCSS(aNode, outputNode, aHUDId);
    }

    if (!isRepeated &&
        (aNode.classList.contains("webconsole-msg-console") ||
         aNode.classList.contains("webconsole-msg-exception") ||
         aNode.classList.contains("webconsole-msg-error"))) {
      isRepeated = this.filterRepeatedConsole(aNode, outputNode);
    }

    if (!isRepeated) {
      outputNode.appendChild(aNode);
    }

    HUDService.regroupOutput(outputNode);

    if (pruneConsoleOutputIfNecessary(aHUDId, aNode.category) == 0) {
      // We can't very well scroll to make the message node visible if the log
      // limit is zero and the node was destroyed in the first place.
      return;
    }

    let isInputOutput = aNode.classList.contains("webconsole-msg-input") ||
                        aNode.classList.contains("webconsole-msg-output");
    let isFiltered = aNode.classList.contains("hud-filtered-by-string") ||
                     aNode.classList.contains("hud-filtered-by-type");

    // Scroll to the new node if it is not filtered, and if the output node is
    // scrolled at the bottom or if the new node is a jsterm input/output
    // message.
    if (!isFiltered && !isRepeated && (scrolledToBottom || isInputOutput)) {
      ConsoleUtils.scrollToVisible(aNode);
    }

    let id = ConsoleUtils.supString(aHUDId);
    let nodeID = aNode.getAttribute("id");
    Services.obs.notifyObservers(id, "web-console-message-created", nodeID);
  },

  /**
   * Check if the given output node is scrolled to the bottom.
   *
   * @param nsIDOMNode aOutputNode
   * @return boolean
   *         True if the output node is scrolled to the bottom, or false
   *         otherwise.
   */
  isOutputScrolledToBottom:
  function ConsoleUtils_isOutputScrolledToBottom(aOutputNode)
  {
    let lastNodeHeight = aOutputNode.lastChild ?
                         aOutputNode.lastChild.clientHeight : 0;
    let scrollBox = aOutputNode.scrollBoxObject.element;

    return scrollBox.scrollTop + scrollBox.clientHeight >=
           scrollBox.scrollHeight - lastNodeHeight / 2;
  },

  /**
   * Abbreviates the given source URL so that it can be displayed flush-right
   * without being too distracting.
   *
   * @param string aSourceURL
   *        The source URL to shorten.
   * @return string
   *         The abbreviated form of the source URL.
   */
  abbreviateSourceURL: function ConsoleUtils_abbreviateSourceURL(aSourceURL) {
    // Remove any query parameters.
    let hookIndex = aSourceURL.indexOf("?");
    if (hookIndex > -1) {
      aSourceURL = aSourceURL.substring(0, hookIndex);
    }

    // Remove a trailing "/".
    if (aSourceURL[aSourceURL.length - 1] == "/") {
      aSourceURL = aSourceURL.substring(0, aSourceURL.length - 1);
    }

    // Remove all but the last path component.
    let slashIndex = aSourceURL.lastIndexOf("/");
    if (slashIndex > -1) {
      aSourceURL = aSourceURL.substring(slashIndex + 1);
    }

    return aSourceURL;
  }
};

//////////////////////////////////////////////////////////////////////////
// HeadsUpDisplayUICommands
//////////////////////////////////////////////////////////////////////////

HeadsUpDisplayUICommands = {
  toggleHUD: function UIC_toggleHUD() {
    var window = HUDService.currentContext();
    var gBrowser = window.gBrowser;
    var linkedBrowser = gBrowser.selectedTab.linkedBrowser;
    var tabId = gBrowser.getNotificationBox(linkedBrowser).getAttribute("id");
    var hudId = "hud_" + tabId;
    var ownerDocument = gBrowser.selectedTab.ownerDocument;
    var hud = ownerDocument.getElementById(hudId);
    var hudRef = HUDService.hudReferences[hudId];

    if (hudRef && hud) {
      if (hudRef.consolePanel) {
        HUDService.deactivateHUDForContext(gBrowser.selectedTab, false);
      }
      else {
        HUDService.storeHeight(hudId);

        HUDService.animate(hudId, ANIMATE_OUT, function() {
          // If the user closes the console while the console is animating away,
          // then these callbacks will queue up, but all the callbacks after the
          // first will have no console to operate on. This test handles this
          // case gracefully.
          if (ownerDocument.getElementById(hudId)) {
            HUDService.deactivateHUDForContext(gBrowser.selectedTab, true);
          }
        });
      }
    }
    else {
      HUDService.activateHUDForContext(gBrowser.selectedTab, true);
      HUDService.animate(hudId, ANIMATE_IN);
    }
  },

  /**
   * Find the hudId for the active chrome window.
   * @return string|null
   *         The hudId or null if the active chrome window has no open Web
   *         Console.
   */
  getOpenHUD: function UIC_getOpenHUD() {
    let chromeWindow = HUDService.currentContext();
    let contentWindow = chromeWindow.gBrowser.selectedBrowser.contentWindow;
    return HUDService.getHudIdByWindow(contentWindow);
  },

  /**
   * The event handler that is called whenever a user switches a filter on or
   * off.
   *
   * @param nsIDOMEvent aEvent
   *        The event that triggered the filter change.
   * @return boolean
   */
  toggleFilter: function UIC_toggleFilter(aEvent) {
    let hudId = this.getAttribute("hudId");
    switch (this.tagName) {
      case "toolbarbutton": {
        let originalTarget = aEvent.originalTarget;
        let classes = originalTarget.classList;

        if (originalTarget.localName !== "toolbarbutton") {
          // Oddly enough, the click event is sent to the menu button when
          // selecting a menu item with the mouse. Detect this case and bail
          // out.
          break;
        }

        if (!classes.contains("toolbarbutton-menubutton-button") &&
            originalTarget.getAttribute("type") === "menu-button") {
          // This is a filter button with a drop-down. The user clicked the
          // drop-down, so do nothing. (The menu will automatically appear
          // without our intervention.)
          break;
        }

        let state = this.getAttribute("checked") !== "true";
        this.setAttribute("checked", state);

        // This is a filter button with a drop-down, and the user clicked the
        // main part of the button. Go through all the severities and toggle
        // their associated filters.
        let menuItems = this.querySelectorAll("menuitem");
        for (let i = 0; i < menuItems.length; i++) {
          menuItems[i].setAttribute("checked", state);
          let prefKey = menuItems[i].getAttribute("prefKey");
          HUDService.setFilterState(hudId, prefKey, state);
        }
        break;
      }

      case "menuitem": {
        let state = this.getAttribute("checked") !== "true";
        this.setAttribute("checked", state);

        let prefKey = this.getAttribute("prefKey");
        HUDService.setFilterState(hudId, prefKey, state);

        // Adjust the state of the button appropriately.
        let menuPopup = this.parentNode;

        let allChecked = true;
        let menuItem = menuPopup.firstChild;
        while (menuItem) {
          if (menuItem.getAttribute("checked") !== "true") {
            allChecked = false;
            break;
          }
          menuItem = menuItem.nextSibling;
        }

        let toolbarButton = menuPopup.parentNode;
        toolbarButton.setAttribute("checked", allChecked);
        break;
      }
    }

    return true;
  },

  command: function UIC_command(aButton) {
    var filter = aButton.getAttribute("buttonType");
    var hudId = aButton.getAttribute("hudId");
    switch (filter) {
      case "copy": {
        let outputNode = HUDService.hudReferences[hudId].outputNode;
        HUDService.copySelectedItems(outputNode);
        break;
      }
      case "selectAll": {
        HUDService.hudReferences[hudId].outputNode.selectAll();
        break;
      }
      case "saveBodies": {
        let checked = aButton.getAttribute("checked") === "true";
        HUDService.saveRequestAndResponseBodies = checked;
        break;
      }
    }
  },

};

//////////////////////////////////////////////////////////////////////////
// ConsoleStorage
//////////////////////////////////////////////////////////////////////////

var prefs = Services.prefs;

const GLOBAL_STORAGE_INDEX_ID = "GLOBAL_CONSOLE";
const PREFS_BRANCH_PREF = "devtools.hud.display.filter";
const PREFS_PREFIX = "devtools.hud.display.filter.";
const PREFS = { network: PREFS_PREFIX + "network",
                networkinfo: PREFS_PREFIX + "networkinfo",
                csserror: PREFS_PREFIX + "csserror",
                cssparser: PREFS_PREFIX + "cssparser",
                exception: PREFS_PREFIX + "exception",
                jswarn: PREFS_PREFIX + "jswarn",
                error: PREFS_PREFIX + "error",
                info: PREFS_PREFIX + "info",
                warn: PREFS_PREFIX + "warn",
                log: PREFS_PREFIX + "log",
                global: PREFS_PREFIX + "global",
              };

function ConsoleStorage()
{
  this.sequencer = null;
  this.consoleDisplays = {};
  // each display will have an index that tracks each ConsoleEntry
  this.displayIndexes = {};
  this.globalStorageIndex = [];
  this.globalDisplay = {};
  this.createDisplay(GLOBAL_STORAGE_INDEX_ID);
  // TODO: need to create a method that truncates the message
  // see bug 570543

  // store an index of display prefs
  this.displayPrefs = {};

  // check prefs for existence, create & load if absent, load them if present
  let filterPrefs;
  let defaultDisplayPrefs;

  try {
    filterPrefs = prefs.getBoolPref(PREFS_BRANCH_PREF);
  }
  catch (ex) {
    filterPrefs = false;
  }

  // TODO: for FINAL release,
  // use the sitePreferencesService to save specific site prefs
  // see bug 570545

  if (filterPrefs) {
    defaultDisplayPrefs = {
      network: (prefs.getBoolPref(PREFS.network) ? true: false),
      networkinfo: (prefs.getBoolPref(PREFS.networkinfo) ? true: false),
      csserror: (prefs.getBoolPref(PREFS.csserror) ? true: false),
      cssparser: (prefs.getBoolPref(PREFS.cssparser) ? true: false),
      exception: (prefs.getBoolPref(PREFS.exception) ? true: false),
      jswarn: (prefs.getBoolPref(PREFS.jswarn) ? true: false),
      error: (prefs.getBoolPref(PREFS.error) ? true: false),
      info: (prefs.getBoolPref(PREFS.info) ? true: false),
      warn: (prefs.getBoolPref(PREFS.warn) ? true: false),
      log: (prefs.getBoolPref(PREFS.log) ? true: false),
      global: (prefs.getBoolPref(PREFS.global) ? true: false),
    };
  }
  else {
    prefs.setBoolPref(PREFS_BRANCH_PREF, false);
    // default prefs for each HeadsUpDisplay
    prefs.setBoolPref(PREFS.network, true);
    prefs.setBoolPref(PREFS.networkinfo, true);
    prefs.setBoolPref(PREFS.csserror, true);
    prefs.setBoolPref(PREFS.cssparser, true);
    prefs.setBoolPref(PREFS.exception, true);
    prefs.setBoolPref(PREFS.jswarn, true);
    prefs.setBoolPref(PREFS.error, true);
    prefs.setBoolPref(PREFS.info, true);
    prefs.setBoolPref(PREFS.warn, true);
    prefs.setBoolPref(PREFS.log, true);
    prefs.setBoolPref(PREFS.global, false);

    defaultDisplayPrefs = {
      network: prefs.getBoolPref(PREFS.network),
      networkinfo: prefs.getBoolPref(PREFS.networkinfo),
      csserror: prefs.getBoolPref(PREFS.csserror),
      cssparser: prefs.getBoolPref(PREFS.cssparser),
      exception: prefs.getBoolPref(PREFS.exception),
      jswarn: prefs.getBoolPref(PREFS.jswarn),
      error: prefs.getBoolPref(PREFS.error),
      info: prefs.getBoolPref(PREFS.info),
      warn: prefs.getBoolPref(PREFS.warn),
      log: prefs.getBoolPref(PREFS.log),
      global: prefs.getBoolPref(PREFS.global),
    };
  }
  this.defaultDisplayPrefs = defaultDisplayPrefs;
}

ConsoleStorage.prototype = {

  updateDefaultDisplayPrefs:
  function CS_updateDefaultDisplayPrefs(aPrefsObject) {
    prefs.setBoolPref(PREFS.network, (aPrefsObject.network ? true : false));
    prefs.setBoolPref(PREFS.networkinfo,
                      (aPrefsObject.networkinfo ? true : false));
    prefs.setBoolPref(PREFS.csserror, (aPrefsObject.csserror ? true : false));
    prefs.setBoolPref(PREFS.cssparser, (aPrefsObject.cssparser ? true : false));
    prefs.setBoolPref(PREFS.exception, (aPrefsObject.exception ? true : false));
    prefs.setBoolPref(PREFS.jswarn, (aPrefsObject.jswarn ? true : false));
    prefs.setBoolPref(PREFS.error, (aPrefsObject.error ? true : false));
    prefs.setBoolPref(PREFS.info, (aPrefsObject.info ? true : false));
    prefs.setBoolPref(PREFS.warn, (aPrefsObject.warn ? true : false));
    prefs.setBoolPref(PREFS.log, (aPrefsObject.log ? true : false));
    prefs.setBoolPref(PREFS.global, (aPrefsObject.global ? true : false));
  },

  sequenceId: function CS_sequencerId()
  {
    if (!this.sequencer) {
      this.sequencer = this.createSequencer();
    }
    return this.sequencer.next();
  },

  createSequencer: function CS_createSequencer()
  {
    function sequencer(aInt) {
      while(1) {
        aInt++;
        yield aInt;
      }
    }
    return sequencer(-1);
  },

  globalStore: function CS_globalStore(aIndex)
  {
    return this.displayStore(GLOBAL_CONSOLE_DOM_NODE_ID);
  },

  displayStore: function CS_displayStore(aId)
  {
    var self = this;
    var idx = -1;
    var id = aId;
    var aLength = self.displayIndexes[id].length;

    function displayStoreGenerator(aInt, aLength)
    {
      // create a generator object to iterate through any of the display stores
      // from any index-starting-point
      while(1) {
        // throw if we exceed the length of displayIndexes?
        aInt++;
        var indexIt = self.displayIndexes[id];
        var index = indexIt[aInt];
        if (aLength < aInt) {
          // try to see if we have more entries:
          var newLength = self.displayIndexes[id].length;
          if (newLength > aLength) {
            aLength = newLength;
          }
          else {
            throw new StopIteration();
          }
        }
        var entry = self.consoleDisplays[id][index];
        yield entry;
      }
    }

    return displayStoreGenerator(-1, aLength);
  },

  recordEntries: function CS_recordEntries(aHUDId, aConfigArray)
  {
    var len = aConfigArray.length;
    for (var i = 0; i < len; i++){
      this.recordEntry(aHUDId, aConfigArray[i]);
    }
  },


  recordEntry: function CS_recordEntry(aHUDId, aConfig)
  {
    var id = this.sequenceId();

    this.globalStorageIndex[id] = { hudId: aHUDId };

    var displayStorage = this.consoleDisplays[aHUDId];

    var displayIndex = this.displayIndexes[aHUDId];

    if (displayStorage && displayIndex) {
      var entry = new ConsoleEntry(aConfig, id);
      displayIndex.push(entry.id);
      displayStorage[entry.id] = entry;
      return entry;
    }
    else {
      throw new Error("Cannot get displayStorage or index object for id " + aHUDId);
    }
  },

  getEntry: function CS_getEntry(aId)
  {
    var display = this.globalStorageIndex[aId];
    var storName = display.hudId;
    return this.consoleDisplays[storName][aId];
  },

  updateEntry: function CS_updateEntry(aUUID)
  {
    // update an individual entry
    // TODO: see bug 568634
  },

  createDisplay: function CS_createdisplay(aId)
  {
    if (!this.consoleDisplays[aId]) {
      this.consoleDisplays[aId] = {};
      this.displayIndexes[aId] = [];
    }
  },

  removeDisplay: function CS_removeDisplay(aId)
  {
    try {
      delete this.consoleDisplays[aId];
      delete this.displayIndexes[aId];
    }
    catch (ex) {
      Cu.reportError("Could not remove console display for id " + aId);
    }
  }
};

/**
 * A Console log entry
 *
 * @param JSObject aConfig, object literal with ConsolEntry properties
 * @param integer aId
 * @returns void
 */

function ConsoleEntry(aConfig, id)
{
  if (!aConfig.logLevel && aConfig.message) {
    throw new Error("Missing Arguments when creating a console entry");
  }

  this.config = aConfig;
  this.id = id;
  for (var prop in aConfig) {
    if (!(typeof aConfig[prop] == "function")){
      this[prop] = aConfig[prop];
    }
  }

  if (aConfig.logLevel == "network") {
    this.transactions = { };
    if (aConfig.activity) {
      this.transactions[aConfig.activity.stage] = aConfig.activity;
    }
  }

}

ConsoleEntry.prototype = {

  updateTransaction: function CE_updateTransaction(aActivity) {
    this.transactions[aActivity.stage] = aActivity;
  }
};

//////////////////////////////////////////////////////////////////////////
// HUDWindowObserver
//////////////////////////////////////////////////////////////////////////

HUDWindowObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver,]
  ),

  init: function HWO_init()
  {
    Services.obs.addObserver(this, "xpcom-shutdown", false);
    Services.obs.addObserver(this, "content-document-global-created", false);
  },

  observe: function HWO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "content-document-global-created") {
      HUDService.windowInitializer(aSubject);
    }
    else if (aTopic == "xpcom-shutdown") {
      this.uninit();
    }
  },

  uninit: function HWO_uninit()
  {
    Services.obs.removeObserver(this, "content-document-global-created");
    Services.obs.removeObserver(this, "xpcom-shutdown");
    this.initialConsoleCreated = false;
  },

};

///////////////////////////////////////////////////////////////////////////////
// CommandController
///////////////////////////////////////////////////////////////////////////////

/**
 * A controller (an instance of nsIController) that makes editing actions
 * behave appropriately in the context of the Web Console.
 */
function CommandController(aWindow) {
  this.window = aWindow;
}

CommandController.prototype = {
  /**
   * Returns the HUD output node that currently has the focus, or null if the
   * currently-focused element isn't inside the output node.
   *
   * @returns nsIDOMNode
   *          The currently-focused output node.
   */
  _getFocusedOutputNode: function CommandController_getFocusedOutputNode()
  {
    let element = this.window.document.commandDispatcher.focusedElement;
    if (element && element.classList.contains("hud-output-node")) {
      return element;
    }
    return null;
  },

  /**
   * Copies the currently-selected entries in the Web Console output to the
   * clipboard.
   *
   * @param nsIDOMNode aOutputNode
   *        The Web Console output node.
   * @returns void
   */
  copy: function CommandController_copy(aOutputNode)
  {
    HUDService.copySelectedItems(aOutputNode);
  },

  /**
   * Selects all the text in the HUD output.
   *
   * @param nsIDOMNode aOutputNode
   *        The HUD output node.
   * @returns void
   */
  selectAll: function CommandController_selectAll(aOutputNode)
  {
    aOutputNode.selectAll();
  },

  supportsCommand: function CommandController_supportsCommand(aCommand)
  {
    return this.isCommandEnabled(aCommand);
  },

  isCommandEnabled: function CommandController_isCommandEnabled(aCommand)
  {
    let outputNode = this._getFocusedOutputNode();
    if (!outputNode) {
      return false;
    }

    switch (aCommand) {
      case "cmd_copy":
        // Only enable "copy" if nodes are selected.
        return outputNode.selectedCount > 0;
      case "cmd_selectAll":
        // "Select All" is always enabled.
        return true;
    }
  },

  doCommand: function CommandController_doCommand(aCommand)
  {
    let outputNode = this._getFocusedOutputNode();
    switch (aCommand) {
      case "cmd_copy":
        this.copy(outputNode);
        break;
      case "cmd_selectAll":
        this.selectAll(outputNode);
        break;
    }
  }
};

///////////////////////////////////////////////////////////////////////////////
// HUDConsoleObserver
///////////////////////////////////////////////////////////////////////////////

/**
 * HUDConsoleObserver: Observes nsIConsoleService for global consoleMessages,
 * if a message originates inside a contentWindow we are tracking,
 * then route that message to the HUDService for logging.
 */

HUDConsoleObserver = {
  QueryInterface: XPCOMUtils.generateQI(
    [Ci.nsIObserver]
  ),

  init: function HCO_init()
  {
    Services.console.registerListener(this);
    Services.obs.addObserver(this, "xpcom-shutdown", false);
  },

  uninit: function HCO_uninit()
  {
    Services.console.unregisterListener(this);
    Services.obs.removeObserver(this, "xpcom-shutdown");
  },

  observe: function HCO_observe(aSubject, aTopic, aData)
  {
    if (aTopic == "xpcom-shutdown") {
      this.uninit();
      return;
    }

    if (!(aSubject instanceof Ci.nsIScriptError) ||
        !(aSubject instanceof Ci.nsIScriptError2) ||
        !aSubject.outerWindowID) {
      return;
    }

    switch (aSubject.category) {
      // We ignore chrome-originating errors as we only
      // care about content.
      case "XPConnect JavaScript":
      case "component javascript":
      case "chrome javascript":
      case "chrome registration":
      case "XBL":
      case "XBL Prototype Handler":
      case "XBL Content Sink":
      case "xbl javascript":
        return;

      case "CSS Parser":
      case "CSS Loader":
        HUDService.reportPageError(CATEGORY_CSS, aSubject);
        return;

      default:
        HUDService.reportPageError(CATEGORY_JS, aSubject);
        return;
    }
  }
};

/**
 * A WebProgressListener that listens for location changes, to update HUDService
 * state information on page navigation.
 *
 * @constructor
 * @param string aHudId
 *        The HeadsUpDisplay ID.
 */
function ConsoleProgressListener(aHudId)
{
  this.hudId = aHudId;
}

ConsoleProgressListener.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsIWebProgressListener,
                                         Ci.nsISupportsWeakReference]),

  onStateChange: function CPL_onStateChange(aProgress, aRequest, aState,
                                            aStatus)
  {
    if (!(aState & Ci.nsIWebProgressListener.STATE_START)) {
      return;
    }

    let uri = null;
    if (aRequest instanceof Ci.imgIRequest) {
      let imgIRequest = aRequest.QueryInterface(Ci.imgIRequest);
      uri = imgIRequest.URI;
    }
    else if (aRequest instanceof Ci.nsIChannel) {
      let nsIChannel = aRequest.QueryInterface(Ci.nsIChannel);
      uri = nsIChannel.URI;
    }

    if (!uri || !uri.schemeIs("file") && !uri.schemeIs("ftp")) {
      return;
    }

    let outputNode = HUDService.hudReferences[this.hudId].outputNode;

    let chromeDocument = outputNode.ownerDocument;
    let msgNode = chromeDocument.createElementNS(HTML_NS, "html:span");

    // Create the clickable URL part of the message.
    let linkNode = chromeDocument.createElementNS(HTML_NS, "html:span");
    linkNode.appendChild(chromeDocument.createTextNode(uri.spec));
    linkNode.classList.add("hud-clickable");
    linkNode.classList.add("webconsole-msg-url");

    linkNode.addEventListener("mousedown", function(aEvent) {
      this._startX = aEvent.clientX;
      this._startY = aEvent.clientY;
    }, false);

    linkNode.addEventListener("click", function(aEvent) {
      if (aEvent.detail == 1 && aEvent.button == 0 &&
          this._startX == aEvent.clientX && this._startY == aEvent.clientY) {
        let viewSourceUtils = chromeDocument.defaultView.gViewSourceUtils;
        viewSourceUtils.viewSource(uri.spec, null, chromeDocument);
      }
    }, false);

    msgNode.appendChild(linkNode);

    let messageNode = ConsoleUtils.createMessageNode(chromeDocument,
                                                     CATEGORY_NETWORK,
                                                     SEVERITY_LOG,
                                                     msgNode,
                                                     null,
                                                     null,
                                                     uri.spec);

    ConsoleUtils.outputMessageNode(messageNode, this.hudId);
  },

  onLocationChange: function() {},
  onStatusChange: function() {},
  onProgressChange: function() {},
  onSecurityChange: function() {},
};

///////////////////////////////////////////////////////////////////////////
// appName
///////////////////////////////////////////////////////////////////////////

/**
 * Get the app's name so we can properly dispatch app-specific
 * methods per API call
 * @returns Gecko application name
 */
function appName()
{
  let APP_ID = Services.appinfo.QueryInterface(Ci.nsIXULRuntime).ID;

  let APP_ID_TABLE = {
    "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}": "FIREFOX" ,
    "{3550f703-e582-4d05-9a08-453d09bdfdc6}": "THUNDERBIRD",
    "{a23983c0-fd0e-11dc-95ff-0800200c9a66}": "FENNEC" ,
    "{92650c4d-4b8e-4d2a-b7eb-24ecf4f6b63a}": "SEAMONKEY",
  };

  let name = APP_ID_TABLE[APP_ID];

  if (name){
    return name;
  }
  throw new Error("appName: UNSUPPORTED APPLICATION UUID");
}

///////////////////////////////////////////////////////////////////////////
// HUDService (exported symbol)
///////////////////////////////////////////////////////////////////////////

try {
  // start the HUDService
  // This is in a try block because we want to kill everything if
  // *any* of this fails
  var HUDService = new HUD_SERVICE();
}
catch (ex) {
  Cu.reportError("HUDService failed initialization.\n" + ex);
  // TODO: kill anything that may have started up
  // see bug 568665
}
