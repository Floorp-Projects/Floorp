PRThreadPriority
================

A thread's priority setting.


Syntax
------

.. code::

   #include <prthread.h>

   typedef enum PRThreadPriority
   {
      PR_PRIORITY_FIRST   = 0,
      PR_PRIORITY_LOW     = 0,
      PR_PRIORITY_NORMAL  = 1,
      PR_PRIORITY_HIGH    = 2,
      PR_PRIORITY_URGENT  = 3,
      PR_PRIORITY_LAST    = 3
   } PRThreadPriority;


Enumerators
~~~~~~~~~~~

``PR_PRIORITY_FIRST``
   Placeholder.
``PR_PRIORITY_LOW``
   The lowest possible priority. This priority is appropriate for
   threads that are expected to perform intensive computation.
``PR_PRIORITY_NORMAL``
   The most commonly expected priority.
``PR_PRIORITY_HIGH``
   Slightly higher priority than ``PR_PRIORITY_NORMAL``. This priority
   is for threads performing work of high urgency but short duration.
``PR_PRIORITY_URGENT``
   Highest priority. Only one thread at a time typically has this
   priority.
``PR_PRIORITY_LAST``
   Placeholder


Description
-----------

In general, an NSPR thread of higher priority has a statistically better
chance of running relative to threads of lower priority. However,
because of the multiple strategies NSPR uses to implement threading on
various host platforms, NSPR priorities are not precisely defined. At
best they are intended to specify a preference in the amount of CPU time
that a higher-priority thread might expect relative to a lower-priority
thread. This preference is still subject to resource availability and
must not be used in place of proper synchronization.


See Also
--------

`Setting Thread
Priorities <Introduction_to_NSPR#Setting_Thread_Priorities>`__.
