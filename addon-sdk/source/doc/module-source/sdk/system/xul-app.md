<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Drew Willcoxon [adw@mozilla.com] -->

The `xul-app` module provides facilities for introspecting the application on
which your program is running.

With the exception of `ids`, each of these properties exposes the attribute of
the same name on the [`nsIXULAppInfo`][nsIXULAppInfo] interface.  For more
information, see the [MDC documentation][].

[nsIXULAppInfo]: http://mxr.mozilla.org/mozilla-central/source/xpcom/system/nsIXULAppInfo.idl
[MDC documentation]: https://developer.mozilla.org/en/nsIXULAppInfo

<api name="ID">
@property {string}
  The GUID of the host application.  For example, for Firefox this value is
  `"{ec8030f7-c20a-464f-9b0e-13a3a9e97384}"`.
</api>

<api name="name">
@property {string}
  The host application name.  The possible values here are:
  
  * `"Firefox"`
  * `"Fennec"`
  * `"Mozilla"`
  * `"SeaMonkey"`
  * `"Sunbird"`
  * `"Thunderbird"`

  "Firefox"` and `"Fennec"` are the most commonly used values.

</api>

<api name="version">
@property {string}
  The host application version.
</api>

<api name="platformVersion">
@property {string}
  The Gecko/XULRunner platform version.
</api>

<api name="ids">
@property {object}
  A mapping of application names to their IDs.  For example,
  `ids["Firefox"] == "{ec8030f7-c20a-464f-9b0e-13a3a9e97384}"`.
</api>

<api name="is">
@function
  Checks whether the host application is the given application.
@param name {string}
  A host application name.
@returns {boolean}
  True if the host application is `name` and false otherwise.
</api>

<api name="isOneOf">
@function
  Checks whether the host application is one of the given applications.
@param names {array}
  An array of host application names.
@returns {boolean}
  True if the host application is one of the `names` and false otherwise.
</api>

<api name="versionInRange">
@function
  Compares a given version to a version range.  See the [MDC documentation](https://developer.mozilla.org/en/Toolkit_version_format#Comparing_versions)
  for details on version comparisons.
@param version {string}
  The version to compare.
@param lowInclusive {string}
  The lower bound of the version range to compare.  The range includes this
  bound.
@param highExclusive {string}
  The upper bound of the version range to compare.  The range does not include
  this bound.
@returns {boolean}
  True if `version` falls in the given range and false otherwise.
</api>
