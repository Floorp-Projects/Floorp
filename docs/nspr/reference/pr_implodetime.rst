PR_ImplodeTime
==============

Converts a clock/calendar time to an absolute time.


Syntax
------

.. code::

   #include <prtime.h>

   PRTime PR_ImplodeTime(const PRExplodedTime *exploded);


Parameters
~~~~~~~~~~

The function has these parameters:

``exploded``
   A pointer to the clock/calendar time to be converted.


Returns
~~~~~~~

An absolute time value.


Description
-----------

This function converts the specified clock/calendar time to an absolute
time and returns the converted time value.
