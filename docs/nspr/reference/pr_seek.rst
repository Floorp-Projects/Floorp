PR_Seek
=======

Moves the current read-write file pointer by an offset expressed as a
32-bit integer.

.. container:: blockIndicator deprecated deprecatedHeader

   | **Deprecated**
   | This feature is no longer recommended. Though some browsers might
     still support it, it may have already been removed from the
     relevant web standards, may be in the process of being dropped, or
     may only be kept for compatibility purposes. Avoid using it, and
     update existing code if possible; see the `compatibility
     table <#Browser_compatibility>`__ at the bottom of this page to
     guide your decision. Be aware that this feature may cease to work
     at any time.

Deprecated in favor of :ref:`PR_Seek64`.


Syntax
~~~~~~

.. code::

   #include <prio.h>

   PRInt32 PR_Seek(
     PRFileDesc *fd,
     PRInt32 offset,
     PRSeekWhence whence);


Parameters
~~~~~~~~~~

The function has the following parameters:

``fd``
   A pointer to a :ref:`PRFileDesc` object.
``offset``
   A value, in bytes, used with the whence parameter to set the file
   pointer. A negative value causes seeking in the reverse direction.
``whence``
   A value of type :ref:`PRSeekWhence` that specifies how to interpret the
   ``offset`` parameter in setting the file pointer associated with the
   fd parameter. The value for the ``whence`` parameter can be one of
   the following:

    - :ref:`PR_SEEK_SET`. Sets the file pointer to the value of the
      ``offset`` parameter.
    - :ref:`PR_SEEK_CUR`. Sets the file pointer to its current location
      plus the value of the ``offset`` parameter.
    - :ref:`PR_SEEK_END`. Sets the file pointer to the size of the file
      plus the value of the ``offset`` parameter.


Returns
~~~~~~~

The function returns one of the following values:

-  If the function completes successfully, it returns the resulting file
   pointer location, measured in bytes from the beginning of the file.
-  If the function fails, the file pointer remains unchanged and the
   function returns -1. The error code can then be retrieved with
   :ref:`PR_GetError`.


Description
-----------

Here's an idiom for obtaining the current location of the file pointer
for the file descriptor ``fd``:

``PR_Seek(fd, 0, PR_SEEK_CUR)``


See Also
--------

If you need to move the file pointer by a large offset that's out of the
range of a 32-bit integer, use :ref:`PR_Seek64`. New code should use
:ref:`PR_Seek64` so that it can handle files larger than 2 GB.
