PR_Cleanup
==========

Coordinates a graceful shutdown of NSPR.


Syntax
------

.. code::

   #include <prinit.h>

   PRStatus PR_Cleanup(void);


Returns
~~~~~~~

The function returns one of the following values:

-  If NSPR has been shut down successfully, ``PR_SUCCESS``.
-  If the calling thread of this function is not the primordial thread,
   ``PR_FAILURE``.


Description
-----------

:ref:`PR_Cleanup` must be called by the primordial thread near the end of
the ``main`` function.

:ref:`PR_Cleanup` attempts to synchronize the natural termination of the
process. It does so by blocking the caller, if and only if it is the
primordial thread, until all user threads have terminated. When the
primordial thread returns from ``main``, the process immediately and
silently exits. That is, the process (if necessary) forcibly terminates
any existing threads and exits without significant blocking and without
error messages or core files.
