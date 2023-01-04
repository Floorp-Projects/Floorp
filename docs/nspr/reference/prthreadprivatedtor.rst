PRThreadPrivateDTOR
===================

The destructor function passed to PR_NewThreadPrivateIndex that is
associated with the resulting thread private index.


Syntax
------

.. code::

   #include <prthread.h>

   typedef void (PR_CALLBACK *PRThreadPrivateDTOR)(void *priv);


Description
~~~~~~~~~~~

Until the data associated with an index is actually set with a call to
:ref:`PR_SetThreadPrivate`, the value of the data is ``NULL``. If the data
associated with the index is not ``NULL``, NSPR passes a reference to
the data to the destructor function when the thread terminates.
