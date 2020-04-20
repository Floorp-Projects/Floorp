=====================
WebBrowser Plugin API
=====================

This page lists all the functions exported from the WebBrowser plugin and explains how to use them.

``ShowPage``
  This function takes one parameter: the URL or file path to be displayed. It returns nothing.

  ``ShowPage`` will block until the page is closed, either by closing the window or by something like sending the parent dialog a "go to page" message.

  Dialog item 1018 will be taken over as the control to display the web page in, so if you're using a custom dialog template, be sure it contains a control with that ID.

  ``file:`` URL's are supported, but passing in just the file path also works. Relative paths are supported but should be avoided because the stub installer's working directory isn't necessarily predictable or meaningful. Web (HTTP or HTTPS) URL's can be used, but the security restrictions the browser control is configured with will prevent most resource files from being downloaded, so they are unlikely to be very useful.

``RegisterCustomFunction``
  This function allows an NSIS function to be called from JavaScript.

  This function takes two parameters, the function address, as obtained from the ``GetFunctionAddress`` instruction, and then a string containing the name of the function to be exposed to JavaScript. It returns nothing.

  ``RegisterCustomFunction`` should be called before ``ShowPage`` (which will block anyway).

  JavaScript can invoke this function by the assigned name under the global ``external`` object (that's part of the design of the control and isn't something we can change). The NSIS function will receive a single parameter on the NSIS stack (whether it needs one or not) and is expected to return a single value on the NSIS stack (whether it needs to or not) which will become the return value from the JavaScript function.

  Currently only strings, integers, and booleans are supported as parameter types; anything else will result in the NSIS function being passed an empty string. More types could be added if needed. The return type will always be passed back to JavaScript directly as a string, and any conversions needed must be performed in JS.

``CreateTimer``
  This function creates an interval timer and invokes a callback at each interval.

  This function takes two parameters, the function address of the timer callback, as obtained from the ``GetFunctionAddress`` instruction, and then the timer interval in milliseconds. It returns a handle which can later be passed to ``CancelTimer``.

``CancelTimer``
  This function stops running a timer that was started by ``CreateTimer``.

  It takes the handle that ``CreateTimer`` returned, and returns nothing.
