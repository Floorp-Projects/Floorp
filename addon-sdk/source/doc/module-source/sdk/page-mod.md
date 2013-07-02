<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<!-- contributed by Nickolay Ponomarev [asqueella@gmail.com] -->
<!-- contributed by Myk Melez [myk@mozilla.org] -->
<!-- contributed by Irakli Gozalishvil [gozala@mozilla.com] -->

The `page-mod` module enables you to run scripts in the context of
specific web pages. To use it, you specify:

* one or more scripts to attach. The SDK calls these scripts "content scripts".
* a pattern that a page's URL must match, in order for the script(s)
to be attached to that page.

For example, the following add-on displays an alert whenever the user
visits any page hosted at "mozilla.org":

    var pageMod = require("sdk/page-mod");

    pageMod.PageMod({
      include: "*.mozilla.org",
      contentScript: 'window.alert("Page matches ruleset");'
    });

You can modify the document in your script:

    var pageMod = require("sdk/page-mod");

    pageMod.PageMod({
      include: "*.mozilla.org",
      contentScript: 'document.body.innerHTML = ' +
                     ' "<h1>Page matches ruleset</h1>";'
    });

You can supply the content script(s) in one of two ways:

* as a string literal, or an array of string literals, assigned to the `contentScript` option, as above
* as separate files supplied in your add-on's "data" directory.
In this case files are specified by a URL typically constructed using the
`url()` method of the
[`self` module's `data` object](modules/sdk/self.html#data):

<!-- -->

    var data = require("sdk/self").data;
    var pageMod = require("sdk/page-mod");
    pageMod.PageMod({
      include: "*.mozilla.org",
      contentScriptFile: data.url("my-script.js")
    });

<!-- -->

    var data = require("sdk/self").data;
    var pageMod = require("sdk/page-mod");

    pageMod.PageMod({
      include: "*.mozilla.org",
      contentScriptFile: [data.url("jquery-1.7.min.js"),
                          data.url("my-script.js")]
    });

<div class="warning">
<p>Unless your content script is extremely simple and consists only of a
static string, don't use <code>contentScript</code>: if you do, you may
have problems getting your add-on approved on AMO.</p>
<p>Instead, keep the script in a separate file and load it using
<code>contentScriptFile</code>. This makes your code easier to maintain,
secure, debug and review.</p>
</div>

A page-mod only attaches scripts to documents loaded in tabs. It will not
attach scripts to add-on panels, page-workers, widgets, or Firefox hidden
windows.

To stop a page-mod from making any more modifications, call its `destroy()`
method.

The `PageMod` constructor takes a number of other options to control its
behavior, all documented in detail in the
[API Reference](modules/sdk/page-mod.html#API Reference) section below:

* `contentStyle` or `contentStyleFile` list stylesheets to attach.
* `contentScriptOptions` defines read-only values accessible to content
scripts.
* `attachTo` controls whether to attach scripts to tabs
that were already open when the page-mod was created, and whether to attach
scripts to iframes as well as the topmost document.
* `contentScriptWhen` controls the point during document load at which content
scripts are attached.

For all the details on content scripts, see the
[guide to content scripts](dev-guide/guides/content-scripts/index.html).

## Communicating With Content Scripts ##

Your add-on's "main.js" can't directly access the state of content scripts
you load, but you can communicate between your add-on and its content scripts
by exchanging messages.

To do this, you'll need to listen to the page-mod's `attach` event.
This event is triggered every time the page-mod's content script is attached
to a document. The listener is passed a
[`worker`](modules/sdk/content/worker.html) object that your add-on
can use to send and receive messages.

For example, the following add-on retrieves the HTML content of specific
tags from documents that match the pattern. The main add-on code sends the
desired tag to the content script, and the content script replies by sending
the HTML content of all the elements with that tag.

/lib/main.js:

    var tag = "p";
    var data = require("sdk/self").data;
    var pageMod = require("sdk/page-mod");

    pageMod.PageMod({
      include: "*.mozilla.org",
      contentScriptFile: data.url("element-getter.js"),
      onAttach: function(worker) {
        worker.port.emit("getElements", tag);
        worker.port.on("gotElement", function(elementContent) {
          console.log(elementContent);
        });
      }
    });

/data/element-getter.js:

    self.port.on("getElements", function(tag) {
      var elements = document.getElementsByTagName(tag);
      for (var i = 0; i < elements.length; i++) {
        self.port.emit("gotElement", elements[i].innerHTML);
      }
    });

When the user loads a document hosted at "mozilla.org":

<img class="image-right" alt="page-mod messaging diagram" src="static-files/media/page-mod-messaging.png"/>

* The content script "element-getter.js" is attached to the document
and runs. It adds a listener to the `getElements` message.
* The `attach` event is sent to the "main.js" code. Its event handler sends
the `getElements` message to the content script, and then adds a listener
to the `gotElement` message.
* The content script receives the `getElements` message, retrieves all
elements of that type, and for each element sends a `gotElement` message
containing the element's `innerHTML`.
* The "main.js" code receives each `gotElement` message and logs the
contents.

If multiple documents that match the page-mod's `include` pattern are loaded,
then each document is loaded into its own execution context with its own copy
of the content scripts. In this case the listener assigned to `onAttach`
is called once for each loaded document, and the add-on code will have a
separate worker for each document.

To learn much more about communicating with content scripts, see the
[guide to content scripts](dev-guide/guides/content-scripts/index.html) and in
particular the chapter on
[communicating using `port`](dev-guide/guides/content-scripts/using-port.html).

## Mapping Workers to Tabs ##

The `worker` has a `tab` property which returns the tab associated with
this worker. You can use this to access
the [`tabs API`](modules/sdk/tabs.html) for the tab associated
with a specific document:

    var pageMod = require("sdk/page-mod");
    var tabs = require("sdk/tabs");

    pageMod.PageMod({
      include: ["*"],
      onAttach: function onAttach(worker) {
        console.log(worker.tab.title);
      }
    });

## Destroying Workers ##

Workers generate a `detach` event when their associated document is closed:
that is, when the tab is closed or the tab's location changes. If
you are maintaining a list of workers belonging to a page-mod, you can use
this event to remove workers that are no longer valid.

For example, if you maintain a list of workers attached to a page-mod:

    var workers = [];

    var pageMod = require("sdk/page-mod").PageMod({
      include: ['*'],
      contentScriptWhen: 'ready',
      contentScriptFile: data.url('pagemod.js'),
      onAttach: function(worker) {
        workers.push(worker);
      }
    });

You can remove workers when they are no longer valid by listening to `detach`:

    var workers = [];

    function detachWorker(worker, workerArray) {
      var index = workerArray.indexOf(worker);
      if(index != -1) {
        workerArray.splice(index, 1);
      }
    }

    var pageMod = require("sdk/page-mod").PageMod({
      include: ['*'],
      contentScriptWhen: 'ready',
      contentScriptFile: data.url('pagemod.js'),
      onAttach: function(worker) {
        workers.push(worker);
        worker.on('detach', function () {
          detachWorker(this, workers);
        });
      }
    });


## Attaching Content Scripts to Tabs ##

We've seen that the page-mod API attaches content scripts to documents based
on their URL. Sometimes, though, we don't care about the URL: we just want
to execute a script on demand in the context of a particular tab.

For example, we might want to run a script in the context of the currently
active tab when the user clicks a widget: to block certain content, to
change the font style, or to display the document's DOM structure.

Using the `attach` method of the [`tab`](modules/sdk/tabs.html)
object, you can attach a set of content scripts to a particular tab. The
scripts are executed immediately.

The following add-on creates a widget which, when clicked, highlights all the
`div` elements in the document loaded into the active tab:

    var widgets = require("sdk/widget");
    var tabs = require("sdk/tabs");

    var widget = widgets.Widget({
      id: "div-show",
      label: "Show divs",
      contentURL: "http://www.mozilla.org/favicon.ico",
      onClick: function() {
        tabs.activeTab.attach({
          contentScript:
            'var divs = document.getElementsByTagName("div");' +
            'for (var i = 0; i < divs.length; ++i) {' +
              'divs[i].setAttribute("style", "border: solid red 1px;");' +
            '}'
        });
      }
    });

## Private Windows ##

If your add-on has not opted into private browsing, then your page-mods will
not attach content scripts to documents loaded into private windows, even if
their URLs match the pattern you have specified.

To learn more about private windows, how to opt into private browsing, and how
to support private browsing, refer to the
[documentation for the `private-browsing` module](modules/sdk/private-browsing.html).

<api name="PageMod">
@class
A page-mod object. Once created a page-mod will execute the supplied content
scripts, and load any supplied stylesheets, in the context of any documents
matching the pattern specified by the `include` property.

<api name="PageMod">
@constructor
Creates a page-mod.
@param options {object}
  Options for the page-mod. All these options are optional except for `include`
  (although if you don't supply any scripts or stylesheets, your page-mod
  won't be very useful).
  @prop include {string,array}
    A match pattern string or an array of match pattern strings.  These define
    the documents to which the page-mod applies. At least one match pattern
    must be supplied.

    You can specify a URL exactly:

        var pageMod = require("sdk/page-mod");
        pageMod.PageMod({
          include: "http://www.iana.org/domains/example/",
          contentScript: 'window.alert("Page matches ruleset");'
        });

    You can specify a number of wildcard forms, for example:

        var pageMod = require("sdk/page-mod");
        pageMod.PageMod({
          include: "*.mozilla.org",
          contentScript: 'window.alert("Page matches ruleset");'
        });

    You can specify a set of URLs using a
    [regular expression](modules/sdk/util/match-pattern.html#Regular Expressions).
    The pattern must match the entire URL, not just a subset, and has
    `global`, `ignoreCase`, and `multiline` disabled.

        var pageMod = require("sdk/page-mod");
        pageMod.PageMod({
          include: /.*developer.*/,
          contentScript: 'window.alert("Page matches ruleset");'
        });

  To specify multiple patterns, pass an array of match patterns:

      var pageMod = require("sdk/page-mod");
      pageMod.PageMod({
        include: ["*.developer.mozilla.org", "*.addons.mozilla.org"],
        contentScript: 'window.alert("Page matches ruleset");'
      });

    See the [match-pattern](modules/sdk/util/match-pattern.html) module for
    a detailed description of match pattern syntax.

  @prop [contentScriptFile] {string,array}
    This option specifies one or more content scripts to attach to targeted
    documents.

    Each script is supplied as a separate file under your add-on's "data"
    directory, and is specified by a URL typically constructed using the
    `url()` method of the
    [`self` module's `data` object](modules/sdk/self.html#data).

        var data = require("sdk/self").data;
        var pageMod = require("sdk/page-mod");
        pageMod.PageMod({
          include: "*",
          contentScriptFile: data.url("my-script.js")
        });

    To attach multiple scripts, pass an array of URLs.

        var data = require("sdk/self").data;
        var pageMod = require("sdk/page-mod");

        pageMod.PageMod({
          include: "*",
          contentScriptFile: [self.data.url("jquery-1.7.min.js"),
                              self.data.url("my-script.js")]
        });

    Content scripts specified using this option are loaded before those
    specified by the `contentScript` option.

  @prop [contentScript] {string,array}
   This option specifies one or more content scripts to attach to targeted
   documents. Each script is supplied directly as a single string:

        var pageMod = require("sdk/page-mod");
        pageMod.PageMod({
          include: "*",
          contentScript: 'window.alert("Page matches ruleset");'
        });

   To attach multiple scripts, supply an array of strings.

   Content scripts specified by this option are loaded after those
   specified by the `contentScriptFile` option.

<div class="warning">
<p>Unless your content script is extremely simple and consists only of a
static string, don't use <code>contentScript</code>: if you do, you may
have problems getting your add-on approved on AMO.</p>
<p>Instead, keep the script in a separate file and load it using
<code>contentScriptFile</code>. This makes your code easier to maintain,
secure, debug and review.</p>
</div>

  @prop [contentScriptWhen="end"] {string}
   By default, content scripts are attached after all the content
   (DOM, JS, CSS, images) for the document has been loaded, at the time the
   [window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
   fires. Using this option you can customize this behavior.

   The option takes a single string that may take any one of the following
   values:

   * `"start"`: load content scripts immediately after the document
   element is inserted into the DOM, but before the DOM content
   itself has been loaded
   * `"ready"`: load content scripts once DOM content has been loaded,
   corresponding to the
   [DOMContentLoaded](https://developer.mozilla.org/en/Gecko-Specific_DOM_Events)
   event
   * `"end"`: the default. Load content scripts once all the content
   (DOM, JS, CSS, images) has been loaded, at the time the
   [window.onload event](https://developer.mozilla.org/en/DOM/window.onload)
   fires

<!-- -->

       var pageMod = require("sdk/page-mod");
       pageMod.PageMod({
         include: "*",
         contentScript: 'window.alert("Page matches ruleset");',
         contentScriptWhen: "start"
       });

   If you specify `"start"` for `contentScriptWhen`, your scripts will not be
   able to interact with the document's DOM right away (although they could
   listen for `window.onload` or `DOMContentLoaded` themselves).

  @prop [contentScriptOptions] {object}
   You can use this option to define some read-only values for your content
   scripts.

   The option consists of an object literal listing `name:value` pairs for
   the values you want to provide to the content script. For example:

       var data = require("sdk/self").data;
       var pageMod = require("sdk/page-mod");
       pageMod.PageMod({
         include: "*",
         contentScriptFile: data.url("my-script.js"),
         contentScriptOptions: {
           showOptions: true,
           someNumbers: [1, 2],
           greeting: "Hello!"
         }
       });

   The values are accessible to content scripts via the `self.options`
   property:

       // my-script.js
       if (self.options.showOptions) {
         window.alert(self.options.greeting);
         window.alert(self.options.someNumbers[0] + self.options.someNumbers[1]);
       }

   The values can be any JSON-serializable value: a string, number,
   boolean, null, array of JSON-serializable values, or an object whose
   property values are themselves JSON-serializable. This means you can't send
   functions, and if the object contains methods they won't be usable. You
   also can't pass cyclic values.

  @prop [contentStyleFile] {string,array}
    Use this option to load one or more stylesheets into the targeted
    documents as
    [user stylesheets](https://developer.mozilla.org/en/CSS/Getting_Started/Cascading_and_inheritance).

    Each stylesheet is supplied as a separate file under your add-on's "data"
    directory, and is specified by a URL typically constructed using the
    `url()` method of the
    [`self` module's `data` object](modules/sdk/self.html#data).
    To add multiple stylesheet files, pass an array of URLs.

        var data = require("sdk/self").data;
        var pageMod = require("sdk/page-mod");

        pageMod.PageMod({
          include: "*.org",
          contentStyleFile: data.url("my-page-mod.css")
        });

    Content styles specified by this
    option are loaded before those specified by the `contentStyle` option.

    You can't currently use relative URLs in stylesheets loaded in this way.
    For example, consider a CSS file that references an external file like this:

        background: rgb(249, 249, 249) url('../../img/my-background.png') repeat-x top center;

    If you attach this file using `contentStyleFile`, "my-background.png"
    will not be found.

    As a workaround for this, you can build an absolute URL to a file in your
    "data" directory, and construct a `contentStyle` option embedding that URL
    in your rule. For example:

        var data = require("sdk/self").data;
        var pageMod = require("sdk/page-mod").PageMod({
          include: "*",
          contentStyleFile: data.url("my-style.css"),
          // contentStyle is built dynamically here to include an absolute URL
          // for the file referenced by this CSS rule.
          // This workaround is needed because we can't use relative URLs
          // in contentStyleFile or contentStyle.
          contentStyle: "h1 { background: url(" + data.url("my-pic.jpg") + ")}"
        });

    This add-on uses a separate file "my-style.css", loaded using
    `contentStyleFile`, for all CSS rules except those that reference
    an external file. For the rule that needs to refer to "my-pic.jpg",
    which is stored in the add-on's "data" directory, it uses `contentStyle`.

    Dynamically constructing code strings like those assigned to `contentScript`
    or `contentStyle` is usually considered an unsafe practice that may cause an
    add-on to fail AMO review. In this case it is safe, and should be allowed,
    but including a comment like that in the example above will help to
    prevent any misunderstanding.

  @prop [contentStyle] {string,array}
    Use this option to load one or more stylesheet rules into
    the targeted documents.

    Each stylesheet rule is supplied as a separate string. To supply
    multiple rules, pass an array of strings:

        var pageMod = require("sdk/page-mod");

        pageMod.PageMod({
          include: "*.org",
          contentStyle: [
            "div { padding: 10px; border: 1px solid silver}",
            "img { display: none}"
          ]
        });

    Content styles specified by this
    option are loaded after those specified by the `contentStyleFile` option.

  @prop [attachTo] {string,array}
   If this option is not specified, content scripts:

   * are not attached to any tabs that are already open when the page-mod is
   created.
   * are attached to all documents whose URL matches the rule: so if your
   rule matches a specific hostname and path, and the topmost document that
   satisfies the rule includes ten iframes using a relative URL, then your
   page-mod is applied eleven times.

<!-- -->

   You can modify this behavior using the `attachTo` option.

   It accepts the following values:

   * `"existing"`: the page-mod will be automatically applied on already
   opened tabs.
   * `"top"`: the page-mod will be applied to top-level tab documents
   * `"frame"`: the page-mod will be applied to all iframes inside tab
   documents

<!-- -->

   If the option is set at all, you must set at least one of `"top"` and
   `"frame"`.

   For example, the following page-mod will be attached to already opened
   tabs, but not to any iframes:

       var pageMod = require("sdk/page-mod");
       pageMod.PageMod({
         include: "*",
         contentScript: "",
         attachTo: ["existing", "top"],
         onAttach: function(worker) {
           console.log("attached to: " + worker.tab.url);
         }
       });

  @prop [onAttach] {function}
   Assign a listener function to this option to listen to the page-mod's
   `attach` event. See the
   [documentation for `attach`](modules/sdk/page-mod.html#attach) and
   [Communicating With Content Scripts](modules/sdk/page-mod.html#Communicating With Content Scripts).

</api>

<api name="include">
@property {List}
  A [list](modules/sdk/util/list.html) of match pattern strings.  These
  define the documents to which the page-mod applies. See the documentation of
  the `include` option above for details of `include` syntax.
  Rules can be added to the list by calling its
  `add` method and removed by calling its `remove` method.

</api>

<api name="destroy">
@method
  Stops the page-mod from making any more modifications. Once destroyed
  the page-mod can no longer be used.

  Modifications already made to open documents by content scripts
  will not be undone, but stylesheets added by `contentStyle` or
  `contentStyleFile`, will be unregistered immediately.
</api>

<api name="attach">
@event
  This event is emitted when the page-mod's content scripts are
  attached to a document whose URL matches the page-mod's `include` pattern.

   The listener function is passed a
   [`worker`](modules/sdk/content/worker.html) object that you
   can use to
   [communicate with the content scripts](modules/sdk/page-mod.html#Communicating With Content Scripts) your page-mod has
   loaded into this particular document.

   The `attach` event is triggered every time this page-mod's content
   scripts are attached to a document. So if the user loads several
   documents which match this page-mod's `include` pattern, `attach` will be
   triggered for each document, each time with a distinct `worker` instance.

   Each worker then represents a channel of communication with the set of
   content scripts loaded by this particular page-mod into that
   particular document.

@argument {Worker}
   The listener function is passed a [`Worker`](modules/sdk/content/worker.html)
   object that can be used to communicate with any content scripts
   attached to this document.
</api>

<api name="error">
@event
This event is emitted when an uncaught runtime error occurs in one of the
page-mod's content scripts.

@argument {Error}
Listeners are passed a single argument, the
[Error](https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects/Error)
object.
</api>

</api>
