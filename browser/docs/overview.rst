Firefox for Desktop
===================

Desktop architecture
--------------------

One of the keys to understanding how Firefox is architected is to know that Gecko is used to render
both web content as well as the UI of the browser. This means that, to a large extent, the UI of
Firefox is made using web technologies like HTML, CSS, and JavaScript. For example, the document
``chrome://browser/content/browser.xhtml`` is the markup for a single Firefox browser window. You
can think of it almost like index.html for a webpage.

The decision to use Gecko for rendering the UI was made during an era when HTML was not yet
expressive enough or sufficient to build desktop UIs. A different markup language,
`XUL <https://en.wikipedia.org/wiki/XUL>`_, was developed for that purpose instead, which Gecko also
knows how to interpret. Since that time, HTML and CSS has evolved to the point where XUL is not
strictly necessary for most use-cases. Where possible, we have been gradually migrating our UI to
use less XUL and more standard HTML and CSS.

Reusable UI Widgets
-------------------

Many of the UI widgets in use in the user interface are built as web components that can be reused.

:ref:`Read more <Reusable UI widgets>`

Tabbrowser
----------

The tabbrowser component manages the tabs in a browser window.

:ref:`Read more <tabbrowser>`

Address Bar
-----------

The address bar (varyingly also referred to as the Awesome Bar, or Quantum Bar) handles users
entering web addresses including a popup with suggestions from various sources including history,
bookmarks and search engines.

:ref:`Read more <Address Bar>`

Search
------

The search service manages the list of search engines available in the address bar.

:ref:`Read more <Search>`

Places
------

The places component includes the database that stores history and bookmarks as well as a set of UI
components that present those to the user.

:ref:`Read more <Places>`

Firefox Sync and Application Services
-------------------------------------

Application services includes the sync engines and Firefox Accounts integration.

:ref:`Read more <Services>`

Developer Tools
---------------

The developer tools support web developers when building webpages but as the Firefox UI is itself
built using web technologies it is also possible to use the developer tools to debug the Firefox UI.

:ref:`Read more <Firefox DevTools Contributor Docs>`

Browser Toolbox
###############

The Browser Toolbox is a way to launch the developer tools in a separate process so that they can be
used to debug the main browser UI.

:ref:`Read more <Browser Toolbox>`

Profiler
########

The Firefox Profiler is the way to measure the performance of your code. Go to
`profiler.firefox.com <https://profiler.firefox.com>`__ to enable the button in Firefox and record a
performance profile. These profiles can be uploaded and shared. It can identify slow parts of the
code, and reveal the underlying behavior of how code runs.

Installer
---------

The Windows installer for Firefox is built with NSIS. There are currently no installers for other
operating systems.

:ref:`Read more <Installer>`

Application Update
------------------

Application Update is responsible for requesting available updates from the update servers,
downloading them, verifying their integrity and then ultimately installing them. A binary patch tool
(bsdiff) is used to reduce the size of update files and update files are delivered in the
bespoke mar (Mozilla ARchive) format.

:ref:`Read more <Application Update>`
