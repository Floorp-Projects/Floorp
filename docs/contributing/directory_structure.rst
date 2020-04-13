Mozilla Source Code Directory Structure
=======================================

The code for some projects in the Mozilla family (such
as `Firefox <https://www.mozilla.org/en-US/firefox/products/>`__ , `Gecko <https://developer.mozilla.org/docs/Mozilla/Gecko/>`__ , `etc. <https://www.mozilla.org/products/>`__)
is combined into a single source tree. The tree contains the source code
as well as the code required to build each project on supported
platforms (Linux, Windows, macOS, etc). This article provides an
overview of what the various directories contain.

To simply take a look at the Mozilla source code, you do not need to
download it. You can look at the source directly with your web browser
using Searchfox (start at https://searchfox.org/mozilla-central/source for
the complete mozilla source code of branch HEAD).

In order to modify the source, you have to acquire it either by
downloading a
`snapshot <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Downloading_Source_Archives>`__
of the sources or by `checking out the current sources from
Mercurial <How to contribute to Firefox>`__.

This document describes the directory structure -- i.e., directories that
are used by at least some of the
Mozilla project's client products. There are other directories in the
other Mozilla repository, such as those for Web tools and those for the
Classic codebase.

See `source code directories
overview <https://developer.mozilla.org/docs/Archive/Misc_top_level/Source_code_directories_overview>`__ for a
somewhat different (older) version of the same information. Also see the
`more detailed overview of the pieces of
Gecko <https://wiki.mozilla.org/Gecko:Overview>`__.

.cargo
------

Configuration files for the `Cargo package
manager <https://crates.io/>`__.

.vscode
-------

Configuration files used by the `Visual Studio Code
IDE <https://code.visualstudio.com/>`__ when working in the
mozilla-central tree.

accessible
----------

Files for accessibility (i.e., MSAA (Microsoft Active Accessibility),
ATK (Accessibility Toolkit, used by GTK+ 2) support files). See
`Accessibility <https://developer.mozilla.org/docs/Web/Accessibility>`__.


browser
-------

Contains the front end code (in XUL, Javascript, XBL, and C++) for the
Firefox browser. Many of these files started off as a copy of files in
`xpfe <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Directory_structure#xpfe>`__/.

browser/extensions
------------------

Contains `PDF.js <https://mozilla.github.io/pdf.js/>`__ and
`WebCompat <https://github.com/mozilla/webcompat-addon>`__ built-in extensions.

browser/themes
--------------

Contains images and CSS files to skin the browser for each OS (Linux,
Mac and Windows)

build
-----

Miscellaneous files used by the build process. See also
`config <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Directory_structure#config>`__/.

caps
----

Capability-based web page security management. It contains C++ interfaces
and code for determining the capabilities of content based on the
security settings or certificates (e.g., VeriSign). See `Component
Security <https://www.mozilla.org/projects/security/components/>`__ .

chrome
------

Chrome registry (See
`here <https://developer.mozilla.org/en/docs/Chrome_Registration>`__)
used with `toolkit <#toolkit>`__/. These files were originally copies of
files in `rdf/chrome/`.

config
------

More files used by the build process, common includes for the makefiles,
etc.


devtools
--------

The `Firefox Developer Tools <https://developer.mozilla.org/docs/Tools>`__ server and client components.


docs
----

Contains the documentation configuration (`Sphinx <http://www.sphinx-doc.org/>`__ based), the index page
and the contribution pages.


docshell
--------

Implementation of the docshell, the main object managing things related
to a document window. Each frame has its own docshell. It contains
methods for loading URIs, managing URI content listeners, etc. It is the
outermost layer of the embedding API used to embed a Gecko browser into
an application.

dom
---

-  `IDL definitions <https://developer.mozilla.org/docs/Mozilla/Tech/XPIDL>`__ of the interfaces defined by
   the DOM specifications and Mozilla extensions to those interfaces
   (implementations of these interfaces are primarily, but not
   completely, in `content <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Directory_structure#content>`__).
-  The parts of the connection between JavaScript and the
   implementations of DOM objects that are specific both to JavaScript
   and to the DOM.
-  Implementations of a few of the core "DOM Level 0" objects, such as
   `window <https://developer.mozilla.org/docs/Web/API/Window>`__ , `window.navigator <https://developer.mozilla.org/docs/Web/API/Window/navigator>`__, `window.location <https://developer.mozilla.org/docs/Web/API/Window/location>`__, etc.

editor
------

The editor directory contains XUL/Javascript for the embeddable editor
component, which is used for the HTML Editor("Composer"), for plain and
HTML mail composition, and for text fields and text areas throughout the
product. The editor is designed like a
"browser window with editing features": it adds some special classes for
editing text and managing transaction undo/redo, but reuses browser code
for nearly everything else.

extensions
----------

Contains several extensions to mozilla, which can be enabled at
compile-time using the ``--enable-extensions`` configure argument.

Note that some of these are now built specially and not using the
``--enable-extensions`` option. For example, disabling xmlextras is done
using ``--disable-xmlextras``.


extensions/auth
---------------

Implementation of the negotiate auth method for HTTP and other
protocols. Has code for SSPI, GSSAPI, etc. See `Integrated
Authentication <https://www.mozilla.org/projects/netlib/integrated-auth.html>`__.


extensions/pref
---------------

Preference-related extensions.

extensions/spellcheck
---------------------

Spellchecker for mailnews and composer.

extensions/universalchardet
---------------------------

Detects the character encoding of text.

gfx
---

Contains interfaces that abstract the capabilities of platform specific
graphics toolkits, along with implementations on various platforms.
These interfaces provide methods for things like drawing images, text,
and basic shapes. It also contains basic data structures such as points
and rectangles used here and in other parts of Mozilla.

gradle
------

Containing files related to a Java build system.

hal
---

Contains platform specified functions (e.g. obtaining battery status,
sensor information, memory information, Android
alarms/vibrate/notifications/orientation, etc)

image
-----

Image rendering library. Contains decoders for the image formats Firefox
supports.

intl
----

Internationalization and localization support. See
`L10n:NewProjects <https://wiki.mozilla.org/L10n:NewProjects>`__.

intl/locale
-----------

Code related to determination of locale information from the operating
environment.

intl/lwbrk
----------

Code related to line breaking and word breaking.

intl/strres
-----------

Code related to string resources used for localization.

intl/uconv
----------

Code that converts (both ways: encoders and decoders) between UTF-16 and
many other character encodings.

intl/unicharutil
----------------

Code related to implementation of various algorithms for Unicode text,
such as case conversion.

ipc
---

Container for implementations of IPC (Inter-Process Communication).

js/src
------

The JavaScript engine, also known as
`SpiderMonkey <https://developer.mozilla.org/docs/Mozilla/Projects/SpiderMonkey>`__.
See also `JavaScript <https://developer.mozilla.org/docs/JavaScript>`__.

js/xpconnect
------------

Support code for calling JavaScript code from C++ code and C++ code from
JavaScript code, using XPCOM interfaces. See
`XPConnect <https://developer.mozilla.org/docs/XPConnect>`__.

layout
------

Code that implements a tree of rendering objects that describe the types
and locations of the objects that are displayed on the screen (such as
CSS boxes, tables, form controls, XUL boxes, etc.), and code that
manages operations over that rendering tree (such as creating and
destroying it, doing layout, painting, and event handling). See
`documentation <https://www.mozilla.org/newlayout/doc/>`__ and `other
information <https://www.mozilla.org/newlayout/>`__.

layout/base
-----------

Code that deals with the rendering tree.

layout/forms
------------

Rendering tree objects for HTML form controls.

layout/generic
--------------

The basic rendering object interface and the rendering tree objects for
basic CSS boxes.

layout/mathml
-------------

Rendering tree objects for `MathML <https://developer.mozilla.org/docs/Web/MathML>`__.

layout/svg
----------

Rendering tree objects for `SVG <https://developer.mozilla.org/docs/Web/SVG>`__.

layout/tables
-------------

Rendering tree objects for CSS/HTML tables.

layout/xul
----------

Additional rendering object interfaces for `XUL <https://developer.mozilla.org/docs/XUL>`__ and
the rendering tree objects for XUL boxes.

media
-----

Contains sources of used media libraries for example *libpng*.

memory
------

Cross-platform wrappers for *memallocs* functions etc.

mfbt
----

Implementations of classes like *WeakPtr*. Multi-platform *assertions*
etc. `More on
MFBT <https://developer.mozilla.org/docs/Mozilla/MFBT>`__

mobile
------

mobile/android
--------------

Firefox for Android and Geckoview

modules
-------

Compression/Archiving, math library, font (and font compression),
Preferences Library

modules/libjar
--------------

Code to read zip files, used for reading the .jar files that contain the
files for the mozilla frontend.

modules/libpref
---------------

Library for reading and writing preferences.

modules/zlib
------------

Source code of zlib, used at least in the networking library for
compressed transfers.

mozglue
-------

Glue library containing various low-level functionality, including a
dynamic linker for Android, a DLL block list for Windows, etc.

netwerk
-------

`Networking library <https://developer.mozilla.org/docs/Necko>`__, also known as Necko.
Responsible for doing actual transfers from and to servers, as well as
for URI handling and related stuff.

netwerk/cookie
--------------

Permissions backend for cookies, images, etc., as well as the user
interface to these permissions and other cookie features.

nsprpub
-------

Netscape Portable Runtime. Used as an abstraction layer to things like
threads, file I/O, and socket I/O. See `Netscape Portable
Runtime <https://www.mozilla.org/projects/nspr/>`__.

nsprpub/lib
-----------

Mostly unused; might be used on Mac?

other-licenses
--------------

Contains libraries that are not covered by the MPL but are used in some
Firefox code.

parser
------

Group of structures and functions needed to parse files based on
XML/HTML.

parser/expat
------------

Copy of the expat source code, which is the XML parser used by mozilla.

parser/html
-----------

The HTML parser (for everything except about:blank).

parser/htmlparser
-----------------

The legacy HTML parser that's still used for about:blank. Parts of it
are also used for managing the conversion of the network bytestream into
Unicode in the XML parsing case.

parser/xml
----------

The code for integrating expat (from parser/expat) into Gecko.

python
------

Cross module python code.

python/mach
-----------

The code for the `Mach <https://developer.mozilla.org/docs/Mozilla/Developer_guide/mach>`__ building
tool.

security
--------

Contains NSS and PSM, to support cryptographic functions in mozilla
(like S/MIME, SSL, etc). See `Network Security Services
(NSS) <https://www.mozilla.org/projects/security/pki/nss/>`__ and
`Personal Security Manager
(PSM) <https://www.mozilla.org/projects/security/pki/psm/>`__.

services
--------

Firefox accounts and sync (history, preferences, tabs, bookmarks,
telemetry, startup time, which addons are installed, etc). See
`here <https://docs.services.mozilla.com/>`__.

servo
-----

`Servo <https://servo.org/>`__, the parallel browser engine project.

startupcache
------------

XXX this needs a description.

storage
-------

`Storage <https://developer.mozilla.org/docs/Mozilla/Tech/XPCOM/Storage>`__: XPCOM wrapper for sqlite. Wants to
unify storage of all profile-related data. Supersedes mork. See also
`Unified Storage <https://wiki.mozilla.org/Mozilla2:Unified_Storage>`__.

taskcluster
-----------

Scripts and code to automatically build and test Mozilla trees for the
continuous integration and release process.

testing
-------

Common testing tools for mozilla codebase projects, test suite
definitions for automated test runs, tests that don't fit anywhere else,
and other fun stuff.

third_party
-----------

Vendored dependencies maintained outside of Mozilla.

toolkit
-------

The "new toolkit" used by Thunderbird, Firefox, etc. This contains
numerous front-end components shared between applications as well as
most of the XBL-implemented parts of the XUL language (most of which was
originally forked from versions in `xpfe/`).

toolkit/mozapps/extensions/test/xpinstall
-----------------------------------------

The installer, which contains code for installing Mozilla and for
installing XPIs/extensions. This directory also contains code needed to
build installer packages. See `XPInstall <https://developer.mozilla.org/docs/XPInstall>`__ and
the `XPInstall project
page <https://www.mozilla.org/projects/xpinstall/>`__.

tools
-----

Some tools which are optionally built during the mozilla build process.

tools/lint
----------

The linter declarations and configurations.
See `linting documentation </code-quality/lint/>`_

uriloader
---------

uriloader/base
--------------

Content dispatch in Mozilla. Used to load uris and find an appropriate
content listener for the data. Also manages web progress notifications.
See `Document Loading: From Load Start to Finding a
Handler <https://www.mozilla.org/docs/docshell/uri-load-start.html>`__
and `The Life Of An HTML HTTP
Request <https://www.mozilla.org/docs/url_load.html>`__.


uriloader/exthandler
--------------------

Used to handle content that Mozilla can't handle itself. Responsible for
showing the helper app dialog, and generally for finding information
about helper applications.

uriloader/prefetch
------------------

Service to prefetch documents in order to have them cached for faster
loading.

view
----

View manager. Contains cross-platform code used for painting, scrolling,
event handling, z-ordering, and opacity. Soon to become obsolete,
gradually.

widget
------

A cross-platform API, with implementations on each platform, for dealing
with operating system/environment widgets, i.e., code related to
creation and handling of windows, popups, and other native widgets and
to converting the system's messages related to painting and events into
the messages used by other parts of Mozilla (e.g., `view/` and
`content/`, the latter of which converts many of the
messages to yet another API, the DOM event API).

xpcom
-----

`Cross-Platform Component Object Model </en-US/docs/XPCOM>`__. Also
contains data structures used by the rest of the mozilla code. See also
`XPCOM Project <https://www.mozilla.org/projects/xpcom/>`__.

xpfe
----

XPFE (Cross Platform Front End) is the SeaMonkey frontend. It contains
the XUL files for the browser interface, common files used by the other
parts of the mozilla suite, and the XBL files for the parts of the XUL
language that are implemented in XBL. Much of this code has been copied
to `browser/` and `toolkit/` for use in
Firefox, Thunderbird, etc.


xpfe/components
---------------

Components used by the Mozilla frontend, as well as implementations of
interfaces that other parts of mozilla expect.


More documentation about Mozilla Source Code Directory Structure
----------------------------------------------------------------

https://developer.mozilla.org/docs/Mozilla/Developer_guide/Source_Code/Directory_structure
