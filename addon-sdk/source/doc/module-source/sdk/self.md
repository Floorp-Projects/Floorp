<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- edited by Erik Vold [erikvvold@gmail.com]  -->

The `self` module provides access to data that is bundled with the add-on
as a whole. It also provides access to the
[Program ID](dev-guide/guides/program-id.html), a value which is
unique for each add-on.

Note that the `self` module is completely different from the global `self`
object accessible to content scripts, which is used by a content script to
[communicate with the add-on code](dev-guide/guides/content-scripts/using-port.html).

<api name="uri">
@property {string}
This property represents an add-on associated unique URI string.
This URI can be used for APIs which require a valid URI string, such as the
[passwords](modules/sdk/passwords.html) module.
</api>

<api name="id">
@property {string}
This property is a printable string that is unique for each add-on. It comes
from the `id` property set in the `package.json` file in the main package
(i.e. the package in which you run `cfx xpi`). While not generally of use to
add-on code directly, it can be used by internal API code to index local
storage and other resources that are associated with a particular add-on.
Eventually, this ID will be unspoofable (see
[JEP 118](https://wiki.mozilla.org/Labs/Jetpack/Reboot/JEP/118) for details).
</api>

<api name="name">
@property {string}
This property contains the add-on's short name. It comes from the `name`
property in the main package's `package.json` file.
</api>

<api name="version">
@property {string}
This property contains the add-on's version string. It comes from the
`version` property set in the `package.json` file in the main package.
</api>

<api name="loadReason">
@property {string}
This property contains of the following strings describing the reason
your add-on was loaded:

<pre>
install
enable
startup
upgrade
downgrade
</pre>

</api>

<api name="isPrivateBrowsingSupported">
@property {boolean}
This property indicates whether or not the add-on supports private browsing.
It comes from the [`private-browsing` key](dev-guide/package-spec.html#permissions)
in the add-on's `package.json` file.
</api>

<api name="data">
@property {object}
The `data` object is used to access data that was bundled with the add-on.
This data lives in the main package's `data/` directory, immediately below
the `package.json` file. All files in this directory will be copied into the
XPI and made available through the `data` object.

The [Package Specification](dev-guide/package-spec.html)
section explains the `package.json` file.

<api name="data.load">
@method
The `data.load(NAME)` method returns the contents of an embedded data file,
as a string. It is most useful for data that will be modified or parsed in
some way, such as JSON, XML, plain text, or perhaps an HTML template. For
data that can be displayed directly in a content frame, use `data.url(NAME)`.
@param name {string} The filename to be read, relative to the
  package's `data` directory. Each package that uses the `self` module
  will see its own `data` directory.
@returns {string}
</api>

<api name="data.url">
@method
The `data.url(NAME)` method returns a url that points at an embedded
data file. It is most useful for data that can be displayed directly in a
content frame. The url can be passed to a content frame constructor, such
as the Panel:

    var self = require("sdk/self");
    var myPanel = require("sdk/panel").Panel({
      contentURL: self.data.url("my-panel-content.html")
    });
    myPanel.show();

@param name {string} The filename to be read, relative to the
  package's `data` directory. Each package that uses the `self` module
  will see its own `data` directory.
@returns {String}
</api>
</api>


