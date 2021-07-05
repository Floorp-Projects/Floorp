avoid-Date-timing
=================

Rejects grabbing the current time via Date.now() or new Date() for timing
purposes when the less problematic performance.now() can be used instead.

The performance.now() function returns milliseconds since page load. To
convert that to milliseconds since the epoch, use:

.. code-block:: js

    performance.timing.navigationStart + performance.now()

Often timing relative to the page load is adequate and that conversion may not
be necessary.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

    Date.now()

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		new Date('2017-07-11');
		performance.now()
