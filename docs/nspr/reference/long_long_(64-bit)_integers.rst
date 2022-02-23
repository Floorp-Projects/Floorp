Long Long integers
==================

This chapter describes the global functions you use to perform 64-bit
integer operations. The functions define a portable API that can be used
reliably in any environment. Where 64-bit integers are desired, use of
NSPR's implementation is recommended to ensure cross-platform
compatibility.

Most of the 64-bit integer operations are implemented as macros. The
specific implementation of each macro depends on whether the compiler
for the target platform supports 64-bit integers. For a specific target
platform, if 64-bit integers are supported for that platform, define
``HAVE_LONG_LONG`` at compile time.

.. _64-Bit_Integer_Types:

64-Bit Integer Types
~~~~~~~~~~~~~~~~~~~~

NSPR provides two types to represent 64-bit integers:

-  :ref:`PRInt64`
-  :ref:`PRUint64`

.. _64-Bit_Integer_Functions:

64-Bit Integer Functions
~~~~~~~~~~~~~~~~~~~~~~~~

The API defined for the 64-bit integer functions is consistent across
all supported platforms.
