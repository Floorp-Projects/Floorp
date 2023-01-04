PR_GetErrorTextLength
=====================

Gets the length of the error text.


Syntax
------

.. code::

   #include <prerror.h>

   PRInt32 PR_GetErrorTextLength(void)


Returns
~~~~~~~

If a zero is returned, no error text is currently set. Otherwise, the
value returned is sufficient to contain the error text currently
available.
