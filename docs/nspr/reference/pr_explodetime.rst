PR_ExplodeTime
==============

Converts an absolute time to a clock/calendar time.


Syntax
------

.. code::

   #include <prtime.h>

   void PR_ExplodeTime(
      PRTime usecs,
      PRTimeParamFn params,
      PRExplodedTime *exploded);


Parameters
~~~~~~~~~~

The function has these parameters:

``usecs``
   An absolute time in the :ref:`PRTime` format.
``params``
   A time parameter callback function.
``exploded``
   A pointer to a location where the converted time can be stored. This
   location must be preallocated by the caller.


Returns
~~~~~~~

Nothing; the buffer pointed to by ``exploded`` is filled with the
exploded time.


Description
-----------

This function converts the specified absolute time to a clock/calendar
time in the specified time zone. Upon return, the location pointed to by
the exploded parameter contains the converted time value.
