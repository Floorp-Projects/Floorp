PRLinger
========

Structure used with the ``PR_SockOpt_Linger`` socket option to specify
the time interval (in :ref:`PRIntervalTime` units) to linger on closing a
socket if any data remain in the socket send buffer.


Syntax
~~~~~~

.. code::

   #include <prio.h>

   typedef struct PRLinger {
     PRBool polarity;
     PRIntervalTime linger;
   } PRLinger;


Fields
~~~~~~

The structure has the following fields:

``polarity``
   Polarity of the option's setting: ``PR_FALSE`` means the option is
   off, in which case the value of ``linger`` is ignored. ``PR_TRUE``
   means the option is on, and the value of ``linger`` will be used to
   determine how long :ref:`PR_Close` waits before returning.
``linger``
   Time (in :ref:`PRIntervalTime` units) to linger before closing if any
   data remain in the socket send buffer.


Description
~~~~~~~~~~~

By default, :ref:`PR_Close` returns immediately, but if there are any data
remaining in the socket send buffer, the system attempts to deliver the
data to the peer. The ``PR_SockOpt_Linger`` socket option, with a value
represented by a structure of type :ref:`PRLinger`, makes it possible to
change this default as follows:

-  If ``polarity`` is set to ``PR_FALSE``, :ref:`PR_Close` returns
   immediately, but if there are any data remaining in the socket send
   buffer, the runtime attempts to deliver the data to the peer.
-  If ``polarity`` is set to ``PR_TRUE`` and ``linger`` is set to 0
   (``PR_INTERVAL_NO_WAIT``), the runtime aborts the connection when it
   is closed and discards any data remaining in the socket send buffer.
-  If ``polarity`` is set to ``PR_TRUE`` and ``linger`` is nonzero, the
   runtime *lingers* when the socket is closed. That is, if any data
   remains in the socket send buffer, :ref:`PR_Close` blocks until either
   all the data is sent and acknowledged by the peer or the interval
   specified by ``linger`` expires.
