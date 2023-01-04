PRCallOnceType
==============

Structure for tracking initialization.


Syntax
------

.. code::

   #include <prinit.h>

   typedef struct PRCallOnceType {
     PRIntn initialized;
     PRInt32 inProgress;
     PRStatus status;
   } PRCallOnceType;


Fields
~~~~~~

The structure has these fields:

``initialized``
   If not zero, the initialization process has been completed.
``inProgress``
   If not zero, the initialization process is currently being executed.
   Calling threads that observe this status block until inProgress is
   zero.
``status``
   An indication of the outcome of the initialization process.


Description
-----------

The client is responsible for initializing the :ref:`PRCallOnceType`
structure to all zeros. This initialization must be accomplished before
any threading issues exist.
