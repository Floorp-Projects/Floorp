PRTimeParameters
================

A representation of time zone information.


Syntax
------

.. code::

    #include <prtime.h>

    typedef struct PRTimeParameters {
        PRInt32 tp_gmt_offset;
        PRInt32 tp_dst_offset;
    } PRTimeParameters;


Description
-----------

Each geographic location has a standard time zone, and if Daylight
Saving Time (DST) is practiced, a daylight time zone. The
:ref:`PRTimeParameters` structure represents the local time zone
information in terms of the offset (in seconds) from GMT. The overall
offset is broken into two components:

``tp_gmt_offset``
   The offset of the local standard time from GMT.

``tp_dst_offset``
   If daylight savings time (DST) is in effect, the DST adjustment from
   the local standard time. This is most commonly 1 hour, but may also
   be 30 minutes or some other amount. If DST is not in effect, the
   tp_dst_offset component is 0.

For example, the US Pacific Time Zone has both a standard time zone
(Pacific Standard Time, or PST) and a daylight time zone (Pacific
Daylight Time, or PDT).

-  In PST, the local time is 8 hours behind GMT, so ``tp_gmt_offset`` is
   -28800 seconds. ``tp_dst_offset`` is 0, indicating that daylight
   saving time is not in effect.

-  In PDT, the clock is turned forward by one hour, so the local time is
   7 hours behind GMT. This is broken down as -8 + 1 hours, so
   ``tp_gmt_offset`` is -28800 seconds, and ``tp_dst_offset`` is 3600
   seconds.

A second example is Japan, which is 9 hours ahead of GMT. Japan does not
use daylight saving time, so the only time zone is Japan Standard Time
(JST). In JST ``tp_gmt_offset`` is 32400 seconds, and ``tp_dst_offset``
is 0.
