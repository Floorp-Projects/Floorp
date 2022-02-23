Process Initialization
======================

This chapter describes the NSPR API for versioning, process
initialization, and shutdown of NSPR.

-  `Identity and Versioning <#Identity_and_Versioning>`__
-  `Initialization and Cleanup <#Initialization_and_Cleanup>`__
-  `Module Initialization <#Module_Initialization>`__

.. _Identity_and_Versioning:

Identity and Versioning
-----------------------

.. _Name_and_Version_Constants:

Name and Version Constants
~~~~~~~~~~~~~~~~~~~~~~~~~~

 - :ref:`PR_NAME`
 - :ref:`PR_VERSION`
 - :ref:`PR_VersionCheck`

.. _Initialization_and_Cleanup:

Initialization and Cleanup
--------------------------

NSPR detects whether the library has been initialized and performs
implicit initialization if it hasn't. Implicit initialization should
suffice unless a program has specific sequencing requirements or needs
to characterize the primordial thread. Explicit initialization is rarely
necessary.

Implicit initialization assumes that the initiator is the primordial
thread and that the thread is a user thread of normal priority.

 - :ref:`PR_Init`
 - :ref:`PR_Initialize`
 - :ref:`PR_Initialized`
 - :ref:`PR_Cleanup`
 - :ref:`PR_DisableClockInterrupts`
 - :ref:`PR_BlockClockInterrupts`
 - :ref:`PR_UnblockClockInterrupts`
 - :ref:`PR_SetConcurrency`
 - :ref:`PR_ProcessExit`
 - :ref:`PR_Abort`

.. _Module_Initialization:

Module Initialization
~~~~~~~~~~~~~~~~~~~~~

Initialization can be tricky in a threaded environment, especially
initialization that must happen exactly once. :ref:`PR_CallOnce` ensures
that such initialization code is called only once. This facility is
recommended in situations where complicated global initialization is
required.

 - :ref:`PRCallOnceType`
 - :ref:`PRCallOnceFN`
 - :ref:`PR_CallOnce`
