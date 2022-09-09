# Preferences

This documentation aims at giving an overview of the preferences API used in DevTools, it
is not an actual documentation about the list of preferences available in DevTools.

## Overview

Preferences allows you to save and read strings, numbers, booleans to the preferences
store, which is tied to a profile. A preference can also have a default value.

The technical solution for handling preferences differs depending whether you are
testing DevTools as Firefox panel, or a standalone tool running with Launchpad.

## Preference types

DevTools relies on nsIPrefBranch for preferences, which supports different types of
preferences:
* `Int`
* `Boolean`
* `Char`
* `String`

Choose the appropriate type depending on the data you need to store. If you need to store
a JavaScript object or array, the recommended way is to:
* use a `String` type preference
* use JSON.stringify to save
* use JSON.parse to read

Note that nsIPrefBranch also supports a `Complex` type, but this type is not supported
when running in Launchpad.

## Reading and updating preferences

### API docs for nsIPrefBranch and nsIPrefService

DevTools relies on Services.pref to handle preferences. You can access the API docs for
this service at:
* [Source for nsIPrefBranch](https://searchfox.org/mozilla-central/source/modules/libpref/nsIPrefBranch.idl)
* [Source for nsIPrefService](https://searchfox.org/mozilla-central/source/modules/libpref/nsIPrefService.idl)

If you are using Launchpad, note that only a subset of nsIPrefService methods are
implemented (addObserver and removeObserver). Launchpad relies on a Services shim file
provided by devtools-module ([code on GitHub](https://github.com/firefox-devtools/devtools-core/blob/master/packages/devtools-modules/src/Services.js)).

### Services.pref.get* and Services.pref.set*

The main APIs you will have to know and use are getters and setters.
* `Services.pref.getIntPref(prefName, defaultValue);` This method will throw if the
preference cannot be found and you didn't pass a default value!
* `Services.pref.setIntPref(prefName, prefValue)` This method will throw if the provided
value does not match the preference type!

These APIs are very similar for each preference type.

## Create a new preference

Debugger-specific preferences should go in
devtools/client/preferences/debugger.js. Beyond that, most new preferences
should go in browser/app/profile/firefox.js, which is for desktop Firefox only.
If a preference should be available even when the client for DevTools is not
shipped (for instance on Fennec) it should go in modules/libpref/init/all.js,
which is for preferences that go in all products.

### Projects using Launchpad

At the time of writing this doc, projects using Launchpad have to duplicate the default
definition of a preference.
* debugger.html: update [src/utils/prefs.js](https://github.com/firefox-devtools/debugger.html/blob/master/src/utils/prefs.js)
* netmonitor: update [index.js](http://searchfox.org/mozilla-central/source/devtools/client/netmonitor/index.js)
* webconsole: update [local-dev/index.js](http://searchfox.org/mozilla-central/source/devtools/client/webconsole/local-dev/index.js)

## Inspect preferences

Depending on the project you are working on, preferences are stored differently but can
always be inspected.

In Firefox, you can open a tab to about:config and search by preference name.

In Launchpad, preferences are actually saved to localStorage. Open DevTools on your
Launchpad application and inspect the local storage content. You should see entries
prefixed by `Services.prefs:`. You will only see preferences where a user-specific value
has overridden the default value.
