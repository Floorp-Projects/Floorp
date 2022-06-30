reject-importGlobalProperties
=============================

Rejects calls to ``Cu.importGlobalProperties`` or
``XPCOMUtils.defineLazyGlobalGetters``.

In system modules all the required properties should already be available. In
non-module code or non-system modules, webidl defined interfaces should already
be available and hence do not need importing.

Options
-------

* "everything": Disallows using the import/getters completely.
* "allownonwebidl": Disallows using the import functions for webidl symbols. Allows
  other symbols.

everything
----------

Incorrect code for this option:

.. code-block:: js

    Cu.importGlobalProperties(['TextEncoder']);
    XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder']);

allownonwebidl
--------------

Incorrect code for this option:

.. code-block:: js

    // AnimationEffect is a webidl property.
    Cu.importGlobalProperties(['AnimationEffect']);
    XPCOMUtils.defineLazyGlobalGetters(this, ['AnimationEffect']);

Correct code for this option:

.. code-block:: js

    // TextEncoder is not defined by webidl.
    Cu.importGlobalProperties(['TextEncoder']);
    XPCOMUtils.defineLazyGlobalGetters(this, ['TextEncoder']);
