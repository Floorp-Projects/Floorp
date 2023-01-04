PR_USEC_PER_SEC
===============

A convenience macro to improve code readability as well as to avoid
mistakes in counting the number of zeros; represents the number of
microseconds in a second.


Syntax
------

.. code::

    #include <prtime.h>

    #define PR_USEC_PER_SEC  1000000UL
