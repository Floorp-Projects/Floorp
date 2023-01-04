PR_ASSERT
=========

Terminates execution when a given expression is ``FALSE``.


Syntax
------

.. code::

   #include <prlog.h>

   void PR_ASSERT ( expression );


Parameters
~~~~~~~~~~

The macro has this parameter:

expression
   Any valid C language expression that evaluates to ``TRUE`` or
   ``FALSE``.


Returns
~~~~~~~

Nothing


Description
-----------

This macro evaluates the specified expression. When the result is zero
(``FALSE``) the application terminates; otherwise the application
continues. The macro converts the expression to a string and passes it
to ``PR_Assert``, using file and line parameters from the compile-time
environment.

This macro compiles to nothing if compile-time options are not specified
to enable logging.
