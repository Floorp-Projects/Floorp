PR_NSEC_PER_MSEC
================

A convenience macro to improve code readability as well as to avoid
mistakes in counting the number of zeros; represents the number of
nanoseconds in a millisecond.


Syntax
------

.. code::

    #include <prtime.h>

    #define PR_NSEC_PER_MSEC 1000000UL
