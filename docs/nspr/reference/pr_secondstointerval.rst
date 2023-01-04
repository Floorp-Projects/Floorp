PR_SecondsToInterval
====================

Converts standard clock seconds to platform-dependent intervals.


Syntax
------

.. code::

    #include <prinrval.h>

    PRIntervalTime PR_SecondsToInterval(PRUint32 seconds);


Parameter
~~~~~~~~~

The function has the following parameter:

``seconds``
   The number of seconds to convert to interval form.


Returns
~~~~~~~

Platform-dependent equivalent of the value passed in the ``seconds``
parameter.
