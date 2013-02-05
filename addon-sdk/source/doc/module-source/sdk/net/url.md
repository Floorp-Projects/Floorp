<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `net/url` module enables you to read content from a URI.

<api name="readURI">
@function
  Reads a URI and returns a [promise](modules/sdk/core/promise.html).

@param uri {string}
  The URL, as a string, to load.

@param [options] {object}
  `options` can have any or all of the following fields:
  @prop [sync=false] {boolean}
    If this option is set to `true`, the promise returned will be resolved
    synchronously. Defaults to `false` if not provided.
  @prop [charset="UTF-8"] {string}
    The character set to use when read the content of the `uri` given.
    Defaults to `UTF-8` if not provided.
@returns {promise}
  The promise that will be resolved with the content of the URL given.
</api>
