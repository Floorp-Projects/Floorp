/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * The origin of this IDL file is
 * www.w3.org/TR/2012/WD-XMLHttpRequest-20120117/
 *
 * Copyright © 2012 W3C® (MIT, ERCIM, Keio), All Rights Reserved. W3C
 * liability, trademark and document use rules apply.
 */

interface Blob;
interface InputStream;
interface MozChannel;
interface IID;

enum XMLHttpRequestResponseType {
  "",
  "arraybuffer",
  "blob",
  "document",
  "json",
  "text",

  // Mozilla-specific stuff
  "moz-chunked-text",
  "moz-chunked-arraybuffer",
  "moz-blob"
};

/**
 * Parameters for instantiating an XMLHttpRequest. They are passed as an
 * optional argument to the constructor:
 *
 *  new XMLHttpRequest({anon: true, system: true});
 */
dictionary MozXMLHttpRequestParameters
{
  /**
   * If true, the request will be sent without cookie and authentication
   * headers.
   */
  boolean mozAnon = false;

  /**
   * If true, the same origin policy will not be enforced on the request.
   */
  boolean mozSystem = false;
};

[Constructor(optional MozXMLHttpRequestParameters params),
 // There are apparently callers, specifically CoffeeScript, who do
 // things like this:
 //   c = new(window.ActiveXObject || XMLHttpRequest)("Microsoft.XMLHTTP")
 // To handle that, we need a constructor that takes a string.
 Constructor(DOMString ignored)]
interface XMLHttpRequest : XMLHttpRequestEventTarget {
  // event handler
  [SetterThrows=Workers, GetterThrows=Workers]
  attribute EventHandler onreadystatechange;

  // states
  const unsigned short UNSENT = 0;
  const unsigned short OPENED = 1;
  const unsigned short HEADERS_RECEIVED = 2;
  const unsigned short LOADING = 3;
  const unsigned short DONE = 4;

  readonly attribute unsigned short readyState;

  // request
  [Throws]
  void open(ByteString method, DOMString url, optional boolean async = true,
            optional DOMString? user, optional DOMString? password);
  [Throws]
  void setRequestHeader(ByteString header, ByteString value);

  [SetterThrows]
  attribute unsigned long timeout;

  [SetterThrows]
  attribute boolean withCredentials;

  [Throws=Workers]
  readonly attribute XMLHttpRequestUpload upload;

  [Throws]
  void send();
  [Throws]
  void send(ArrayBuffer data);
  [Throws]
  void send(ArrayBufferView data);
  [Throws]
  void send(Blob data);
  [Throws]
  void send(Document data);
  [Throws]
  void send(DOMString? data);
  [Throws]
  void send(FormData data);
  [Throws]
  void send(InputStream data);

  [Throws=Workers]
  void abort();

  // response
  [Throws=Workers]
  readonly attribute unsigned short status;

  readonly attribute ByteString statusText;
  [Throws]
  ByteString? getResponseHeader(ByteString header);

  [Throws=Workers]
  ByteString getAllResponseHeaders();

  [Throws=Workers]
  void overrideMimeType(DOMString mime);

  [SetterThrows]
  attribute XMLHttpRequestResponseType responseType;
  [Throws]
  readonly attribute any response;
  [Throws]
  readonly attribute DOMString? responseText;

  [Throws=MainThread]
  readonly attribute Document? responseXML;

  // Mozilla-specific stuff

  [SetterThrows=Workers]
  attribute boolean mozBackgroundRequest;

  [ChromeOnly]
  readonly attribute MozChannel? channel;

  [Throws]
  void sendAsBinary(DOMString body);
  [Throws, ChromeOnly]
  any getInterface(IID iid);

  readonly attribute boolean mozAnon;
  readonly attribute boolean mozSystem;
};
