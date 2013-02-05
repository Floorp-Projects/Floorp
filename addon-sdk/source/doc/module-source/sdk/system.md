<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

The `system` module enables an add-on to get information about the
environment it's running in, access arguments passed to it via the `cfx`
[`--static-args`](dev-guide/cfx-tool.html#arguments) option, and
quit the host application.

## Querying Your Environment ##

Using the `system` module you can access environment
variables (such as `PATH`), find out which operating system your add-on is
running on and get information about the host application (for example,
Firefox or Fennec), such as its version.

    var system = require("sdk/system");
    // PATH environment variable
    console.log(system.env.PATH);
    // operating system
    console.log("platform = " + system.platform);
    // processor architecture
    console.log("architecture = " + system.architecture);
    // compiler used to build host application
    console.log("compiler = " + system.compiler);
    // host application build identifier
    console.log("build = " + system.build);
    // host application UUID
    console.log("id = " + system.id);
    // host application name
    console.log("name = " + system.name);
    // host application version
    console.log("version = " + system.version);
    // host application vendor
    console.log("vendor = " + system.vendor);
    // host application profile directory
    console.log("profile directory = " + system.pathFor("ProfD"));

## Accessing --static-args ##

Static arguments are accessible by name as properties of the
[`staticArgs`](modules/sdk/system.html#staticArgs) property.

    var system = require("sdk/system");
    console.log(system.staticArgs.foo);

## Quit the host application ##

To quit the host application, use the 
[`exit()`](modules/sdk/system.html#exit(code)) function.

    var system = require("sdk/system");
    system.exit();

<api name="staticArgs">
@property {Object}

The JSON object that was passed via the
[`cfx --static-args` option](dev-guide/cfx-tool.html#arguments).

For example, suppose your add-on includes code like this:

    var system = require("sdk/system");
    console.log(system.staticArgs.foo);

If you pass it a static argument named "foo" using `--static-args`, then
the value of "foo" will be written to the console:

<pre>
(addon-sdk)~/my-addons/system > cfx run --static-args="{ \"foo\": \"Hello\" }"
Using binary at '/Applications/Firefox.app/Contents/MacOS/firefox-bin'.
Using profile at '/var/folders/me/DVFDGavr5GDFGDtU/-Tmp-/tmpOCTgL3.mozrunner'.
info: system: Hello
</pre>

</api>

<api name="env">
@property {Object}

This object provides access to environment variables.

You can get the
value of an environment variable by accessing the property with that name:

    var system = require("sdk/system");
    console.log(system.env.PATH);

You can test whether a variable exists by checking whether a property with
that name exists:

    var system = require("sdk/system");
    if ('PATH' in system.env) {
      console.log("PATH is set");
    }

You can set a variable by setting the property:

    var system = require("sdk/system");
    system.env.FOO = "bar";
    console.log(system.env.FOO);

You can unset a variable by deleting the property:

    var system = require("sdk/system");
    delete system.env.FOO;

You **can't** enumerate environment variables.

</api>

<api name="exit">
@function

Quits the host application with the specified `code`.
If `code` is omitted, `exit()` uses the
success code `0`. To exit with failure use `1`.

    var system = require("sdk/system");
    system.exit();

@param code {integer}
  To exit with failure, set this to `1`. To exit with success, omit this
  argument.

</api>

<api name="pathFor">
@function

Firefox enables you to get the path to certain "special" directories,
such as the desktop or the profile directory. This function exposes that
functionality to add-on authors.

For the full list of "special" directories and their IDs, see
["Getting_files in special directories"](https://developer.mozilla.org/en-US/docs/Code_snippets/File_I_O#Getting_files_in_special_directories).

For example:

    // get Firefox profile path
    var profilePath = require('sdk/system').pathFor('ProfD');
    // get OS temp files directory (/tmp)
    var temps = require('sdk/system').pathFor('TmpD');
    // get OS desktop path for an active user (~/Desktop on linux
    // or C:\Documents and Settings\username\Desktop on windows).
    var desktopPath = require('sdk/system').pathFor('Desk');

@param id {String}
  The ID of the special directory.
@returns {String}
  The path to the directory.

</api>

<api name="platform">
@property {String}
The type of operating system you're running on.
This will be one of the values listed as
[OS_TARGET](https://developer.mozilla.org/en-US/docs/OS_TARGET),
converted to lower case.

    var system = require("sdk/system");
    console.log("platform = " + system.platform);
</api>

<api name="architecture">
@property {String}
The type of processor architecture you're running on.
This will be one of: `"arm"``, `"ia32"`, or `"x64"`.

    var system = require("sdk/system");
    console.log("architecture = " + system.architecture);
</api>

<api name="compiler">
@property {String}
The type of compiler used to build the host application.
For example: `"msvc"`, `"n32"`, `"gcc2"`, `"gcc3"`, `"sunc"`, `"ibmc"`

    var system = require("sdk/system");
    console.log("compiler = " + system.compiler);
</api>

<api name="build">
@property {String}
An identifier for the specific build, derived from the build date.
This is useful if you're trying to target individual nightly builds.
See [nsIXULAppInfo's `appBuildID`](https://developer.mozilla.org/en-US/docs/Using_nsIXULAppInfo#Version).

    var system = require("sdk/system");
    console.log("build = " + system.build);
</api>

<api name="id">
@property {String}
The UUID for the host application. For example,
`"{ec8030f7-c20a-464f-9b0e-13a3a9e97384}"` for Firefox.
This has traditionally been in the form
`"{AAAAAAAA-BBBB-CCCC-DDDD-EEEEEEEEEEEE}"` but for some applications it may
be in the form `"appname@vendor.tld"`.


See [nsIXULAppInfo's `ID`](https://developer.mozilla.org/en-US/docs/Using_nsIXULAppInfo#ID).

    var system = require("sdk/system");
    console.log("id = " + system.id);
</api>

<api name="name">
@property {String}
The human-readable name for the host application. For example, "Firefox".

    var system = require("sdk/system");
    console.log("name = " + system.name);

</api>

<api name="version">
@property {String}
The version of the host application.

See [nsIXULAppInfo's `version`](https://developer.mozilla.org/en-US/docs/Using_nsIXULAppInfo#Version).

    var system = require("sdk/system");
    console.log("version = " + system.version);
</api>

<api name="platformVersion">
@property {String}
The version of XULRunner that underlies the host application.

See [nsIXULAppInfo's `platformVersion`](https://developer.mozilla.org/en-US/docs/Using_nsIXULAppInfo#Platform_version).

    var system = require("sdk/system");
    console.log("XULRunner version = " + system.platformVersion);
</api>

<api name="vendor">
@property {String}
The name of the host application's vendor, for example: `"Mozilla"`.

    var system = require("sdk/system");
    console.log("vendor = " + system.vendor);
</api>
