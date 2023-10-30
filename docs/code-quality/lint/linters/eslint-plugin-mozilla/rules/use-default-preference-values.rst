use-default-preference-values
=============================

Require providing a second parameter to ``get*Pref`` methods instead of
using a try/catch block.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    try { blah = branch.getCharPref('blah'); } catch(e) {}

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    blah = branch.getCharPref('blah', true);
