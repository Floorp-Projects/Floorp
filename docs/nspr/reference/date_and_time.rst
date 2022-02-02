This chapter describes the date and time functions in NSPR.

NSPR represents time in two ways, absolute time and clock/calendar time.
NSPR provides types and constants for both representations, and
functions to convert time values between the two.

-  Absolute time representation treats time instants as points along the
   time line. A time instant is represented by its position on the time
   line relative to the origin, called the epoch. NSPR defines the epoch
   to be midnight (00:00:00) 1 January 1970 UTC (Coordinated Universal
   Time). In this form, time is just a point on the time line. There is
   no notion of time zone.

-  Clock/calendar time, used for human interfaces, represents time in
   the familiar year, month, day, hour, minute, second components. In
   this form, the time zone information is important. For example,
   without specifying the time zone, the time 8:00AM 1 May 1998 is
   ambiguous. The NSPR data type for clock/calendar time, called an
   exploded time, has the time zone information in it, so that its
   corresponding point in absolute time is uniquely specified.

Note that absolute and clock times are not normally used in timing
operations. For functions that deal with the measurement of elapsed time
and with timeouts, see `Interval Timing <Interval_Timing>`__.

-  `Macros for Time Unit
   Conversion <#Macros_for_Time_Unit_Conversion>`__
-  `Types and Constants <#Types_and_Constants>`__
-  `Time Parameter Callback
   Functions <#Time_Parameter_Callback_Functions>`__
-  `Functions <#Functions>`__

.. _Macros_for_Time_Unit_Conversion:

Macros for Time Unit Conversion
-------------------------------

Macros for converting between seconds, milliseconds, microseconds, and
nanoseconds.

-  :ref:`PR_MSEC_PER_SEC`
-  :ref:`PR_USEC_PER_SEC`
-  :ref:`PR_NSEC_PER_SEC`
-  :ref:`PR_USEC_PER_MSEC`
-  :ref:`PR_NSEC_PER_MSEC`

.. _Types_and_Constants:

Types and Constants
-------------------

Types and constants defined for NSPR dates and times are:

-  :ref:`PRTime`
-  :ref:`PRTimeParameters`
-  :ref:`PRExplodedTime`

.. _Time_Parameter_Callback_Functions:

Time Parameter Callback Functions
---------------------------------

In some geographic locations, use of Daylight Saving Time (DST) and the
rule for determining the dates on which DST starts and ends have changed
a few times. Therefore, a callback function is used to determine time
zone information.

You can define your own time parameter callback functions, which must
conform to the definition :ref:`PRTimeParamFn`. Two often-used callback
functions of this type are provided by NSPR:

-  :ref:`PRTimeParamFn`
-  :ref:`PR_LocalTimeParameters` and :ref:`PR_GMTParameters`

Functions
---------

The functions that create and manipulate time and date values are:

-  :ref:`PR_Now`
-  :ref:`PR_ExplodeTime`
-  :ref:`PR_ImplodeTime`
-  :ref:`PR_NormalizeTime`
