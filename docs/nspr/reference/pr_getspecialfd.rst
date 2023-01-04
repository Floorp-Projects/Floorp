PR_GetSpecialFD
===============

Gets the file descriptor that represents the standard input, output, or
error stream.


Syntax
------

.. code::

   #include <prio.h>

   PRFileDesc* PR_GetSpecialFD(PRSpecialFD id);


Parameter
~~~~~~~~~

The function has the following parameter:

``id``
   A pointer to an enumerator of type ``PRSpecialFD``, indicating the
   type of I/O stream desired: ``PR_StandardInput``,
   ``PR_StandardOutput``, or ``PR_StandardError``.


Returns
~~~~~~~

If the ``id`` parameter is valid, :ref:`PR_GetSpecialFD` returns a file
descriptor that represents the corresponding standard I/O stream.
Otherwise, :ref:`PR_GetSpecialFD` returns ``NULL`` and sets the error to
``PR_INVALID_ARGUMENT_ERROR``.


Description
-----------

Type ``PRSpecialFD`` is defined as follows:

.. code::

   typedef enum PRSpecialFD{
      PR_StandardInput,
      PR_StandardOutput,
      PR_StandardError
   } PRSpecialFD;

``#define PR_STDIN PR_GetSpecialFD(PR_StandardInput)``
``#define PR_STDOUT PR_GetSpecialFD(PR_StandardOutput)``
``#define PR_STDERR PR_GetSpecialFD(PR_StandardError)``

File descriptors returned by :ref:`PR_GetSpecialFD` are owned by the
runtime and should not be closed by the caller.
