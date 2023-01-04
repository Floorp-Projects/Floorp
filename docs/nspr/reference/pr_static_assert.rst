PR_STATIC_ASSERT
================

Prevents code from compiling when an expression has the value ``FALSE``
at compile time.


Syntax
------

.. code::

   #include <prlog.h>

   PR_STATIC_ASSERT ( expression );


Parameters
~~~~~~~~~~

The macro has this parameter:

expression
   Any valid expression which evaluates at compile-time to ``TRUE`` or
   ``FALSE``. An expression which cannot be evaluated at compile time
   will cause a compiler error; see :ref:`PR_ASSERT` for a runtime
   alternative.


Returns
~~~~~~~

Nothing


Description
-----------

This macro evaluates the specified expression. When the result is zero
(``FALSE``) program compilation will fail with a compiler error;
otherwise compilation completes successfully. The compiler error will
include the number of the line for which the compile-time assertion
failed.

This macro may only be used in locations where an ``extern`` function
declaration may be used.
