PR_NormalizeTime
================

Adjusts the fields of a clock/calendar time to their proper ranges,
using a callback function.


Syntax
------

.. code::

   #include <prtime.h>

   void PR_NormalizeTime (
     PRExplodedTime *time,
     PRTimeParamFn params);


Parameters
~~~~~~~~~~

The function has these parameters:

``time``
   A pointer to a clock/calendar time in the :ref:`PRExplodedTime` format.
``params``
   A time parameter callback function.


Returns
~~~~~~~

Nothing; the ``time`` parameter is altered by the callback function.


Description
-----------

This function adjusts the fields of the specified time structure using
the specified time parameter callback function, so that they are in the
proper range.

Call this function in these situations:

-  To normalize a time after performing arithmetic operations directly
   on the field values of the calendar time object. For example, if you
   have a ``]`` object that represents the date 3 March 1998 and you
   want to say "forty days from 3 March 1998", you can simply add 40 to
   the ``tm_mday`` field and then call ``PR_NormalizeTime()``.

-  To calculate the optional field values ``tm_wday`` and ``tm_yday``.
   For example, suppose you want to compute the day of week for 3 March
   1998. You can set ``tm_mday`` to 3, ``tm_month`` to 2, and
   ``tm_year`` to 1998, and all the other fields to 0, then call
   ``PR_NormalizeTime()`` with :ref:`PR_GMTParameters`. On return,
   ``tm_wday`` (and ``tm_yday``) are set for you.

-  To convert from one time zone to another. For example, if the input
   argument time is in time zone A and the input argument ``params``
   represents time zone B, when ``PR_NormalizeTime()`` returns, time
   will be in time zone B.
