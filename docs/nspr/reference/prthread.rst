PRThread
========

An NSPR thread.


Syntax
------

.. code::

   #include <prthread.h>

   typedef struct PRThread PRThread;


Description
~~~~~~~~~~~

In NSPR, a thread is represented by a pointer to an opaque structure of
type :ref:`PRThread`. This pointer is a required parameter for most of the
functions that operate on threads.

A ``PRThread*`` is the successful result of creating a new thread. The
identifier remains valid until it returns from its root function and, if
the thread was created joinable, is joined.
