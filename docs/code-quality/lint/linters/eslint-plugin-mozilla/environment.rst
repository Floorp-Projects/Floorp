Environment
===========

These environments are available by specifying a comment at the top of the file,
e.g.

.. code-block:: js

   /* eslint-env mozilla/chrome-worker */

There are also built-in ESLint environments available as well. Find them here: http://eslint.org/docs/user-guide/configuring#specifying-environments

browser-window
--------------

Defines the environment for scripts that are in the main browser.xhtml scope.

chrome-worker
-------------

Defines the environment for chrome workers. This differs from normal workers by
the fact that `ctypes` can be accessed as well.

frame-script
------------

Defines the environment for frame scripts.

jsm
---

Defines the environment for jsm files (javascript modules).
