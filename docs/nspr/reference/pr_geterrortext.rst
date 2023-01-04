PR_GetErrorText
===============

Copies the current thread's current error text without altering the text
as stored in the thread's context.


Syntax
------

.. code::

   #include <prerror.h>

   PRInt32 PR_GetErrorText(char *text);


Parameters
~~~~~~~~~~

The function has one parameter:

``text``
   On output, the array pointed to contains the thread's current error
   text.


Returns
-------

The actual number of bytes copied. If the result is zero, ``text`` is
unaffected.
