PR_IntervalToMicroseconds
=========================

Converts platform-dependent intervals to standard clock microseconds.


Syntax
------

.. code::

    #include <prinrval.h>

    PRUint32 PR_IntervalToMicroseconds(PRIntervalTime ticks);


Parameter
~~~~~~~~~

``ticks``
   The number of platform-dependent intervals to convert.


Returns
~~~~~~~

Equivalent in microseconds of the value passed in the ``ticks``
parameter.


Description
-----------

Conversion may cause overflow, which is not reported.
