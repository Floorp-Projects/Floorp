<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `match-pattern` module can be used to test strings containing URLs
against simple patterns.

## Specifying Patterns ##

There are three ways you can specify patterns:

* as an exact match string
* using a wildcard in a string
* using a regular expression

### Exact Matches ###

**A URL** matches only that URL. The URL must start with a scheme, end with a
slash, and contain no wildcards.

<table>

  <colgroup>
    <col width="20%">
    <col width="25%">
    <col width="55%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
    <th>Example non-matching URLs</th>
  </tr>

  <tr>
    <td><code>"http://example.com/"</code></td>
    <td><code>http://example.com/</code></td>
    <td><code>http://example.com</code><br>
        <code>http://example.com/foo</code><br>
        <code>https://example.com/</code><br>
        <code>http://foo.example.com/</code></td>
  </tr>

</table>

### Wildcards ###

**A single asterisk** matches any URL with an `http`, `https`, or `ftp`
scheme. For other schemes like `file`, `resource`, or `data`, use a scheme
followed by an asterisk, as below.

<table>

  <colgroup>
    <col width="20%">
    <col width="25%">
    <col width="55%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
    <th>Example non-matching URLs</th>
  </tr>

  <tr>
    <td><code>"*"</code></td>
    <td><code>http://example.com/</code><br>
        <code>https://example.com/</code><br>
        <code>ftp://example.com/</code><br>
        <code>http://bar.com/foo.js</code><br>
        <code>http://foo.com/</code></td>
    <td><code>file://example.js</code><br>
        <code>resource://me/my-addon/data/file.html</code><br>
        <code>data:text/html,Hi there</code></td>
  </tr>

</table>

**A domain name prefixed with an asterisk and dot** matches any URL of that
domain or a subdomain, using any of `http`, `https`, `ftp`.

<table>

  <colgroup>
    <col width="20%">
    <col width="25%">
    <col width="55%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
    <th>Example non-matching URLs</th>
  </tr>

  <tr>
    <td><code>"*.example.com"</code></td>
    <td><code>http://example.com/</code><br>
        <code>http://foo.example.com/</code><br>
        <code>https://example.com/</code><br>
        <code>http://example.com/foo</code><br>
        <code>ftp://foo.example.com/</code></td>
    <td><code>ldap://example.com</code><br>
        <code>http://example.foo.com/</code></td>
  </tr>

</table>

**A URL followed by an asterisk** matches that URL and any URL prefixed with
the pattern.

<table>

  <colgroup>
    <col width="20%">
    <col width="25%">
    <col width="55%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
    <th>Example non-matching URLs</th>
  </tr>

  <tr>
    <td><code>"https://foo.com/*"</code></td>
    <td><code>https://foo.com/</code><br>
        <code>https://foo.com/bar</code></td>
    <td><code>http://foo.com/</code><br>
        <code>https://foo.com</code><br>
        <code>https://bar.foo.com/</code></td>
  </tr>

</table>

**A scheme followed by an asterisk** matches all URLs with that scheme. To
match local files, use `file://*`, and to match files loaded from your
add-on's [data](modules/sdk/self.html#data) directory, use `resource://`.

<table>

  <colgroup>
    <col width="20%">
    <col width="80%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
  </tr>

  <tr>
    <td><code>"file://*"</code></td>
    <td><code>file://C:/file.html</code><br>
        <code>file:///home/file.png</code></td>
  </tr>

  <tr>
    <td><code>"resource://*"</code></td>
    <td><code>resource://my-addon-at-me-dot-org/my-addon/data/file.html</code></td>
  </tr>

  <tr>
    <td><code>"data:*"</code></td>
    <td><code>data:text/html,Hi there</code></td>
  </tr>

</table>

### Regular Expressions ###

You can specify patterns using a
[regular expression](https://developer.mozilla.org/en/JavaScript/Guide/Regular_Expressions):

    var { MatchPattern } = require("sdk/page-mod/match-pattern");
    var pattern = new MatchPattern(/.*example.*/);

The regular expression is subject to restrictions based on those applied to the
[HTML5 pattern attribute](http://dev.w3.org/html5/spec/common-input-element-attributes.html#attr-input-pattern). In particular:

* The pattern must match the entire value, not just any subset. For example, the
pattern `/moz.*/` will not match the URL `http://mozilla.org`.

* The expression is compiled with the `global`, `ignoreCase`, and `multiline` flags
  disabled. The `MatchPattern` constructor will throw an exception
  if you try to set any of these flags.

<table>

  <colgroup>
    <col width="30%">
    <col width="35%">
    <col width="35%">
  </colgroup>

  <tr>
    <th>Example pattern</th>
    <th>Example matching URLs</th>
    <th>Example non-matching URLs</th>
  </tr>

  <tr>
    <td><code>/.*moz.*/</code></td>
    <td><code>http://foo.mozilla.org/</code><br>
        <code>http://mozilla.org</code><br>
        <code>https://mozilla.org</code><br>
        <code>http://foo.com/mozilla</code><br>
        <code>http://hemozoon.org</code><br>
        <code>mozscheme://foo.org</code><br></td>
    <td><code>http://foo.org</code><br>
  </tr>

  <tr>
    <td><code>/http:\/\/moz.*/</code></td>
    <td><code>http://mozilla.org</code><br>
        <code>http://mozzarella.com</code></td>
    <td><code>https://mozilla.org</code><br>
        <code>http://foo.mozilla.org/</code><br>
        <code>http://foo.com/moz</code></td>
  </tr>

  <tr>
    <td><code>/http.*moz.*/</code><br></td>
    <td><code>http://foo.mozilla.org/</code><br>
        <code>http://mozilla.org</code><br>
        <code>http://hemozoon.org/</code></td>
        <td><code>ftp://http/mozilla.org</code></td>
  </tr>

</table>

## Examples ##

    var { MatchPattern } = require("sdk/page-mod/match-pattern");
    var pattern = new MatchPattern("http://example.com/*");
    console.log(pattern.test("http://example.com/"));       // true
    console.log(pattern.test("http://example.com/foo"));    // true
    console.log(pattern.test("http://foo.com/"));           // false!

<api name="MatchPattern">
@class
<api name="MatchPattern">
@constructor
  This constructor creates match pattern objects that can be used to test URLs.
@param pattern {string}
  The pattern to use.  See Patterns above.
</api>

<api name="test">
@method
  Tests a URL against the match pattern.
@param url {string}
  The URL to test.
@returns {boolean}
  True if the URL matches the pattern and false otherwise.
</api>
</api>
