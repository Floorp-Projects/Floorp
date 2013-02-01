<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The module provides data encoding and decoding using Base64 algorithms.

##Example

    var base64 = require("sdk/base64");

    var encodedData = base64.encode("Hello, World");
    var decodedData = base64.decode(encodedData);

##Unicode Strings

In order to `encode` and `decode` properly Unicode strings, the `charset`
parameter needs to be set to `"utf-8"`:

    var base64 = require("sdk/base64");

    var encodedData = base64.encode(unicodeString, "utf-8");
    var decodedData = base64.decode(encodedData, "utf-8");

<api name="encode">
@function
  Creates a base-64 encoded ASCII string from a string of binary data.

@param data {string}
  The data to encode
@param [charset] {string}
  The charset of the string to encode (optional).
  The only accepted value is `"utf-8"`.

@returns {string}
  The encoded string
</api>

<api name="decode">
@function
  Decodes a string of data which has been encoded using base-64 encoding.

@param data {string}
  The encoded data
@param [charset] {string}
  The charset of the string to encode (optional).
  The only accepted value is `"utf-8"`.

@returns {string}
  The decoded string
</api>
