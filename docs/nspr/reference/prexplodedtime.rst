PRExplodedTime
==============

A clock/calendar representation of times.


Syntax
------

.. code::

    #include <prtime.h>

    typedef struct PRExplodedTime {
        PRInt32 tm_usec;
        PRInt32 tm_sec;
        PRInt32 tm_min;
        PRInt32 tm_hour;
        PRInt32 tm_mday;
        PRInt32 tm_month;
        PRInt16 tm_year;
        PRInt8 tm_wday;
        PRInt16 tm_yday;
        PRTimeParameters tm_params;
    } PRExplodedTime;


Description
-----------

The :ref:`PRExplodedTime` structure represents clock/calendar time.
:ref:`PRExplodedTime` has the familiar time components: year, month, day of
month, hour, minute, second. It also has a microsecond component, as
well as the day of week and the day of year. In addition,
:ref:`PRExplodedTime` includes a :ref:`PRTimeParameters` structure
representing the local time zone information, so that the time point is
non-ambiguously specified.

The essential members of :ref:`PRExplodedTime` are:

 - :ref:`tm_year`: absolute year, AD (by "absolute," we mean if the year is
   2000, this field's value is 2000).
 - :ref:`tm_month`: number of months past tm_year. The range is [0, 11]. 0
   is January and 11 is December.
 - :ref:`tm_mday`: the day of month. The range is [1, 31]. Note that it
   starts from 1 as opposed to 0.
 - :ref:`tm_hour`: number of hours past tm_mday. The range is [0, 23].
 - :ref:`tm_min`: number of minutes past tm_hour. The range is [0, 59].
 - :ref:`tm_sec`: number of seconds past tm_min. The range is [0, 61]. The
   values 60 and 61 are for accommodating up to two leap seconds.
 - :ref:`tm_usec`: number of microseconds past tm_sec. The range is [0,
   999999].
 - :ref:`tm_params`: a `PRTimeParameters` structure representing the
   local time zone information.

The nonessential members of :ref:`PRExplodedTime` are:

 - :ref:`tm_wday`: day of week. The range is [0, 6]. 0 is Sunday, 1 is
   Monday, and 6 is Saturday.
 - :ref:`tm_yday`: day of year. The range is [0, 365]. 0 is the 1st of
   January.

On input to NSPR functions, only the essential members of
:ref:`PRExplodedTime` must be specified. The two nonessential members (day
of week and day of year) are ignored by NSPR functions as input. When an
NSPR function returns a :ref:`PRExplodedTime` object or sets a
:ref:`PRExplodedTime` object as output, all of the :ref:`PRExplodedTime`
members are set, including the nonessential members. You can also use
``PR_NormalizeTime()`` to calculate the values of the nonessential
members.
