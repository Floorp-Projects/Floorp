no-define-cc-etc
================

Disallows old-style definitions for ``Cc``/``Ci``/``Cu``/``Cr``. These are now
defined globally for all chrome contexts.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    var Cc = Components.classes;
    var Ci = Components.interfaces;
    var {Ci: interfaces, Cc: classes, Cu: utils} = Components;
    var Cr = Components.results;


Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    const CC = Components.Constructor;
