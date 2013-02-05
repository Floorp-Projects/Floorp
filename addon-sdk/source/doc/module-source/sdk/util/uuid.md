<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `uuid` module provides a low level API for generating or parsing
UUIDs.

It exports a single function, uuid().

For more details about UUID representations and what they are used for by the
platform see the MDN documentation for
[JSID](https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIJSID).

## Generate UUID

To generate a new UUID, call `uuid()` with no arguments:

    let uuid = require('sdk/util/uuid').uuid();

## Parsing UUID

To convert a string representation of a UUID to an `nsID`, pass
the string representation to `uuid()`:

    let { uuid } = require('sdk/util/uuid');
    let firefoxUUID = uuid('{ec8030f7-c20a-464f-9b0e-13a3a9e97384}');

<api name="uuid">
@function
  Generate a new [UUID](https://developer.mozilla.org/en-US/docs/Generating_GUIDs),
  or convert a string representation of a UUID to an `nsID`.
  @param stringId {string}
  String representation of a UUID, such as:
<pre>
"8CBC9BF4-4A16-11E2-AEF7-C1A56188709B"
</pre>
  Optional. If this argument is supplied, it will be converted to an `nsID`
  and returned. Otherwise a new `nsID` will be generated and returned.

  @returns {nsID}
  A UUID, represented as an `nsID` object.

</api>
