PR_ClearInterrupt
=================

Clears the interrupt request for the calling thread.


Syntax
------

.. code::

   #include <prthread.h>

   void PR_ClearInterrupt(void);


Description
-----------

Interrupting is a cooperative process, so it's possible that the thread
passed to :ref:`PR_Interrupt` may never respond to the interrupt request.
For example, the target thread may reach the agreed-on control point
without providing an opportunity for the runtime to notify the thread of
the interrupt request. In this case, the request for interrupt is still
pending with the thread and must be explicitly canceled. Therefore it is
sometimes necessary to call :ref:`PR_ClearInterrupt` to clear a previous
interrupt request.

If no interrupt request is pending, :ref:`PR_ClearInterrupt` is a no-op.
