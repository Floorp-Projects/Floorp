PR_strtod
=========

Converts the prefix of a decimal string to the nearest double-precision
floating point number.


Syntax
------

.. code::

   #include <prdtoa.h>

   PRFloat64 PR_strtod(const char *s00, char **se);


Parameters
~~~~~~~~~~

The function has these parameters:

``s00``
   The input string to be scanned.
``se``
   A pointer that, if not ``NULL``, will be assigned the address of the
   last character scanned in the input string.


Returns
~~~~~~~

The result of the conversion is a ``PRFloat64`` value equivalent to the
input string. If the parameter ``se`` is not ``NULL`` the location it
references is also set.


Description
-----------

:ref:`PR_strtod` converts the prefix of the input decimal string pointed to
by ``s00`` to a nearest double-precision floating point number. Ties are
broken by the IEEE round-even rule. The string is scanned up to the
first unrecognized character. If the value of ``se`` is not
(``char **``) ``NULL``, :ref:`PR_strtod` stores a pointer to the character
terminating the scan in ``*se``. If the answer would overflow, a
properly signed ``HUGE_VAL`` (infinity) is returned. If the answer would
underflow, a properly signed 0 is returned. In both cases,
``PR_GetError()`` returns the error code ``PR_RANGE_ERROR``. If no
number can be formed, ``se`` is set to ``s00``, and 0 is returned.
