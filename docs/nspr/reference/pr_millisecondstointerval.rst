PR_MillisecondsToInterval
=========================

Converts standard clock milliseconds to platform-dependent intervals.


Syntax
------

.. code::

    #include <prinrval.h>

    PRIntervalTime PR_MillisecondsToInterval(PRUint32 milli);


Parameter
~~~~~~~~~

The function has the following parameter:

``milli``
   The number of milliseconds to convert to interval form.


Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``milli``
parameter.
