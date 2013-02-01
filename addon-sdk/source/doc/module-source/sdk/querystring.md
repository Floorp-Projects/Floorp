<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

Module exports utility functions for working with query strings.

### stringify

Object may be serialize to a query string via exported `stringify` function:

    querystring.stringify({ foo: 'bar', baz: 4 });  // => 'foo=bar&baz=4'

Optionally `separator` and `assignment` arguments may be passed to
override default `'&'` and`'='` characters:

    querystring.stringify({ foo: 'bar', baz: 4 }, ';', ':');  // => 'foo:bar;baz:4'

### parse

Query string may be deserialized to an object via exported `parse`
function:

    querystring.parse('foo=bar&baz=bla') // =>  { foo: 'bar', baz: 'bla' }

Optionally `separator` and `assignment` arguments may be passed to
override default `'&'` and `'='` characters:

    querystring.parse('foo:bar|baz:bla', '|', ':')  // => { foo: 'bar', baz: 'bla' }

### escape

The escape function used by `stringify` to encodes a string safely
matching RFC 3986 for `application/x-www-form-urlencoded`.

### unescape

The unescape function used by `parse` to decode a string safely.
