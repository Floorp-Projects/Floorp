<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Porting the Library Detector #

This example walks through the process of porting a XUL-based add-on to the
SDK. It's a very simple add-on and a good candidate for porting because
there are suitable SDK APIs for all its features.

<img class="image-right" src="static-files/media/librarydetector/library-detector.png" alt="Library Detector Screenshot" />

The add-on is Paul Bakaus's
[Library Detector](https://addons.mozilla.org/en-US/firefox/addon/library-detector/).

The Library Detector tells you which JavaScript frameworks the current
web page is using. It does this by checking whether particular objects
that those libraries add to the global window object are defined.
For example, if `window.jQuery` is defined, then the page has loaded
[jQuery](http://jquery.com/).

For each library that it finds, the library detector adds an icon
representing that library to the status bar. It adds a tooltip to each
icon, which contains the library name and version.

You can browse and run the ported version in the SDK's `examples` directory.

### How the Library Detector Works ###

All the work is done inside a single file,
[`librarydetector.xul`](http://code.google.com/p/librarydetector/source/browse/trunk/chrome/content/librarydetector.xul)
This contains:

<ul>
	<li>a XUL overlay</li>
	<li>a script</li>
</ul>

The XUL overlay adds a `box` element to the browser's status bar:

<script type="syntaxhighlighter" class="brush: html"><![CDATA[
  &lt;statusbar id="status-bar"&gt; &lt;box orient="horizontal" id="librarydetector"&gt; &lt;/box&gt; &lt;/statusbar&gt;
]]>
</script>

The script does everything else.

The bulk of the script is an array of test objects, one for each library.
Each test object contains a function called `test()`: if the
function finds the library, it defines various additional properties for
the test object, such as a `version` property containing the library version.
Each test also contains a `chrome://` URL pointing to the icon associated with
its library.

The script listens to [gBrowser](https://developer.mozilla.org/en/Code_snippets/Tabbed_browser)'s
`DOMContentLoaded` event. When this is triggered, the `testLibraries()`
function builds an array of libraries by iterating through the tests and
adding an entry for each library which passes.

Once the list is built, the `switchLibraries()` function constructs a XUL
`statusbarpanel` element for each library it found, populates it with the
icon at the corresponding `chrome://` URL, and adds it to the box.

Finally, it listens to gBrowser's `TabSelect` event, to update the contents
of the box for that window.

### Content Script Separation ###

The test objects in the original script need access to the DOM window object,
so in the SDK port, they need to run in a content script. In fact, they need
access to the un-proxied DOM window, so they can see the objects added by
libraries, so we’ll need to use the experimental [unsafeWindow](dev-guide/guides/content-scripts/accessing-the-dom.html#unsafeWindow)

The main add-on script, `main.js`, will use a
[`page-mod`](modules/sdk/page-mod.html)
to inject the content script into every new page.

The content script, which we'll call `library-detector.js`, will keep most of
the logic of the `test` functions intact. However, instead of maintaining its
own state by listening for `gBrowser` events and updating the
user interface, the content script will just run when it's loaded, collect
the array of library names, and post it back to `main.js`:

    function testLibraries() {
      var win = unsafeWindow;
      var libraryList = [];
      for(var i in LD_tests) {
        var passed = LD_tests[i].test(win);
        if (passed) {
          var libraryInfo = {
            name: i,
            version: passed.version
          };
          libraryList.push(libraryInfo);
        }
      }
      self.postMessage(libraryList);
    }

    testLibraries();

`main.js` responds to that message by fetching the tab
corresponding to that worker using
[`worker.tab`](modules/sdk/content/worker.html#tab), and adding
the array of library names to that tab's `libraries` property:

    pageMod.PageMod({
      include: "*",
      contentScriptWhen: 'end',
      contentScriptFile: (data.url('library-detector.js')),
      onAttach: function(worker) {
        worker.on('message', function(libraryList) {
          if (!worker.tab.libraries) {
            worker.tab.libraries = [];
          }
          libraryList.forEach(function(library) {
            if (worker.tab.libraries.indexOf(library) == -1) {
              worker.tab.libraries.push(library);
            }
          });
          if (worker.tab == tabs.activeTab) {
            updateWidgetView(worker.tab);
          }
        });
      }
    });

The content script is executed once for every `window.onload` event, so
it will run multiple times when a single page containing multiple iframes
is loaded. So `main.js` needs to filter out any duplicates, in case
a page contains more than one iframe, and those iframes use the same library.

### Implementing the User Interface ###

#### Showing the Library Array ####

The [`widget`](modules/sdk/widget.html) module is a natural fit
for displaying the library list. We'll specify its content using HTML, so we
can display an array of icons. The widget must be able to display different
content for different windows, so we'll use the
[`WidgetView`](modules/sdk/widget.html) object.

`main.js` will create an array of icons corresponding to the array of library
names, and use that to build the widget's HTML content dynamically:

    function buildWidgetViewContent(libraryList) {
      widgetContent = htmlContentPreamble;
      libraryList.forEach(function(library) {
          widgetContent += buildIconHtml(icons[library.name],
            library.name + "<br>Version: " + library.version);
      });
      widgetContent += htmlContentPostamble;
      return widgetContent;
    }

    function updateWidgetView(tab) {
      var widgetView = widget.getView(tab.window);
      if (!tab.libraries) {
        tab.libraries = [];
      }
      widgetView.content = buildWidgetViewContent(tab.libraries);
      widgetView.width = tab.libraries.length * ICON_WIDTH;
    }

`main.js` will
use the [`tabs`](modules/sdk/tabs.html) module to update the
widget's content when necessary (for example, when the user switches between
tabs):

    tabs.on('activate', function(tab) {
      updateWidgetView(tab);
    });

    tabs.on('ready', function(tab) {
      tab.libraries = [];
    });

#### Showing the Library Detail ####

The XUL library detector displayed the detailed information about each
library on mouseover in a tooltip: we can't do this using a widget, so
instead will use a panel. This means we'll need two additional content
scripts:

* one in the widget's context, which listens for icon mouseover events
and sends a message to `main.js` containing the name of the corresponding
library:

<pre><code>
function setLibraryInfo(element) {
  self.port.emit('setLibraryInfo', element.target.title);
}

var elements = document.getElementsByTagName('img');

for (var i = 0; i &lt; elements.length; i++) {
  elements[i].addEventListener('mouseover', setLibraryInfo, false);
}
</code></pre>

* one in the panel, which updates the panel's content with the library
information:

<pre><code>
self.on("message", function(libraryInfo) {
  window.document.body.innerHTML = libraryInfo;
});
</code></pre>

Finally `main.js` relays the library information from the widget to the panel:

<pre><code>
widget.port.on('setLibraryInfo', function(libraryInfo) {
  widget.panel.postMessage(libraryInfo);
});
</code></pre>

<img class="image-center" src="static-files/media/librarydetector/panel-content.png" alt="Updating panel content" />
