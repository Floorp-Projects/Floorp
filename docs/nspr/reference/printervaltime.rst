PRIntervalTime
==============

A platform-dependent type that represents a monotonically increasing
integer--the NSPR runtime clock.


Syntax
------

.. code::

    #include <prinrval.h>

    typedef PRUint32 PRIntervalTime;

    #define PR_INTERVAL_MIN 1000UL
    #define PR_INTERVAL_MAX 100000UL

    #define PR_INTERVAL_NO_WAIT 0UL
    #define PR_INTERVAL_NO_TIMEOUT 0xffffffffUL


Description
-----------

The units of :ref:`PRIntervalTime` are platform-dependent. They are chosen
to be appropriate for the host OS, yet provide sufficient resolution and
period to be useful to clients.

The increasing interval value represented by :ref:`PRIntervalTime` wraps.
It should therefore never be used for intervals greater than
approximately 6 hours. Interval times are accurate regardless of host
processing requirements and are very cheap to acquire.

The constants ``PR_INTERVAL_MIN`` and ``PR_INTERVAL_MAX`` define a range
in ticks per second. These constants bound both the period and the
resolution of a :ref:`PRIntervalTime` object.

The reserved constants ``PR_INTERVAL_NO_WAIT`` and
``PR_INTERVAL_NO_TIMEOUT`` have special meaning for NSPR. They indicate
that the process should wait no time (return immediately) or wait
forever (never time out), respectively.

.. _Important_Note:

Important Note
~~~~~~~~~~~~~~

The counters used for interval times are allowed to overflow. Since the
sampling of the counter used to define an arbitrary epoch may have any
32-bit value, some care must be taken in the use of interval times. The
proper coding style to test the expiration of an interval is as follows:

.. code::

    if ((PRIntervalTime)(now - epoch) > interval)
    <... interval has expired ...>

As long as the interval and the elapsed time (now - epoch) do not exceed
half the namespace allowed by a :ref:`PRIntervalTime` (2\ :sup:`31`-1), the
expression shown above provides the expected result even if the signs of
now and epoch differ.

The resolution of a :ref:`PRIntervalTime` object is defined by the API.
NSPR guarantees that there will be at least 1000 ticks per second and
not more than 100000. At the maximum resolution of 10000 ticks per
second, each tick represents 1/100000 of a second. At that rate, a
32-bit register will overflow in approximately 28 hours, making the
maximum useful interval approximately 6 hours. Waiting on events more
than half a day in the future must therefore be based on a calendar
time.
