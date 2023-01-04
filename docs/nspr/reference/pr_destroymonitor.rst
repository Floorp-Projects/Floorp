PR_DestroyMonitor
=================

Destroys a monitor object.


Syntax
------

.. code::

   #include <prmon.h>

   void PR_DestroyMonitor(PRMonitor *mon);


Parameter
~~~~~~~~~

The function has the following parameter:

``mon``
   A reference to an existing structure of type :ref:`PRMonitor`.


Description
-----------

The caller is responsible for guaranteeing that the monitor is no longer
in use before calling :ref:`PR_DestroyMonitor`. There must be no thread
(including the calling thread) in the monitor or waiting on the monitor.
