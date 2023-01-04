PR_dtoa
=======

Converts a floating point number to a string.


Syntax
------

.. code::

   #include <prdtoa.h>

   PRStatus PR_dtoa(
      PRFloat64 d,
      PRIntn mode,
      PRIntn ndigits,
      PRIntn *decpt,
      PRIntn *sign,
      char **rve,
      char *buf,
      PRSize bufsz);


Parameters
~~~~~~~~~~

The function has these parameters:

``d``
   The floating point number to be converted to a string.
``mode``
   The type of conversion to employ.
``ndigits``
   The number of digits desired in the output string.
``decpt``
   A pointer to a memory location where the runtime will store the
   offset, relative to the beginning of the output string, of the
   conversion's decimal point.
``sign``
   A location where the runtime can store an indication that the
   conversion was of a negative value.
``*rve``
   If not ``NULL`` this location is set to the address of the end of the
   result.
``buf``
   The address of the buffer in which to store the result.
``bufsz``
   The size of the buffer provided to hold the result.

Results
~~~~~~~

The principle output is the null-terminated string stored in ``buf``. If
``rve`` is not ``NULL``, ``*rve`` is set to point to the end of the
returned value.


Description
-----------

This function converts the specified floating point number to a string,
using the method specified by ``mode``. Possible modes are:

``0``
   Shortest string that yields ``d`` when read in and rounded to
   nearest.
``1``
   Like 0, but with Steele & White stopping rule. For example, with IEEE
   754 arithmetic, mode 0 gives 1e23 whereas mode 1 gives
   9.999999999999999e22.
``2``
   ``max(1, ndigits)`` significant digits. This gives a return value
   similar to that of ``ecvt``, except that trailing zeros are
   suppressed.
``3``
   Through ``ndigits`` past the decimal point. This gives a return value
   similar to that from ``fcvt``, except that trailing zeros are
   suppressed, and ``ndigits`` can be negative.
``4,5,8,9``
   Same as modes 2 and 3, but using\ *left to right* digit generation.
``6-9``
   Same as modes 2 and 3, but do not try fast floating-point estimate
   (if applicable).
``all others``
   Treated as mode 2.

Upon return, the buffer specified by ``buf`` and ``bufsz`` contains the
converted string. Trailing zeros are suppressed. Sufficient space is
allocated to the return value to hold the suppressed trailing zeros.

If the input parameter ``d`` is\ *+Infinity*,\ *-Infinity* or\ *NAN*,
``*decpt`` is set to 9999.
