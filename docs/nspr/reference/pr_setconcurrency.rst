PR_SetConcurrency
=================

Creates extra virtual processor threads. Generally used with MP systems.


Syntax
------

.. code::

   #include <prinit.h>

   void PR_SetConcurrency(PRUintn numCPUs);


Parameter
~~~~~~~~~

:ref:`PR_SetConcurrency` has one parameter:

``numCPUs``
   The number of extra virtual processor threads to be created.


Description
-----------

Setting concurrency controls the number of virtual processors that NSPR
uses to implement its ``M x N`` threading model. The ``M x N`` model is
not available on all host systems. On those where it is not available,
:ref:`PR_SetConcurrency` is ignored.

Virtual processors are actually\ *global* threads, each of which is
designed to support an arbitrary number of\ *local* threads. Since
global threads are scheduled by the host operating system, this model is
particularly applicable to multiprocessor architectures, where true
parallelism is possible. However, it may also prove advantageous on
uniprocessor systems to reduce the impact of having a locally scheduled
thread calling incidental blocking functions. In such cases, all the
threads being supported by the virtual processor will block, but those
assigned to another virtual processor will be unaffected.
