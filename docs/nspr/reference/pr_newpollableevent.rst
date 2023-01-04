PR_NewPollableEvent
===================

Create a pollable event file descriptor.


Syntax
------

.. code::

   NSPR_API(PRFileDesc *) PR_NewPollableEvent( void);


Parameter
~~~~~~~~~

None.


Returns
~~~~~~~

Pointer to :ref:`PRFileDesc` or ``NULL``, on error.
