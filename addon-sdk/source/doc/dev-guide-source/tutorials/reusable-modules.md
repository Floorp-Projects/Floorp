<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Creating Reusable Modules #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

With the SDK you don't have to keep all your add-on in a single "main.js"
file. You can split your code into separate modules with clearly defined
interfaces between them. You then import and use these modules from other
parts of your add-on using the `require()` statement, in exactly that same
way that you import core SDK modules like
[`widget`](modules/sdk/widget.html) or
[`panel`](modules/sdk/panel.html).

It can often make sense to structure a larger or more complex add-on as a
collection of modules. This makes the design of the add-on easier to
understand and provides some encapsulation as each module will export only
what it chooses to, so you can change the internals of the module without
breaking its users.

Once you've done this, you can package the modules and distribute them
independently of your add-on, making them available to other add-on developers
and effectively extending the SDK itself.

In this tutorial we'll do exactly that with a module that exposes the
geolocation API in Firefox.

## Using Geolocation in an Add-on ##

Suppose we want to use the
[geolocation API built into Firefox](https://developer.mozilla.org/en/using_geolocation).
The SDK doesn't provide an API to access geolocation, but we can
[access the underlying XPCOM API using `require("chrome")`](dev-guide/guides/xul-migration.html#xpcom).

The following add-on adds a [button to the toolbar](dev-guide/tutorials/adding-toolbar-button.html):
when the user clicks the button, it loads the
[XPCOM nsIDOMGeoGeolocation](https://developer.mozilla.org/en/XPCOM_Interface_Reference/NsIDOMGeoGeolocation)
object, and retrieves the user's current position:

    var {Cc, Ci} = require("chrome");

    // Implement getCurrentPosition by loading the nsIDOMGeoGeolocation
    // XPCOM object.
    function getCurrentPosition(callback) {
      var xpcomGeolocation = Cc["@mozilla.org/geolocation;1"]
                          .getService(Ci.nsIDOMGeoGeolocation);
      xpcomGeolocation.getCurrentPosition(callback);
    }

    var widget = require("sdk/widget").Widget({
      id: "whereami",
      label: "Where am I?",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        getCurrentPosition(function(position) {
          console.log("latitude: ", position.coords.latitude);
          console.log("longitude: ", position.coords.longitude);
        });
      }
    });

Try it out:

* create a new directory called "whereami" and navigate to it
* execute `cfx init`
* open "lib/main.js" and add the code above
* execute `cfx run`, then `cfx run` again

You should see a button added to the "Add-on Bar" at the bottom of
the browser window:

<img class="image-center" src="static-files/media/screenshots/widget-mozilla.png"
alt="Mozilla icon widget" />

Click the button, and after a short delay you should see output like
this in the console:

<pre>
info: latitude:  29.45799999
info: longitude:  93.0785269
</pre>

So far, so good. But the geolocation guide on MDN tells us that we must
[ask the user for permission](https://developer.mozilla.org/en/using_geolocation#Prompting_for_permission)
before using the API.

So we'll extend the add-on to include an adapted version of the code in
that MDN page:

<pre><code>
var activeBrowserWindow = require("sdk/window/utils").getMostRecentBrowserWindow();
var {Cc, Ci} = require("chrome");

// Ask the user to confirm that they want to share their location.
// If they agree, call the geolocation function, passing the in the
// callback. Otherwise, call the callback with an error message.
function getCurrentPositionWithCheck(callback) {
  let pref = "extensions.whereami.allowGeolocation";
  let message = "whereami Add-on wants to know your location."
  let branch = Cc["@mozilla.org/preferences-service;1"]
               .getService(Ci.nsIPrefBranch2);
  if (branch.getPrefType(pref) === branch.PREF_STRING) {
    switch (branch.getCharPref(pref)) {
    case "always":
      return getCurrentPosition(callback);
    case "never":
      return callback(null);
    }
  }
  let done = false;

  function remember(value, result) {
    return function () {
      done = true;
      branch.setCharPref(pref, value);
      if (result) {
        getCurrentPosition(callback);
      }
      else {
        callback(null);
      }
    }
  }

  let self = activeBrowserWindow.PopupNotifications.show(
               activeBrowserWindow.gBrowser.selectedBrowser,
               "geolocation",
               message,
               "geo-notification-icon",
    {
      label: "Share Location",
      accessKey: "S",
      callback: function (notification) {
        done = true;
        getCurrentPosition(callback);
      }
    }, [
      {
        label: "Always Share",
        accessKey: "A",
        callback: remember("always", true)
      },
      {
        label: "Never Share",
        accessKey: "N",
        callback: remember("never", false)
      }
    ], {
      eventCallback: function (event) {
        if (event === "dismissed") {
          if (!done)
            callback(null);
          done = true;
          PopupNotifications.remove(self);
        }
      },
      persistWhileVisible: true
    });
}

// Implement getCurrentPosition by loading the nsIDOMGeoGeolocation
// XPCOM object.
function getCurrentPosition(callback) {
  var xpcomGeolocation = Cc["@mozilla.org/geolocation;1"]
                      .getService(Ci.nsIDOMGeoGeolocation);
  xpcomGeolocation.getCurrentPosition(callback);
}

var widget = require("sdk/widget").Widget({
  id: "whereami",
  label: "Where am I?",
  contentURL: "http://www.mozilla.org/favicon.ico",
  onClick: function() {
    getCurrentPositionWithCheck(function(position) {
      if (!position) {
        console.log("The user denied access to geolocation.");
      }
      else {
       console.log("latitude: ", position.coords.latitude);
       console.log("longitude: ", position.coords.longitude);
      }
    });
  }
});

</code></pre>

This works fine: when we click the button, we get a notification box
asking for permission, and depending on our choice the add-on logs either
the position or an error message.

But the code is now somewhat long and complex, and if we want to do much
more in the add-on, it will be hard to maintain. So let's split the
geolocation code into a separate module.

## Creating a Separate Module ##

### Create `geolocation.js` ###

First create a new file in "lib" called "geolocation.js", and copy
everything except the widget code into this new file.

Next, add the following line somewhere in the new file:

    exports.getCurrentPosition = getCurrentPositionWithCheck;

This defines the public interface of the new module. We export a single
a function to prompt the user for permission and get the current position
if they agree.

So "geolocation.js" should look like this:

<pre><code>
var activeBrowserWindow = require("sdk/window/utils").getMostRecentBrowserWindow();
var {Cc, Ci} = require("chrome");

// Ask the user to confirm that they want to share their location.
// If they agree, call the geolocation function, passing the in the
// callback. Otherwise, call the callback with an error message.
function getCurrentPositionWithCheck(callback) {
  let pref = "extensions.whereami.allowGeolocation";
  let message = "whereami Add-on wants to know your location."
  let branch = Cc["@mozilla.org/preferences-service;1"]
               .getService(Ci.nsIPrefBranch2);
  if (branch.getPrefType(pref) === branch.PREF_STRING) {
    switch (branch.getCharPref(pref)) {
    case "always":
      return getCurrentPosition(callback);
    case "never":
      return callback(null);
    }
  }
  let done = false;

  function remember(value, result) {
    return function () {
      done = true;
      branch.setCharPref(pref, value);
      if (result) {
        getCurrentPosition(callback);
      }
      else {
        callback(null);
      }
    }
  }

  let self = activeBrowserWindow.PopupNotifications.show(
               activeBrowserWindow.gBrowser.selectedBrowser,
               "geolocation",
               message,
               "geo-notification-icon",
    {
      label: "Share Location",
      accessKey: "S",
      callback: function (notification) {
        done = true;
        getCurrentPosition(callback);
      }
    }, [
      {
        label: "Always Share",
        accessKey: "A",
        callback: remember("always", true)
      },
      {
        label: "Never Share",
        accessKey: "N",
        callback: remember("never", false)
      }
    ], {
      eventCallback: function (event) {
        if (event === "dismissed") {
          if (!done)
            callback(null);
          done = true;
          PopupNotifications.remove(self);
        }
      },
      persistWhileVisible: true
    });
}

// Implement getCurrentPosition by loading the nsIDOMGeoGeolocation
// XPCOM object.
function getCurrentPosition(callback) {
  var xpcomGeolocation = Cc["@mozilla.org/geolocation;1"]
                      .getService(Ci.nsIDOMGeoGeolocation);
  xpcomGeolocation.getCurrentPosition(callback);
}

exports.getCurrentPosition = getCurrentPositionWithCheck;
</code></pre>

### Update `main.js` ###

Finally, update "main.js". First add a line to import the new module:

    var geolocation = require("./geolocation");

When importing modules that are not SDK built in modules, it's a good
idea to specify the path to the module explicitly like this, rather than
relying on the module loader to find the module you intended.

Edit the widget's call to `getCurrentPositionWithCheck()` so it calls
the geolocation module's `getCurrentPosition()` function instead:

    geolocation.getCurrentPosition(function(position) {
      if (!position) {

Now "main.js" should look like this:

<pre><code>
var geolocation = require("./geolocation");

var widget = require("sdk/widget").Widget({
  id: "whereami",
  label: "Where am I?",
  contentURL: "http://www.mozilla.org/favicon.ico",
  onClick: function() {
    geolocation.getCurrentPosition(function(position) {
      if (!position) {
        console.log("The user denied access to geolocation.");
      }
      else {
       console.log("latitude: ", position.coords.latitude);
       console.log("longitude: ", position.coords.longitude);
      }
    });
  }
});
</code></pre>

## Packaging the Geolocation Module ##

So far, this is a useful technique for structuring your add-on.
But you can also package and distribute modules independently of
your add-on: then any other add-on developer can download your
module and use it in exactly the same way they use the SDK's built-in
modules.

### Code Changes ###

First we'll make a couple of changes to the code.
At the moment the message displayed in the prompt and the name of
the preference used to store the user's choice are hardcoded:

    let pref = "extensions.whereami.allowGeolocation";
    let message = "whereami Add-on wants to know your location."

Instead we'll use the `self` module to ensure that they are specific
to the add-on:

    var addonName = require("sdk/self").name;
    var addonId = require("sdk/self").id;
    let pref = "extensions." + addonId + ".allowGeolocation";
    let message = addonName + " Add-on wants to know your location."

### Repackaging ###

Next we'll repackage the geolocation module.

* create a new directory called "geolocation", and run `cfx init` in it.
* delete the "main.js" that `cfx` generated, and copy "geolocation.js"
there instead.

### Documentation ###

If you document your modules, people who install your package and
execute `cfx docs` will see the documentation
integrated with the SDK's own documentation.

You can document the geolocation module by creating a file called
"geolocation.md" in your package's "doc" directory. This file is also
written in Markdown, although you can optionally use some
[extended syntax](https://wiki.mozilla.org/Jetpack/SDK/Writing_Documentation#APIDoc_Syntax)
to document APIs.

Try it:

* add a "geolocation.md" under "doc"
* copy your geolocation package under the "packages" directory in the SDK root
* execute `cfx docs`

Once `cfx docs` has finished, you should see a new entry appear in the
sidebar called "Third-Party APIs", which lists the geolocation module.

### Editing "package.json" ###

The "package.json" file in your package's root directory contains metadata
for your package. See the
[package specification](dev-guide/package-spec.html) for
full details. If you intend to distribute the package, this is a good place
to add your name as the author, choose a distribution license, and so on.

## Learning More ##

To see some of the modules people have already developed, see the page of
[community-developed modules](https://github.com/mozilla/addon-sdk/wiki/Community-developed-modules).
To learn how to use third-party modules in your own code, see the
[tutorial on adding menu items](dev-guide/tutorials/adding-menus.html).