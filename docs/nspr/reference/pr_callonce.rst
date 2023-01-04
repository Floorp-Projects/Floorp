PR_CallOnce
===========

Ensures that subsystem initialization occurs only once.


Syntax
------

.. code::

   PRStatus PR_CallOnce(
     PRCallOnceType *once,
     PRCallOnceFN func);


Parameters
~~~~~~~~~~

:ref:`PR_CallOnce` has these parameters:

``once``
   A pointer to an object of type :ref:`PRCallOnceType`. Initially (before
   any threading issues exist), the object must be initialized to all
   zeros. From that time on, the client should consider the object
   read-only (or even opaque) and allow the runtime to manipulate its
   content appropriately.
``func``
   A pointer to the function the calling client has designed to perform
   the subsystem initialization. The function will be called once, at
   most, for each subsystem to be initialized. It should return a
   :ref:`PRStatus` indicating the result of the initialization process.
   While the first thread executes this function, other threads
   attempting the same initialization will be blocked until it has been
   completed.
