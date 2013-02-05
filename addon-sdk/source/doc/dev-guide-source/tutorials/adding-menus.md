<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Add a Menu Item to Firefox #

<span class="aside">
To follow this tutorial you'll need to have
[installed the SDK](dev-guide/tutorials/installation.html)
and learned the
[basics of `cfx`](dev-guide/tutorials/getting-started-with-cfx.html).
</span>

The SDK doesn't yet provide an API to add new menu items to Firefox.
But it's extensible by design, so anyone can build and publish
modules for add-on developers to use. Luckily, Erik Vold has written
a [`menuitems`](https://github.com/erikvold/menuitems-jplib) module
that enables us to add menu items.

This tutorial does double-duty. It describes the general method for
using an external, third-party module in your add-on, and it
describes how to add a menu item using the `menuitems` module in particular.

## Installing `menuitems` ##

First we'll download the `menuitems` package from
[https://github.com/erikvold/menuitems-jplib](https://github.com/erikvold/menuitems-jplib/zipball/51080383cbb0fe2a05f8992a8aae890f4c014176).

Third-party packages like `menuitems` can be installed in three
different places:

* in the `packages` directory under the SDK root. If you do this the package
is available to any other add-ons you're developing using that SDK instance,
and the package's documentation is visible through `cfx docs`.
* in a `packages` directory you create under your add-on's root: if you
do this, the package is only available to that add-on.
* in a directory indicated using the `packages` key in
your add-on's [package.json](dev-guide/package-spec.html). If you
do this, you may not keep any packages in your add-on's `packages`
directory, or they will not be found.

In this example we will install the package under the SDK root. From
the SDK root directory, execute something like the following commands:

<pre>
cd packages
tar -xf ../erikvold-menuitems-jplib-d80630c.zip
</pre>

Now if you run `cfx docs` you'll see a new section appear in the sidebar
labeled "Third-Party APIs", which lists the modules in the `menuitems`
package: this package contains a single module, also
called `menuitems`.

Click on the module name and you'll see API documentation for the module.

## Module Dependencies ##

If third-party modules only depend on SDK modules, you can use them right
away, but if they depend on other third-party modules, you'll have to install
those dependencies as well.

In the package's main directory you'll find a file called "package.json".
Open it up and look for an entry named "dependencies". The entry for the
`menuitems` package is:

<pre>
"dependencies": ["vold-utils"]
</pre>

This tells us that we need to install the `vold-utils` package,
which we can do by downloading it from
[https://github.com/erikvold/vold-utils-jplib](https://github.com/voldsoftware/vold-utils-jplib/zipball/1b2ad874c2d3b2070a1b0d43301aa3731233e84f)
and adding it under the `packages` directory alongside `menuitems`.

## Using `menuitems` ##

We can use the `menuitems` module in exactly the same way we use built-in
modules.

The documentation for the `menuitems` module tells us to we create a menu
item using `MenuItem()`. Of the options accepted by `MenuItem()`, we'll use
this minimal set:

* `id`: identifier for this menu item
* `label`: text the item displays
* `command`: function called when the user selects the item
* `menuid`: identifier for the item's parent element
* `insertbefore`: identifier for the item before which we want our item to
appear

Next, create a new add-on. Make a directory called 'clickme' wherever you
like, navigate to it and run `cfx init`. Open `lib/main.js` and add the
following code:

    var menuitem = require("menuitems").Menuitem({
      id: "clickme",
      menuid: "menu_ToolsPopup",
      label: "Click Me!",
      onCommand: function() {
        console.log("clicked");
      },
      insertbefore: "menu_pageInfo"
    });

Next, we have to declare our dependency on the `menuitems` package.
In your add-on's `package.json` add the line:

<pre>
"dependencies": "menuitems"
</pre>

Note that due to
[bug 663480](https://bugzilla.mozilla.org/show_bug.cgi?id=663480), if you
add a `dependencies` line to `package.json`, and you use any modules from
the SDK, then you must also declare your dependency on that built-in package,
like this:

<pre>
"dependencies": ["menuitems", "addon-sdk"]
</pre>

Now we're done. Run the add-on and you'll see the new item appear in the
`Tools` menu: select it and you'll see `info: clicked` appear in the
console.

## Caveats ##

Eventually we expect the availability of a rich set of third party packages
will be one of the most valuable aspects of the SDK. Right now they're a great
way to use features not supported by the supported APIs without the
complexity of using the low-level APIs, but there are some caveats you should
be aware of:

* our support for third party packages is still fairly immature. One
consequence of this is that it's not always obvious where to find third-party
packages, although the
[Community Developed Modules](https://github.com/mozilla/addon-sdk/wiki/Community-developed-modules)
page in the SDK's GitHub Wiki lists a number of packages.

* because third party modules typically use low-level APIs, they may be broken
by new releases of Firefox.
