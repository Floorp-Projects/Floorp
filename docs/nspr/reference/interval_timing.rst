NSPR defines a platform-dependent type, :ref:`PRIntervalTime`, for timing
intervals of fewer than approximately 6 hours. This chapter describes
:ref:`PRIntervalTime` and the functions that allow you to use it for timing
purposes:

-  `Interval Time Type and
   Constants <#Interval_Time_Type_and_Constants>`__
-  `Interval Functions <#Interval_Functions>`__

.. _Interval_Time_Type_and_Constants:

Interval Time Type and Constants
--------------------------------

All timed functions in NSPR require a parameter that depicts the amount
of time allowed to elapse before the operation is declared failed. The
type of such arguments is :ref:`PRIntervalTime`. Such parameters are common
in NSPR functions such as those used for I/O operations and operations
on condition variables.

NSPR 2.0 provides interval times that are efficient in terms of
performance and storage requirements. Conceptually, they are based on
free-running counters that increment at a fixed rate without possibility
of outside influence (as might be observed if one was using a
time-of-day clock that gets reset due to some administrative action).
The counters have no fixed epoch and have a finite period. To make use
of these counters, the application must declare a point in time, the
epoch, and an amount of time elapsed since that **epoch**, the
**interval**. In almost all cases the epoch is defined as the value of
the interval timer at the time it was sampled.

 - :ref:`PRIntervalTime`

.. _Interval_Functions:

Interval Functions
------------------

Interval timing functions are divided into three groups:

-  `Getting the Current Interval and Ticks Per
   Second <#Getting_the_Current_Interval_and_Ticks_Per_Second>`__
-  `Converting Standard Clock Units to Platform-Dependent
   Intervals <#Converting_Standard_Clock_Units_to_Platform-Dependent_Intervals>`__
-  `Converting Platform-Dependent Intervals to Standard Clock
   Units <#Converting_Platform-Dependent_Intervals_to_Standard_Clock_Units>`__

.. _Getting_the_Current_Interval_and_Ticks_Per_Second:

Getting the Current Interval and Ticks Per Second
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_IntervalNow`
 - :ref:`PR_TicksPerSecond`

.. _Converting_Standard_Clock_Units_to_Platform-Dependent_Intervals:

Converting Standard Clock Units to Platform-Dependent Intervals
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_SecondsToInterval`
 - :ref:`PR_MillisecondsToInterval`
 - :ref:`PR_MicrosecondsToInterval`

.. _Converting_Platform-Dependent_Intervals_to_Standard_Clock_Units:

Converting Platform-Dependent Intervals to Standard Clock Units
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_IntervalToSeconds`
 - :ref:`PR_IntervalToMilliseconds`
 - :ref:`PR_IntervalToMicroseconds`
