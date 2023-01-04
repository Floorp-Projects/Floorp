PR_IntervalNow
==============

Returns the value of NSPR's free-running interval timer.


Syntax
------

.. code::

    #include <prinrval.h>

    PRIntervalTime PR_IntervalNow(void);


Returns
~~~~~~~

A :ref:`PRIntervalTime` object.


Description
-----------

You can use the value returned by ``PR_IntervalNow()`` to establish
epochs and to determine intervals (that is, compute the difference
between two times). ``PR_IntervalNow()`` is both very efficient and
nonblocking, so it is appropriate to use (for example) while holding a
mutex.

The most common use for ``PR_IntervalNow()`` is to establish an epoch
and test for the expiration of intervals. In this case, you typically
call ``PR_IntervalNow()`` in a sequence that looks like this:

.. code::

    PRUint32 interval = ... ; // milliseconds
    // ...
    PRStatus rv;
    PRIntervalTime epoch = PR_IntervalNow();
    PR_Lock(data->mutex);
    while (!EvaluateData(data))  /* wait until condition is met */
    {
        PRUint32 delta = PR_IntervalToMilliseconds(PR_IntervalNow() - epoch);
        if (delta > interval) break;  /* timeout */
        rv = PR_Wait(data->condition, PR_MillisecondsToInterval(interval - delta));
        if (PR_FAILURE == rv) break;  /* likely an interrupt */
    }
    PR_Unlock(data->mutex);
