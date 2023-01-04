PR_MicrosecondsToInterval
=========================

Converts standard clock microseconds to platform-dependent intervals.


Syntax
------

.. code::

    #include <prinrval.h>

    PRIntervalTime PR_MicrosecondsToInterval(PRUint32 milli);


Parameter
~~~~~~~~~

The function has the following parameter:

``micro``
   The number of microseconds to convert to interval form.


Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``micro``
parameter.
