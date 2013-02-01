<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `frame/utils` module provides helper functions for working with platform
internals like [frames](https://developer.mozilla.org/en/XUL/iframe) and
[browsers](https://developer.mozilla.org/en/XUL/browser).

Module exports `create` function that takes the `nsIDOMDocument` of a
[privileged document](https://developer.mozilla.org/en/Working_with_windows_in_chrome_code)
and creates a `browser` element in its `documentElement`:

    let { open } = require('sdk/window/utils');
    let { create } = require('sdk/frame/utils');
    let window = open('data:text/html,Foo');
    let frame = create(window.document);

Optionally `create` can be passed set of `options` to configure created frame
even further.

Execution of scripts may easily be enabled:

    let { open } = require('sdk/window/utils');
    let { create } = require('sdk/frame/utils');
    let window = open('data:text/html,top');
    let frame = create(window.document, {
      uri: 'data:text/html,<script>alert("Hello")</script>',
      allowJavascript: true
    });

<api name="create">
@function
  Creates a XUL `browser` element in a privileged document.
   @param document {nsIDOMDocument}
   @param options {object}
    Options for the new frame, with the following properties:
     @prop type {String}
       String that defines access type of the document loaded into it. Defaults to
       `"content"`. For more details and other possible values see
       [documentation on MDN](https://developer.mozilla.org/en/XUL/Attribute/browser.type)
     @prop uri {String}
      URI of the document to be loaded into the new frame. Defaults to `about:blank`.
     @prop remote {Boolean}
      If `true` separate process will be used for this frame, and
      all the following options are ignored.
     @prop allowAuth {Boolean}
      Whether to allow auth dialogs. Defaults to `false`.
     @prop allowJavascript {Boolean}
      Whether to allow Javascript execution. Defaults to `false`.
     @prop allowPlugins {Boolean}
      Whether to allow plugin execution. Defaults to `false`.
@returns {frame}
The new [`browser`](https://developer.mozilla.org/en-US/docs/XUL/browser)
element.
</api>
