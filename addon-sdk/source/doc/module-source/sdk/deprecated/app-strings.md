<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<div class="warning">
<p>The <code>app-strings</code> module is deprecated.</p>
<p>If its functionality is still considered useful it will be added
to the <a href="modules/sdk/l10n.html"><code>l10n</code></a> module,
otherwise it will just be removed.</p>
</div>

The `app-strings` module gives you access to the host application's localized
string bundles (`.properties` files).

The module exports the `StringBundle` constructor function.  To access a string
bundle, construct an instance of `StringBundle`, passing it the bundle's URL:

    var StringBundle = require("app-strings").StringBundle;
    var bundle = StringBundle("chrome://browser/locale/browser.properties");

To get the value of a string, call the object's `get` method, passing it
the name of the string:

    var accessKey = bundle.get("contextMenuSearchText.accesskey");
    // "S" in the en-US locale

To get the formatted value of a string that accepts arguments, call the object's
`get` method, passing it the name of the string and an array of arguments
with which to format the string:

    var searchText = bundle.get("contextMenuSearchText",
                                ["universe", "signs of intelligent life"]);
    // 'Search universe for "signs of intelligent life"' in the en-US locale

To get all strings in the bundle, iterate the object, which returns arrays
of the form [name, value]:

    for (var [name, value] in Iterator(bundle))
      console.log(name + " = " + value);

Iteration
---------

<code>for (var name in bundle) { ... }</code>

Iterate the names of strings in the bundle.

<code>for each (var val in bundle) { ... }</code>

Iterate the values of strings in the bundle.

<code>for (var [name, value] in Iterator(bundle)) { ... }</code>

Iterate the names and values of strings in the bundle.


<api name="StringBundle">
@class
The `StringBundle` object represents a string bundle.
<api name="StringBundle">
@constructor
Creates a StringBundle object that gives you access to a string bundle.
@param url {string} the URL of the string bundle
@returns {StringBundle} the string bundle
</api>
<api name="get">
@method Get the value of the string with the given name.
@param [name] {string} the name of the string to get
@param [args] {array} (optional) strings that replace placeholders in the string
@returns {string} the value of the string
</api>
</api>
