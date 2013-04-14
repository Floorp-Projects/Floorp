<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Atul Varma [atul@mozilla.com]  -->
<!-- edited by Noelle Murata [fiveinchpixie@gmail.com]  -->


The `url` module provides functionality for the parsing and retrieving of URLs.

<api name="URL">
@class
<api name="URL">
@constructor
  The URL constructor creates an object that represents a URL,  verifying that
  the provided string is a valid URL in the process.  Any API in the SDK which
  has a URL parameter will accept `URL` objects, not raw strings, unless
  otherwise noted.

@param source {string}
  A string to be converted into a URL. If `source` is not a valid URI, this
  constructor will throw an exception.

@param [base] {string}
  An optional string used to resolve relative `source` URLs into absolute ones.
</api>

<api name="scheme">
@property {string}
  The name of the protocol in the URL.
</api>

<api name="userPass">
@property {string}
  The username:password part of the URL, `null` if not present.
</api>

<api name="host">
@property {string}
  The host of the URL, `null` if not present.
</api>

<api name="port">
@property {integer}
  The port number of the URL, `null` if none was specified.
</api>

<api name="path">
@property {string}
  The path component of the URL.
</api>

<api name="toString">
@method
  Returns a string representation of the URL.
@returns {string}
  The URL as a string.
</api>
</api>

<api name="toFilename">
@function
  Attempts to convert the given URL to a native file path.  This function will
  automatically attempt to resolve non-file protocols, such as the `resource:`
  protocol, to their place on the file system. An exception is raised if the URL
  can't be converted; otherwise, the native file path is returned as a string.

@param url {string}
  The URL, as a string, to be converted.

@returns {string}
  The converted native file path as a string.
</api>

<api name="fromFilename">
@function
  Converts the given native file path to a `file:` URL.

@param path {string}
  The native file path, as a string, to be converted.

@returns {string}
  The converted URL as a string.
</api>

<api name="isValidURI">
@function
  Checks the validity of a URI. `isValidURI("http://mozilla.org")` would return `true`, whereas `isValidURI("mozilla.org")` would return `false`.

@param uri {string}
  The URI, as a string, to be tested.

@returns {boolean}
  A boolean indicating whether or not the URI is valid.
</api>

<api name="DataURL">
@class
<api name="DataURL">
@constructor
  The DataURL constructor creates an object that represents a data: URL,
  verifying that the provided string is a valid data: URL in the process.

@param uri {string}
  A string to be parsed as Data URL. If is not a valid URI, this constructor
  will throw an exception.
</api>

<api name="mimeType">
@property {string}
  The MIME type of the data. By default is an empty string.
</api>

<api name="parameters">
@property {object}
  An hashmap that contains the parameters of the Data URL. By default is
  an empty object.
</api>

<api name="base64">
@property {boolean}
  Defines the encoding for the value in `data` property.
</api>

<api name="data">
@property {string}
  The string that contains the data in the Data URL.
  If the `uri` given to the constructor contains `base64` parameter, this string is decoded.
</api>

<api name="toString">
@method
  Returns a string representation of the Data URL.
  If `base64` is `true`, the `data` property is encoded using base-64 encoding.
@returns {string}
  The URL as a string.
</api>
</api>
