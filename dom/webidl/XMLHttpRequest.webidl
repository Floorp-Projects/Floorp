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
  [Throws]
  void open(DOMString method, DOMString url, optional boolean async = true,
            optional DOMString? user, optional DOMString? password);
  [Throws]
  void setRequestHeader(DOMString header, DOMString value);

  [GetterInfallible]
  attribute unsigned long timeout;

  [GetterInfallible, SetterInfallible=MainThread]
  attribute boolean withCredentials;

  [Infallible=MainThread]
  readonly attribute XMLHttpRequestUpload upload;

  [Throws]
  void send();
  [Throws]
  void send(ArrayBuffer data);
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
  [Infallible=MainThread]
  readonly attribute unsigned short status;

  [Infallible]
  readonly attribute DOMString statusText;
  [Throws]
  DOMString? getResponseHeader(DOMString header);

  [Throws=Workers]
  DOMString getAllResponseHeaders();

  [Throws=Workers]
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

  [Throws]
  void sendAsBinary(DOMString body);
  [Throws]
  any getInterface(IID iid);

  [Infallible]
  readonly attribute boolean mozAnon;

  [Infallible]
  readonly attribute boolean mozSystem;
};
