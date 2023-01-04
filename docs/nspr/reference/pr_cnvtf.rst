PR_cnvtf
========

Converts a floating point number to a string.


Syntax
------

.. code::

   #include <prdtoa.h>

   void PR_cnvtf (
     char *buf,
     PRIntn bufsz,
     PRIntn prcsn,
     PRFloat64 fval);


Parameters
~~~~~~~~~~

The function has these parameters:

``buf``
   The address of the buffer in which to store the result.
``bufsz``
   The size of the buffer provided to hold the result.
``prcsn``
   The number of digits of precision to which to generate the floating
   point value.
``fval``
   The double-precision floating point number to be converted.


Description
-----------

:ref:`PR_cnvtf` is a simpler interface to convert a floating point number
to a string. It conforms to the ECMA standard of Javascript
(ECMAScript).

On return, the result is written to the buffer pointed to by ``buf`` of
size ``bufsz``.
