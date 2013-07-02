<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->


# XUL Migration Guide #

This guide aims to help you migrate a XUL-based add-on to the SDK.

First we'll outline how to decide whether
<a href="dev-guide/guides/xul-migration.html#should-you-migrate">
your add-on is a good candidate for migration</a> via a
[comparison of the benefits and limitations of the SDK versus XUL development](dev-guide/guides/sdk-vs-xul.html).

Next, we'll look at some of the main tasks involved in migrating:

* <a href="dev-guide/guides/xul-migration.html#content-scripts">
working with content scripts</a>
* <a href="dev-guide/guides/xul-migration.html#supported-apis">
using the SDK's supported APIs</a>
* how to
go beyond the supported APIs when necessary, by:
    * <a href="dev-guide/guides/xul-migration.html#third-party-packages">
using third party modules</a>
    * <a href="dev-guide/guides/xul-migration.html#low-level-apis">
using the SDK's low-level APIs</a>
    * <a href="dev-guide/guides/xul-migration.html#xpcom">
getting direct access to XPCOM</a>

Finally, we'll walk through a
<a href="dev-guide/guides/xul-migration.html#library-detector">
simple example</a>.

## <a name="should-you-migrate">Should You Migrate?</a> ##

See this [comparison of the benefits and limitations of SDK development
and XUL development](dev-guide/guides/sdk-vs-xul.html).

Whether you should migrate a particular add-on is largely a matter of
how well the SDK's
<a href="dev-guide/guides/xul-migration.html#supported-apis">
supported APIs</a> meet its needs.

* If your add-on can accomplish everything it needs using only the
supported APIs, it's a good candidate for migration.

* If your add-on needs a lot of help from third party packages, low-level
APIs, or XPCOM, then the cost of migrating is high, and may not be worth
it at this point.

* If your add-on only needs a little help from those techniques, and can
accomplish most of what it needs using the supported APIs, then it might
still be worth migrating: we'll add more supported APIs in future releases
to meet important use cases.

## <a name="content-scripts">Content Scripts</a> ##

In a XUL-based add-on, code that uses XPCOM objects, code that manipulates
the browser chrome, and code that interacts with web pages all runs in the
same context. But the SDK makes a distinction between:

* **add-on scripts**, which can use the SDK APIs, but are not able to interact
with web pages
* **content scripts**, which can access web pages, but do not have access to
the SDK's APIs

Content scripts and add-on scripts communicate by sending each other JSON
messages: in fact, the ability to communicate with the add-on scripts is the
only extra privilege a content script is granted over a normal remote web
page script.

A XUL-based add-on will need to be reorganized to respect this distinction.

Suppose an add-on wants to make a cross-domain XMLHttpRequest based on some
data extracted from a web page. In a XUL-based extension you would implement
all this in a single script. An SDK-based equivalent would need to be
structured like this:

* the main add-on code (1) attaches a content script to the page, and (2)
registers a listener function for messages from the content script
* the content script (3) extracts the data from the page and (4) sends
it to the main add-on code in a message
* the main add-on code (5) receives the message and (6) sends the request,
using the SDK's [`request`](modules/sdk/request.html) API

<img class="image-center" src="static-files/media/xul-migration-cs.png"
alt="Content script organization">

There are two related reasons for this design. The first is security: it
reduces the risk that a malicious web page will be able to access privileged
APIs. The second is the need to be compatible with the multi-process architecture
planned for Firefox: after this is implemented in Firefox, all add-ons will
need to use a similar pattern, so it's likely that a XUL-based add-on will
need to be rewritten anyway.

There's much more information on content scripts in the
[Working With Content Scripts](dev-guide/guides/content-scripts/index.html) guide.

## <a name="supported-apis">Using the Supported APIs</a> ##

The SDK provides a set of high level APIs
providing some basic user interface components and functionality commonly
required by add-ons. Because we expect to keep these APIs compatible as new versions
of Firefox are released, we call them the "supported" APIs.

See the [tutorials](dev-guide/tutorials/index.html)
and the [High-Level API reference](modules/high-level-modules.html).
If the supported APIs do what you need, they're the best option: you get the
benefits of compatibility across Firefox releases and of the SDK's security
model.

APIs like [`widget`](modules/sdk/widget.html) and
[`panel`](modules/sdk/panel.html) are very generic and with the
right content can be used to replace many specific XUL elements. But there are
some notable limitations in the SDK APIs and even a fairly simple UI may need
some degree of redesign to work with them.

Some limitations are the result of intentional design choices. For example,
widgets always appear by default in the
[add-on bar](https://developer.mozilla.org/en/The_add-on_bar) (although users
may relocate them by
[toolbar customization](http://support.mozilla.com/en-US/kb/how-do-i-customize-toolbars))
because it makes for a better user experience for add-ons to expose their
interfaces in a consistent way. In such cases it's worth considering
changing your user interface to align with the SDK APIs.

Some limitations only exist because we haven't yet implemented the relevant
APIs: for example, there's currently no way to add items to the browser's main
menus using the SDK's supported APIs.

Many add-ons will need to make some changes to their user interfaces if they
are to use only the SDK's supported APIs, and add-ons which make drastic
changes to the browser chrome will very probably need more than the SDK's
supported APIs can offer.

Similarly, the supported APIs expose only a small fraction of the full range
of XPCOM functionality.

## <a name="third-party-packages">Using Third Party Packages</a> ##

The SDK is extensible by design: developers can create new modules filling gaps
in the SDK, and package them for distribution and reuse. Add-on developers can
install these packages and use the new modules.

If you can find a third party package that does what you want, this is a great
way to use features not supported in the SDK without having to use the
low-level APIs.

See the
[guide to adding Firefox menu items](dev-guide/tutorials/adding-menus.html).
Some useful third party packages are
[collected in the Jetpack Wiki](https://wiki.mozilla.org/Jetpack/Modules).

Note, though, that by using third party packages you're likely to lose the
security and compatibility benefits of using the SDK.

## <a name="low-level-apis">Using the Low-level APIs</a> ##

<span class="aside">
But note that unlike the supported APIs, low-level APIs do not come with a
compatibility guarantee, so we do not expect code using them will necessarily
continue to work as new versions of Firefox are released.
</span>

In addition to the High-Level APIs, the SDK includes a number of
[Low-Level APIs](modules/low-level-modules.html) some of which, such
[`xhr`](modules/sdk/net/xhr.html) and
[`window/utils`](modules/sdk/window/utils.html), expose powerful
browser capabilities.

In this section we'll use low-level modules how to:

* modify the browser chrome using dynamic manipulation of the DOM
* directly access the [tabbrowser](https://developer.mozilla.org/en/XUL/tabbrowser)
object

### <a name="browser-chrome">Modifying the Browser Chrome</a> ###

The [`window/utils`](modules/sdk/window/utils.html) module gives
you direct access to chrome windows, including the browser's chrome window.
Here's a really simple example add-on that modifies the browser chrome using
`window/utils`:

    function removeForwardButton() {
      var window = require("sdk/window/utils").getMostRecentBrowserWindow();
      var forward = window.document.getElementById('forward-button');
      var parent = window.document.getElementById('unified-back-forward-button');
      parent.removeChild(forward);
    }

    var widgets = require("sdk/widget");

    widgets.Widget({
      id: "remove-forward-button",
      label: "Remove the forward button",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        removeForwardButton();
      }
    });

There are more useful examples of this technique in the Jetpack Wiki's
collection of [third party modules](https://wiki.mozilla.org/Jetpack/Modules).

### <a name="accessing-tabbrowser">Accessing <a href="https://developer.mozilla.org/en/XUL/tabbrowser">tabbrowser</a> ###


The [`tabs/utils`](modules/sdk/tabs/utils.html) module gives
you direct access to the
[tabbrowser](https://developer.mozilla.org/en/XUL/tabbrowser) object and the
XUL tab objects it contains. This simple example modifies the selected tab's
CSS to enable the user to highlight the selected tab:

    var widgets = require("sdk/widget");
    var tabUtils = require("sdk/tabs/utils");
    var self = require("sdk/self");

    function highlightTab(tab) {
      if (tab.style.getPropertyValue('background-color')) {
        tab.style.setProperty('background-color','','important');
      }
      else {
        tab.style.setProperty('background-color','rgb(255,255,100)','important');
      }
    }

    var widget = widgets.Widget({
      id: "tab highlighter",
      label: "Highlight tabs",
      contentURL: self.data.url("highlight.png"),
      onClick: function() {
        var window = require("sdk/window/utils").getMostRecentBrowserWindow();
        highlightTab(tabUtils.getActiveTab(window));
      }
    });

### Security Implications ###

The SDK implements a security model in which an add-on only gets to access the
APIs it explicitly imports via `require()`. This is useful, because it means
that if a malicious web page is able to inject code into your add-on's
context, it is only able to use the APIs you have imported. For example, if
you have only imported the
[`notifications`](modules/sdk/notifications.html) module, then
even if a malicious web page manages to inject code into your add-on, it
can't use the SDK's [`file`](modules/sdk/io/file.html) module to
access the user's data.

But this means that the more powerful modules you `require()`, the greater
is your exposure if your add-on is compromised. Low-level modules like `xhr`,
`tab-browser` and `window-utils` are much more powerful than the modules in
`addon-kit`, so your add-on needs correspondingly more rigorous security
design and review.

## <a name="xpcom">Using XPCOM</a> ##

Finally, if none of the above techniques work for you, you can use the
`require("chrome")` statement to get direct access to the
[`Components`](https://developer.mozilla.org/en/Components_object) object,
which you can then use to load and access any XPCOM object.

The following complete add-on uses
[`nsIPromptService`](https://developer.mozilla.org/en/XPCOM_Interface_Reference/nsIPromptService)
to display an alert dialog:

    var {Cc, Ci} = require("chrome");

    var promptSvc = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                    getService(Ci.nsIPromptService);

    var widget = require("sdk/widget").Widget({
      id: "xpcom example",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        promptSvc.alert(null, "My Add-on", "Hello from XPCOM");
      }
    });

It's good practice to encapsulate code which uses XPCOM by
[packaging it in its own module](dev-guide/tutorials/reusable-modules.html).
For example, we could package the alert feature implemented above using a
script like:

    var {Cc, Ci} = require("chrome");

    var promptSvc = Cc["@mozilla.org/embedcomp/prompt-service;1"].
                getService(Ci.nsIPromptService);

    exports.alert = function(title, text) {
        promptSvc.alert(null, title, text);
    };

If we save this as "alert.js" in our add-on's `lib` directory, we can rewrite
`main.js` to use it as follows:

    var widget = require("sdk/widget").Widget({
      id: "xpcom example",
      label: "Mozilla website",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        require("./alert").alert("My Add-on", "Hello from XPCOM");
      }
    });

One of the benefits of this is that we can control which parts of the add-on
are granted chrome privileges, making it easier to review and secure the code.

### Security Implications ###

We saw above that using powerful low-level modules like `tab-browser`
increases the damage that a malicious web page could do if it were able to
inject code into your add-ons context. This applies with even greater force
to `require("chrome")`, since this gives full access to the browser's
capabilities.

## <a name="library-detector">Example: Porting the Library Detector</a> ##

[Porting the Library Detector](dev-guide/guides/library-detector.html)
walks through the process of porting a XUL-based add-on to the
SDK. It's a very simple add-on and a good candidate for porting because
there are suitable SDK APIs for all its features.

Even so, we have to change its user interface slightly if we are to use only
the supported APIs.
