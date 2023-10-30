use-cc-etc
======================

This requires using ``Cc`` rather than ``Components.classes``, and the same for
``Components.interfaces``, ``Components.results`` and ``Components.utils``.
This has a slight performance advantage by avoiding the use of the dot.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    let foo = Components.classes['bar'];
    let bar = Components.interfaces.bar;
    Components.results.NS_ERROR_ILLEGAL_INPUT;
    Components.utils.reportError('fake');

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    let foo = Cc['bar'];
    let bar = Ci.bar;
    Cr.NS_ERROR_ILLEGAL_INPUT;
    Cu.reportError('fake');
