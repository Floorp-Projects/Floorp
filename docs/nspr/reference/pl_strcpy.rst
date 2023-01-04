PL_strcpy
=========


Copies a string, up to and including the trailing ``'\0'``, into a
destination buffer.


Syntax
~~~~~~

.. code::

   char * PL_strcpy(char *dest, const char *src);


Parameters
~~~~~~~~~~

The function has these parameters:

``dest``
   Pointer to a buffer. On output, the buffer contains a copy of the
   string passed in src.
``src``
   Pointer to the string to be copied.


Returns
~~~~~~~

The function returns a pointer to the buffer specified by the ``dest``
parameter.


Description
~~~~~~~~~~~

If the string specified by ``src`` is longer than the buffer specified
by ``dest``, the buffer will not be null-terminated.
