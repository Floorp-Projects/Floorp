no-useless-removeEventListener
==============================

Reject calls to removeEventListener where ``{once: true}`` could be used instead.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    elt.addEventListener('click', function listener() {
      elt.removeEventListener('click', listener);
    });

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

	  elt.addEventListener('click', handler, {once: true});
