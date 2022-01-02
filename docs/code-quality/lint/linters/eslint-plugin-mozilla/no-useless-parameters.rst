no-useless-parameters
=====================

Reject common XPCOM methods called with useless optional parameters (eg.
``Services.io.newURI(url, null, null)``, or non-existent parameters (eg.
``Services.obs.removeObserver(name, observer, false)``).

This option can be autofixed (``--fix``).

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    elt.addEventListener('click', handler, false);
    Services.io.newURI('http://example.com', null, null);
    Services.obs.notifyObservers(obj, 'topic', null);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

    elt.addEventListener('click', handler);
    Services.io.newURI('http://example.com');
    Services.obs.notifyObservers(obj, 'topic');
