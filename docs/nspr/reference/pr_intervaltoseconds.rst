PR_IntervalToSeconds
====================

Converts platform-dependent intervals to standard clock seconds.


Syntax
------

.. code::

    #include <prinrval.h>

    PRUint32 PR_IntervalToSeconds(PRIntervalTime ticks);


Parameter
~~~~~~~~~

``ticks``
   The number of platform-dependent intervals to convert.


Returns
~~~~~~~

Equivalent in seconds of the value passed in the ``ticks`` parameter.


Description
-----------

Conversion may cause overflow, which is not reported.
