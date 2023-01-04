PRTime
======

A representation of absolute times.


Syntax
------

.. code::

    #include <prtime.h>

    typedef PRInt64 PRTime;


Description
-----------

This type is a 64-bit integer representing the number of microseconds
since the NSPR epoch, midnight (00:00:00) 1 January 1970 Coordinated
Universal Time (UTC). A time after the epoch has a positive value, and a
time before the epoch has a negative value.

In NSPR, we use the more familiar term Greenwich Mean Time (GMT) in
place of UTC. Although UTC and GMT are not exactly the same in their
precise definitions, they can generally be treated as if they were.

.. note::

   **Note:** Keep in mind that while :ref:`PRTime` stores times in
   microseconds since epoch, JavaScript date objects store times in
   milliseconds since epoch.
