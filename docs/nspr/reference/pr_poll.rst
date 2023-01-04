PR_Poll
=======

Detects when I/O is ready for a set of socket file descriptors.


Syntax
------

.. code::

   #include <prio.h>

   PRInt32 PR_Poll(
     PRPollDesc *pds,
     PRIntn npds,
     PRIntervalTime timeout);


Parameters
~~~~~~~~~~

The function has the following parameters:

``pds``
   A pointer to the first element of an array of ``PRPollDesc``
   structures.
``npds``
   The number of elements in the ``pds`` array. If this parameter is
   zero, :ref:`PR_Poll` is equivalent to :ref:`PR_Sleep` with a timeout.
``timeout``
   Amount of time the call will block waiting for I/O to become ready.
   If this time expires without any I/O becoming ready, :ref:`PR_Poll`
   returns zero.


Returns
~~~~~~~

The function returns one of these values:

-  If successful, the function returns a positive number indicating the
   number of ``PRPollDesc`` structures in ``pds`` that have events.
-  The value 0 indicates the function timed out.
-  The value -1 indicates the function failed. The reason for the
   failure can be obtained by calling :ref:`PR_GetError`.


Description
~~~~~~~~~~~

This function returns as soon as I/O is ready on one or more of the
underlying socket objects. A count of the number of ready descriptors is
returned unless a timeout occurs, in which case zero is returned.

The ``in_flags`` field of the ``PRPollDesc`` data structure should be
set to the I/O events (readable, writable, exception, or some
combination) that the caller is interested in. On successful return, the
``out_flags`` field of the ``PRPollDesc`` data structure is set to
indicate what kind of I/O is ready on the respective descriptor.
:ref:`PR_Poll` uses the ``out_flags`` fields as scratch variables during
the call. If :ref:`PR_Poll` returns 0 or -1, the ``out_flags`` fields do
not contain meaningful values and must not be used.

The ``PRPollDesc`` structure is defined as follows:

.. code::

   struct PRPollDesc {
     PRFileDesc* fd;
     PRInt16 in_flags;
     PRInt16 out_flags;
   };

   typedef struct PRPollDesc PRPollDesc;

The structure has the following fields:

``fd``
   A pointer to a :ref:`PRFileDesc` object representing a socket or a
   pollable event. This field can be set to ``NULL`` to indicate to
   :ref:`PR_Poll` that this ``PRFileDesc object`` should be ignored.

   .. note::

      On Unix, the ``fd`` field can be set to a pointer to any
      :ref:`PRFileDesc` object, including one representing a file or a
      pipe. Cross-platform applications should only set the ``fd`` field
      to a pointer to a :ref:`PRFileDesc` object representing a socket or a
      pollable event because on Windows the ``select`` function can only
      be used with sockets.
``in_flags``
   A bitwise ``OR`` of the following bit flags:

 - :ref:`PR_POLL_READ`: ``fd`` is readable.
 - :ref:`PR_POLL_WRITE`: ``fd`` is writable.
 - :ref:`PR_POLL_EXCEPT`: ``fd`` has an exception condition.

``out_flags``
   A bitwise ``OR`` of the following bit flags:

 - :ref:`PR_POLL_READ`
 - :ref:`PR_POLL_WRITE`
 - :ref:`PR_POLL_EXCEPT`
 - :ref:`PR_POLL_ERR`: ``fd`` has an error.
 - :ref:`PR_POLL_NVAL`: ``fd`` is bad.

Note that the ``PR_POLL_ERR`` and ``PR_POLL_NVAL`` flags are used only
in ``out_flags``. The ``PR_POLL_ERR`` and ``PR_POLL_NVAL`` events are
always reported by :ref:`PR_Poll`.
