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

interface Document;
interface Blob;
interface FormData;
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

[Constructor(optional MozXMLHttpRequestParameters params)]
interface XMLHttpRequest : XMLHttpRequestEventTarget {
  // event handler
  [TreatNonCallableAsNull, GetterInfallible=MainThread]
  attribute Function? onreadystatechange;

  // states
  const unsigned short UNSENT = 0;
  const unsigned short OPENED = 1;
  const unsigned short HEADERS_RECEIVED = 2;
  const unsigned short LOADING = 3;
  const unsigned short DONE = 4;

  [Infallible]
  readonly attribute unsigned short readyState;

  // request
  void open(DOMString method, DOMString url, optional boolean async = true,
            optional DOMString? user, optional DOMString? password);
  void setRequestHeader(DOMString header, DOMString value);

  [GetterInfallible]
  attribute unsigned long timeout;

  [GetterInfallible, SetterInfallible=MainThread]
  attribute boolean withCredentials;

  [Infallible=MainThread]
  readonly attribute XMLHttpRequestUpload upload;

  void send();
  void send(ArrayBuffer data);
  void send(Blob data);
  void send(Document data);
  void send(DOMString? data);
  void send(FormData data);
  void send(InputStream data);

  [Infallible=MainThread]
  void abort();

  // response
  [Infallible=MainThread]
  readonly attribute unsigned short status;

  [Infallible]
  readonly attribute DOMString statusText;
  DOMString? getResponseHeader(DOMString header);

  [Infallible=MainThread]
  DOMString getAllResponseHeaders();

  [Infallible=MainThread]
  void overrideMimeType(DOMString mime);

  [GetterInfallible]
  attribute XMLHttpRequestResponseType responseType;
  readonly attribute any response;
  readonly attribute DOMString? responseText;

  [GetterInfallible=Workers]
  readonly attribute Document? responseXML;

  // Mozilla-specific stuff
  [GetterInfallible, SetterInfallible=MainThread]
  attribute boolean multipart;

  [GetterInfallible, SetterInfallible=MainThread]
  attribute boolean mozBackgroundRequest;

  [ChromeOnly, GetterInfallible]
  readonly attribute MozChannel channel;

  void sendAsBinary(DOMString body);
  any getInterface(IID iid);

  [Infallible]
  readonly attribute boolean mozAnon;

  [Infallible]
  readonly attribute boolean mozSystem;
};
