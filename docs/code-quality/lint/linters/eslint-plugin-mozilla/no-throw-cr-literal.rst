no-throw-cr-literal
===================

This is similar to the ESLint built-in rule no-throw-literal. It disallows
throwing Components.results code directly.

Throwing bare literals is inferior to throwing Exception objects, which provide
stack information.  Cr.ERRORs should be be passed as the second argument to
``Components.Exception()`` to create an Exception object with stack info, and
the correct result property corresponding to the NS_ERROR that other code
expects.
Using a regular ``new Error()`` to wrap just turns it into a string and doesn't
set the result property, so the errors can't be recognised.

This option can be autofixed (``--fix``).

.. code-block:: js

    performance.timing.navigationStart + performance.now()

Often timing relative to the page load is adequate and that conversion may not
be necessary.

Examples of incorrect code for this rule:
-----------------------------------------

.. code-block:: js

   throw Cr.NS_ERROR_UNEXPECTED;
   throw Components.results.NS_ERROR_ABORT;
   throw new Error(Cr.NS_ERROR_NO_INTERFACE);

Examples of correct code for this rule:
---------------------------------------

.. code-block:: js

		throw Components.Exception("Not implemented", Cr.NS_ERROR_NOT_IMPLEMENTED);
