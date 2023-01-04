PR_CreateThreadPool
===================

Create a new hash table.


Syntax
------

.. code::

   #include <prtpool.h>

   NSPR_API(PRThreadPool *)
   PR_CreateThreadPool(
     PRInt32 initial_threads,
     PRInt32 max_threads,
     PRUint32 stacksize
   );


Parameters
~~~~~~~~~~

The function has the following parameters:

``initial_threads``
   The number of threads to be created within this thread pool.
``max_threads``
   The limit on the number of threads that will be created to server the
   thread pool.
``stacksize``
   Size of the stack allocated to each thread in the thread.


Returns
~~~~~~~

Pointer to a :ref:`PRThreadPool` structure or ``NULL`` on error.


Description
~~~~~~~~~~~
