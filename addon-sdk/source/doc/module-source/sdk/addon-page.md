<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

With the Add-on SDK you can present information to the user,
such as a guide to using your add-on, in a browser tab.
You can supply the content in an HTML file in your add-on's
"data" directory.

***Note:*** This module has no effect on Fennec.

For pages like this, navigational elements such as the
[Awesome Bar](http://support.mozilla.org/en-US/kb/Location%20bar%20autocomplete),
[Search Bar](http://support.mozilla.org/en-US/kb/Search%20bar), or
[Bookmarks Toolbar](http://support.mozilla.org/en-US/kb/Bookmarks%20Toolbar)
are not usually relevant and distract from the content
you are presenting. The `addon-page` module provides a simple
way to have a page which excludes these elements.

To use the module import it using `require()`. After this,
the page loaded from "data/index.html" will not contain
navigational elements:

    var addontab = require("sdk/addon-page");
    var data = require("sdk/self").data;

    require("sdk/tabs").open(data.url("index.html"));

<img src="static-files/media/screenshots/addon-page.png" alt="Example add-on page" class="image-center"/>
This only affects the page at "data/index.html":
all other pages are displayed normally.

