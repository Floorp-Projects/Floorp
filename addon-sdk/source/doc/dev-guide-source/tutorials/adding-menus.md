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

First, create a new add-on. Make a directory called "clickme" wherever you
like, navigate to it and run `cfx init`.

<pre>
mkdir clickme
cd clickme
cfx init
</pre>

The usual directory structure will be created:

<ul class="tree">
  <li>clickme
    <ul>
    <li>data</li>
    <li>docs
      <ul><li>main.md</li></ul>
    </li>
    <li>lib
      <ul><li>main.js</li></ul>
    </li>
    <li>package.json</li>
    <li>README.md</li>
    <li>tests
      <ul><li>test-main.js</li></ul>
    </li>
    </ul>
  </li>
</ul>

<div style="clear:both"></div>

## Installing `menuitems` ##

Create a directory under "clickme" called "packages".
Then download the `menuitems` package from
[https://github.com/erikvold/menuitems-jplib](https://github.com/erikvold/menuitems-jplib/zipball/51080383cbb0fe2a05f8992a8aae890f4c014176) and extract it into the "packages" directory you just created:

<pre>
mkdir packages
cd packages
tar -xf ../erikvold-menuitems-jplib-d80630c.zip
</pre>

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

The [documentation for the `menuitems` module](https://github.com/erikvold/menuitems-jplib/blob/master/docs/menuitems.md)
tells us to create a menu item using `MenuItem()`. Of the options
accepted by `MenuItem()`, we'll use this minimal set:

* `id`: identifier for this menu item
* `label`: text the item displays
* `command`: function called when the user selects the item
* `menuid`: identifier for the item's parent element
* `insertbefore`: identifier for the item before which we want our item to
appear

<!--comment to terminate Markdown list -->

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

Third-party modules are a great way to use features not directly supported by
the SDK, but because third party modules typically use low-level APIs,
they may be broken by new releases of Firefox.
