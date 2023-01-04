This chapter describes the most common NSPR types. Other chapters
describe more specialized types when describing the functions that use
them.

-  `Calling Convention Types <#Calling_Convention_Types>`__ are used for
   externally visible functions and globals.
-  `Algebraic Types <#Algebraic_Types>`__ of various lengths are used
   for integer algebra.
-  `Miscellaneous Types <#Miscellaneous_Types>`__ are used for
   representing size, pointer difference, Boolean values, and return
   values.

For information on naming conventions for NSPR types, functions, and
macros, see `NSPR Naming
Conventions <Introduction_to_NSPR#NSPR_Naming_Conventions>`__.

.. _Calling_Convention_Types:

Calling Convention Types
------------------------

These types are used to support cross-platform declarations of
prototypes and implementations:

 - :ref:`PR_EXTERN` is used for declarations of external functions or
   variables.
 - :ref:`PR_IMPLEMENT` is used for definitions of external functions or
   variables.
 - :ref:`PR_CALLBACK` is used for definitions and declarations of functions
   that are called via function pointers. A typical example is a
   function implemented in an application but called from a shared
   library.

Here are some simple examples of the use of these types:

.. container:: highlight

   In dowhim.h:

   .. code::

      PR_EXTERN( void ) DoWhatIMean( void );

      static void PR_CALLBACK RootFunction(void *arg);

.. container:: highlight

   In dowhim.c:

   .. code::

      PR_IMPLEMENT( void ) DoWhatIMean( void ) { return; };

      PRThread *thread = PR_CreateThread(..., RootFunction, ...);

.. _Algebraic_Types:

Algebraic Types
---------------

NSPR provides the following type definitions with unambiguous bit widths
for algebraic operations:

-  `8-, 16-, and 32-bit Integer
   Types <#8-,_16-,_and_32-bit_Integer_Types>`__
-  `64-bit Integer Types <#64-bit_Integer_Types>`__
-  `Floating-Point Number Type <#Floating-Point_Number_Type>`__

For convenience, NSPR also provides type definitions with
platform-dependent bit widths:

-  `Native OS Integer Types <#Native_OS_Integer_Types>`__

.. _8-.2C_16-.2C_and_32-bit_Integer_Types:

8-, 16-, and 32-bit Integer Types
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. _Signed_Integers:

Signed Integers
^^^^^^^^^^^^^^^

 - :ref:`PRInt8`
 - :ref:`PRInt16`
 - :ref:`PRInt32`

.. _Unsigned_Integers:

Unsigned Integers
^^^^^^^^^^^^^^^^^

 - :ref:`PRUint8`
 - :ref:`PRUint16`
 - :ref:`PRUint32`

.. _64-bit_Integer_Types:

64-bit Integer Types
~~~~~~~~~~~~~~~~~~~~

Different platforms treat 64-bit numeric fields in different ways. Some
systems require emulation of 64-bit fields by using two 32-bit numeric
fields bound in a structure. Since the types (``long long`` versus
``struct LONGLONG``) are not type compatible, NSPR defines macros to
manipulate 64-bit numeric fields. These macros are defined in
``prlong.h``. Conscientious use of these macros ensures portability of
code to all the platforms supported by NSPR and still provides optimal
behavior on those systems that treat long long values directly.

 - :ref:`PRInt64`
 - :ref:`PRUint64`

.. _Floating-Point_Number_Type:

Floating-Point Number Type
~~~~~~~~~~~~~~~~~~~~~~~~~~

The NSPR floating-point type is always 64 bits.

 - :ref:`PRFloat64`

.. _Native_OS_Integer_Types:

Native OS Integer Types
~~~~~~~~~~~~~~~~~~~~~~~

These types are most appropriate for automatic variables. They are
guaranteed to be at least 16 bits, though various architectures may
define them to be wider (for example, 32 or even 64 bits). These types
are never valid for fields of a structure.

 - :ref:`PRIntn`
 - :ref:`PRUintn`

.. _Miscellaneous_Types:

Miscellaneous Types
-------------------

-  `Size Type <#Size_Type>`__
-  `Pointer Difference Types <#Pointer_Difference_Types>`__
-  `Boolean Types <#Boolean_Types>`__
-  `Status Type for Return Values <#Status_Type_for_Return_Values>`__

.. _Size_Type:

Size Type
~~~~~~~~~

 - :ref:`PRSize`

.. _Pointer_Difference_Types:

Pointer Difference Types
~~~~~~~~~~~~~~~~~~~~~~~~

Types for pointer difference. Variables of these types are suitable for
storing a pointer or pointer subtraction. These are the same as the
corresponding types in ``libc``.

 - :ref:`PRPtrdiff`
 - :ref:`PRUptrdiff`

.. _Boolean_Types:

Boolean Types
~~~~~~~~~~~~~

Type and constants for Boolean values.

 - :ref:`PRBool`
 - :ref:`PRPackedBool`

.. _Status_Type_for_Return_Values:

Status Type for Return Values
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PRStatus`
