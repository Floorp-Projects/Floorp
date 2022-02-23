Process Management And Interprocess Communication
=================================================

This chapter describes the NSPR routines that deal with processes. A
process is an instance of a program. NSPR provides routines to create a
new process and to wait for the termination of another process.

NSPR does not provide an equivalent of the Unix ``fork()``. The
newly-created process executes its program from the beginning. A new
process can inherit specified file descriptors from its parent, and the
parent can redirect the standard I/O streams of the child process to
specified file descriptors.

Note that the functions described in this chapter are not available for
MacOS or Win16 operating systems.

.. _Process_Management_Types_and_Constants:

Process Management Types and Constants
--------------------------------------

The types defined for process management are:

 - :ref:`PRProcess`
 - :ref:`PRProcessAttr`

.. _Process_Management_Functions:

Process Management Functions
----------------------------

The process manipulation function fall into these categories:

-  `Setting the Attributes of a New
   Process <#Setting_the_Attributes_of_a_New_Process>`__
-  `Creating and Managing
   Processes <#Creating_and_Managing_Processes>`__

.. _Setting_the_Attributes_of_a_New_Process:

Setting the Attributes of a New Process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The functions that create and manipulate attribute sets of new processes
are:

 - :ref:`PR_NewProcessAttr`
 - :ref:`PR_ResetProcessAttr`
 - :ref:`PR_DestroyProcessAttr`
 - :ref:`PR_ProcessAttrSetStdioRedirect`
 - :ref:`PR_ProcessAttrSetCurrentDirectory`
 - :ref:`PR_ProcessAttrSetInheritableFD`

.. _Creating_and_Managing_Processes:

Creating and Managing Processes
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The functions that create and manage processes are:

 - :ref:`PR_CreateProcess`
 - :ref:`PR_DetachProcess`
 - :ref:`PR_WaitProcess`
 - :ref:`PR_KillProcess`
