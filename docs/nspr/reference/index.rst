NSPR API Reference
==================

.. _Introduction_to_NSPR:

:ref:`Introduction to NSPR`
--------------------------------------------------------------------------------

-  :ref:`NSPR_Naming_Conventions`
-  :ref:`NSPR_Threads`

   -  :ref:`Thread_Scheduling`

      -  :ref:`Setting_Thread_Priorities`
      -  :ref:`Preempting_Threads`
      -  :ref:`Interrupting_Threads`

-  :ref:`NSPR_Thread_Synchronization`

   -  :ref:`Locks_and_Monitors`
   -  :ref:`Condition_Variables`

-  :ref:`NSPR_Sample_Code`

:ref:`NSPR_Types`
-----------------

-  :ref:`Calling_Convention_Types`
-  :ref:`Algebraic_Types`

   -  :ref:`8-.2C_16-.2C_and_32-bit_Integer_Types`

      - :ref:`Signed_Integers`
      - :ref:`Unsigned_Integers`

   -  :ref:`64-bit_Integer_Types`
   -  :ref:`Floating-Point_Number_Type`
   -  :ref:`Native_OS_Integer_Types`

-  :ref:`Miscellaneous_Types`

   -  :ref:`Size_Type`
   -  :ref:`Pointer_Difference_Types`
   -  :ref:`Boolean_Types`
   -  :ref:`Status_Type_for_Return_Values`

.. _Threads:

:ref:`Threads`
--------------

-  :ref:`Threading_Types_and_Constants`
-  :ref:`Threading_Functions`

   -  :ref:`Creating.2C_Joining.2C_and_Identifying_Threads`
   -  :ref:`Controlling_Thread_Priorities`
   -  :ref:`Controlling_Per-Thread_Private_Data`
   -  :ref:`Interrupting_and_Yielding`
   -  :ref:`Setting_Global_Thread_Concurrency`
   -  :ref:`Getting_a_Thread.27s_Scope`

.. _Process_Initialization:

:ref:`Process_Initialization`
-----------------------------

-  :ref:`Identity_and_Versioning`

   -  :ref:`Name_and_Version_Constants`

-  :ref:`Initialization_and_Cleanup`
-  :ref:`Module_Initialization`

.. _Locks:

:ref:`Locks`
------------

-  :ref:`Lock_Type`
-  :ref:`Lock_Functions`

.. _Condition_Variables:

:ref:`Condition_Variables`
--------------------------

-  :ref:`Condition_Variable_Type`
-  :ref:`Condition_Variable_Functions`

.. _Monitors:

:ref:`Monitors`
---------------

-  :ref:`Monitor_Type`
-  :ref:`Monitor_Functions`

.. _Cached_Monitors:

:ref:`Cached_Monitors`
----------------------

-  :ref:`Cached_Monitors_Functions`

.. _I.2FO_Types:

:ref:`I_O_Types`
----------------

-  :ref:`Directory_Type`
-  :ref:`File_Descriptor_Types`
-  :ref:`File_Info_Types`
-  :ref:`Network_Address_Types`
-  :ref:`Types_Used_with_Socket_Options_Functions`
-  :ref:`Type_Used_with_Memory-Mapped_I.2FO`
-  :ref:`Offset_Interpretation_for_Seek_Functions`

.. _I.2FO_Functions:

:ref:`I_O_Functions`
--------------------

-  :ref:`Functions_that_Operate_on_Pathnames`
-  :ref:`Functions_that_Act_on_File_Descriptors`
-  :ref:`Directory_I.2FO_Functions`
-  :ref:`Socket_Manipulation_Functions`
-  :ref:`Converting_Between_Host_and_Network_Addresses`
-  :ref:`Memory-Mapped_I.2FO_Functions`
-  :ref:`Anonymous_Pipe_Function`
-  :ref:`Polling_Functions`
-  :ref:`Pollable_Events`
-  :ref:`Manipulating_Layers`

.. _Network_Addresses:

:ref:`Network_Addresses`
------------------------

-  :ref:`Network_Address_Types_and_Constants`
-  :ref:`Network_Address_Functions`

.. _Atomic_Operations:

:ref:`Atomic_Operations`
------------------------

-  :ref:`PR_AtomicIncrement`
-  :ref:`PR_AtomicDecrement`
-  :ref:`PR_AtomicSet`

.. _Interval_Timing:

:ref:`Interval_Timing`
----------------------

-  :ref:`Interval_Time_Type_and_Constants`
-  :ref:`Interval_Functions`

.. _Date_and_Time:

:ref:`Date_and_Time`
--------------------

-  :ref:`Types_and_Constants`
-  :ref:`Time_Parameter_Callback_Functions`
-  :ref:`Functions`

.. _Memory_Management_Operations:

:ref:`Memory_Management_Operations`
-----------------------------------

-  :ref:`Memory_Allocation_Functions`
-  :ref:`Memory_Allocation_Macros`

.. _String_Operations:

:ref:`String_Operations`
------------------------

-  :ref:`PL_strlen`
-  :ref:`PL_strcpy`
-  :ref:`PL_strdup`
-  :ref:`PL_strfree`

.. _Floating_Point_Number_to_String_Conversion:

:ref:`Floating_Point_Number_to_String_Conversion`
-------------------------------------------------

-  :ref:`PR_strtod`
-  :ref:`PR_dtoa`
-  :ref:`PR_cnvtf`

.. _Linked_Lists:

:ref:`Linked_Lists`
-------------------

-  :ref:`Linked_List_Types`

   -  :ref:`PRCList`

-  :ref:`Linked_List_Macros`

   -  :ref:`PR_INIT_CLIST`
   -  :ref:`PR_INIT_STATIC_CLIST`
   -  :ref:`PR_APPEND_LINK`
   -  :ref:`PR_INSERT_LINK`
   -  :ref:`PR_NEXT_LINK`
   -  :ref:`PR_PREV_LINK`
   -  :ref:`PR_REMOVE_LINK`
   -  :ref:`PR_REMOVE_AND_INIT_LINK`
   -  :ref:`PR_INSERT_BEFORE`
   -  :ref:`PR_INSERT_AFTER`

.. _Dynamic_Library_Linking:

:ref:`Dynamic_Library_Linking`
------------------------------

-  :ref:`Library_Linking_Types`

   -  :ref:`PRLibrary`
   -  :ref:`PRStaticLinkTable`

-  :ref:`Library_Linking_Functions`

   -  :ref:`PR_SetLibraryPath`
   -  :ref:`PR_GetLibraryPath`
   -  :ref:`PR_GetLibraryName`
   -  :ref:`PR_FreeLibraryName`
   -  :ref:`PR_LoadLibrary`
   -  :ref:`PR_UnloadLibrary`
   -  :ref:`PR_FindSymbol`
   -  :ref:`PR_FindSymbolAndLibrary`
   -  :ref:`Finding_Symbols_Defined_in_the_Main_Executable_Program`

-  :ref:`Platform_Notes`

   -  :ref:`Dynamic_Library_Search_Path`
   -  :ref:`Exporting_Symbols_from_the_Main_Executable_Program`

.. _Process_Management_and_Interprocess_Communication:

:ref:`Process Management and Interprocess Communication`
--------------------------------------------------------

-  :ref:`Process_Management_Types_and_Constants`

   -  :ref:`PRProcess`
   -  :ref:`PRProcessAttr`

-  :ref:`Process_Management_Functions`

   -  :ref:`Setting_the_Attributes_of_a_New_Process`
   -  :ref:`Creating_and_Managing_Processes`

.. _Logging:

:ref:`Logging`
-----------------------------------------

-  :ref:`Conditional_Compilation_and_Execution`
-  :ref:`Log_Types_and_Variables`

   -  :ref:`PRLogModuleInfo`
   -  :ref:`PRLogModuleLevel`
   -  :ref:`NSPR_LOG_MODULES`
   -  :ref:`NSPR_LOG_FILE`

-  :ref:`Logging_Functions_and_Macros`

   -  :ref:`PR_NewLogModule`
   -  :ref:`PR_SetLogFile`
   -  :ref:`PR_SetLogBuffering`
   -  :ref:`PR_LogPrint`
   -  :ref:`PR_LogFlush`
   -  :ref:`PR_LOG_TEST`
   -  :ref:`PR_LOG`
   -  :ref:`PR_Assert`
   -  :ref:`PR_ASSERT`
   -  :ref:`PR_NOT_REACHED`

-  :ref:`Use_Example`

.. _Named_Shared_Memory:

:ref:`Named_Shared_Memory`
--------------------------

-  :ref:`Shared_Memory_Protocol`
-  :ref:`Named_Shared_Memory_Functions`

.. _Anonymous_Shared_Memory:

:ref:`Anonymous_Shared_Memory`
------------------------------

-  :ref:`Anonymous_Memory_Protocol`
-  :ref:`Anonymous_Shared_Memory_Functions`

.. _IPC_Semaphores:

:ref:`IPC_Semaphores`
---------------------

-  :ref:`IPC_Semaphore_Functions`

.. _Thread_Pools:

:ref:`Thread_Pools`
-------------------

-  :ref:`Thread_Pool_Types`
-  :ref:`Thread_Pool_Functions`

.. _Random_Number_Generator:

:ref:`Random_Number_Generator`
------------------------------

-  :ref:`Random_Number_Generator_Function`

.. _Hash_Tables:

:ref:`Hash_Tables`
------------------

-  :ref:`Hash_Table_Types_and_Constants`
-  :ref:`Hash_Table_Functions`

.. _NSPR_Error_Handling:

:ref:`NSPR_Error_Handling`
--------------------------

-  :ref:`Error_Type`
-  :ref:`Error_Functions`
-  :ref:`Error_Codes`
