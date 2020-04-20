==================
Stub Installer GUI
==================

The stub installer's primary GUI is built using the web platform, rendered by the native Windows browser control ``IWebBrowser2``, which is based on Internet Explorer (Microsoft never updated it to use Edge [#1]_).

This is all driven by a custom NSIS plugin, called WebBrowser. The plugin's source code can be found in the directory ``other-licenses/nsis/Contrib/WebBrowser``, and its public API is documented :doc:`here <WebBrowserAPI>`. The plugin implements the COM interfaces needed to host the IWebBrowser2 object and embed it using OLE into the dialog provided by NSIS. It also provides an implementation of timers [#2]_, and supports making NSIS functions callable from JavaScript running inside the web page.

Note that not 100% of the stub UI is built using WebBrowser; we also have message boxes which use `the Windows TaskDialog function <https://docs.microsoft.com/en-us/windows/win32/api/commctrl/nf-commctrl-taskdialog>`_, invoked from NSIS code using the standard System plugin.

Changing the UI
===============

For each of the two main pages, there is one HTML file (which also contains the JavaScript for the page) and one CSS file.

The HTML file for each page is always the same regardless of the branding, so it's kept in the ``installer`` directory next to ``stub.nsi``. These files should be updated if elements need to be added or removed from the page, or if features requiring JavaScript are being implemented or changed.

Each branding directory (``/browser/branding/*``) contains a CSS file for each page, which specifies how the page should be laid out and rendered. These are normal CSS files and can easily be modified to change or redesign one or both pages for any or all brandings. Currently there are two completely different layouts, one for release and another for all other brandings, and the two differ only in this CSS file and in the background image file (also in the branding directory).

Constraints
===========

The most severe constraint on the web UI is that the browser engine must be configured to run in Internet Explorer 8 mode. This is because the installer must run correctly on an unpatched Windows 7 installation with no service packs or other updates, and that is the version of IE that such an installation provides. That means this constraint can only be relaxed if Firefox's OS support changes to exclude unpatched Windows 7 (i.e., either dropping Windows 7 entirely or requiring Service Pack 1).

Image assets (meaning only the background image at the moment) can be any format IE8 can open, including JPEG, GIF, or PNG, but any newer formats like WebP won't work. SVG isn't supported until IE9.

Fonts should be limited to those that a default Windows 7+ installation provides, which may vary based on locale (it is possible to specify a specific font to use for a certain locale).

The size of the window is difficult to change, and doing so would affect all brandings and require redesigning the background art, so changing that should be avoided.

Care should be taken to make sure that accessibility is maintained, and best practices for web accessibility should be followed. For example, the installing page contains a progress bar. Normally, and in keeping with best practices, this would be implemented using a ``<progress>`` element, but IE8 doesn't support those, so it has to be built manually out of divs instead. Because this is an inherently inaccessible design, appropriate ARIA attributes are used so that the control is presented to accessibility tools as a real progress bar.

Technical Details
=================

Custom Functions
----------------

Custom functions provide a foreign function interface that allows JavaScript running in the web page to invoke NSIS functions. Since the installer logic is all handled in NSIS, this is necessary for the UI to be able to determine the state of the installation, relay any settings to the installer, etc. Custom functions always take a single argument on the NSIS stack, and return a single value on the NSIS stack (this design is mostly for convenience, to avoid needing the C++ glue layer to handle different numbers of intended arguments or return values).

The implementation of custom functions is basically glue between the NSIS ExecuteCodeSegment mechanism (which allows executing NSIS functions from native plugin code) and the web browser control's support for IDispatch (which allows invoking native code from JavaScript). Connecting those two things together allows us to run NSIS code from within JavaScript.

To make an NSIS function available to JavaScript, the NSIS script must call the plugin's ``RegisterCustomFunction`` export, passing it the address (as obtained from the ``GetFunctionAddress`` instruction) and the name that the function should be exposed to JavaScript under. This call has to happen after the call to ``Init`` and before ``ShowPage``. The plugin wraps that address in a native C++ function, and passes it to the WebBrowser control to register with its IDispatch implementation. That code allocates a ``DISPID`` for the new function and associates it with the given name. Later, when JavaScript invokes a function by that name, our ``IDispatch::Invoke`` calls the wrapper function with the argument that was passed in by JavaScript (if any), and makes sure the return value gets relayed back to JavaScript as well.



.. [#1] There is currently a new control called `WebView2 <https://docs.microsoft.com/en-us/microsoft-edge/hosting/webview2>`_ in development which uses the Chrome-based Edge engine, but it's unfinished as of this writing and would only work on machines with Chrome-based Edge installed anyway.

.. [#2] Previously, the stub installer's UI was based on nsDialogs, and used its timer implementation, so when nsDialogs was removed an alternative was needed. The replacement timers don't strictly need to be provided by the WebBrowser plugin, but it was the convenient place to put those functions.

.. toctree::
   WebBrowserAPI
