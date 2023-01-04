PR_LocalTimeParameters
======================

Returns the time zone offset information that maps the specified
:ref:`PRExplodedTime` to local time.


Syntax
------

.. code::

   #include <prtime.h>

   PRTimeParameters PR_LocalTimeParameters (
      const PRExplodedTime *gmt);


Parameter
~~~~~~~~~

``gmt``
   A pointer to the clock/calendar time whose offsets are to be
   determined. This time should be specified in GMT.


Returns
~~~~~~~

A time parameters structure that expresses the time zone offsets at the
specified time.


Description
-----------

This is a frequently-used time parameter callback function. You don't
normally call it directly; instead, you pass it as a parameter to
``PR_ExplodeTime()`` or ``PR_NormalizeTime()``.
