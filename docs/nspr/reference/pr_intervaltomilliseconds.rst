PR_IntervalToMilliseconds
=========================

Converts platform-dependent intervals to standard clock milliseconds.


Syntax
------

.. code::

    #include <prinrval.h>

    PRUint32 PR_IntervalToMilliseconds(PRIntervalTime ticks);


Parameter
~~~~~~~~~

``ticks``
   The number of platform-dependent intervals to convert.


Returns
~~~~~~~

Equivalent in milliseconds of the value passed in the ``ticks``
parameter.


Description
-----------

Conversion may cause overflow, which is not reported.
