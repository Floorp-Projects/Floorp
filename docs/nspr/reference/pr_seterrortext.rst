PR_SetErrorText
===============

Sets the text associated with an error.


Syntax
------

.. code::

   #include <prerror.h>

   void PR_SetErrorText(PRIntn textLength, const char *text)


Parameters
~~~~~~~~~~

The function has these parameters:

``textLength``
   The length of the text in the ``text``. May be ``NULL``. If not
   ``NULL``, and if ``text`` is zero, the string is assumed to be a
   null-terminated C string. Otherwise the text is assumed to be the
   length specified and to possibly include ``NULL`` characters (as
   might occur in a multilingual string).

``text``
   The text to associate with the error.


Description
-----------

The text is copied into the thread structure and remains there until the
next call to :ref:`PR_SetError`. If there is error text already present in
the thread, the previous value is first deleted. The new value is copied
into storage allocated and owned by NSPR and remains there until the
next call to :ref:`PR_SetError` or another call to :ref:`PR_SetErrorText`.

NSPR makes no use of this function. Clients may use it for their own
purposes.
