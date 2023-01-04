NSPR Logging
============

This chapter describes the global functions you use to perform logging.
NSPR provides a set of logging functions that conditionally write
``printf()`` style strings to the console or to a log file. NSPR uses
this facility itself for its own development debugging purposes.

You can select events to be logged by module or level. A module is a
user-defined class of log events. A level is a numeric value that
indicates the seriousness of the event to be logged. You can combine
module and level criteria to get highly selective logging.

NSPR also provides "assert"-style macros and functions to aid in
application debugging.

-  `Conditional Compilation and
   Execution <#Conditional_Compilation_and_Execution>`__
-  `Log Types and Variables <#Log_Types_and_Variables>`__
-  `Logging Functions and Macros <#Logging_Functions_and_Macros>`__
-  `Use Example <#Use_Example>`__

.. _Conditional_Compilation_and_Execution:

Conditional Compilation and Execution
-------------------------------------

NSPR's logging facility is conditionally compiled in and enabled for
applications using it. These controls are platform dependent. Logging is
not compiled in for the Win16 platform. Logging is compiled into the
NSPR debug builds; logging is not compiled into the NSPR optimized
builds. The compile time ``#define`` values ``DEBUG`` or
``FORCE_PR_LOG`` enable NSPR logging for application programs.

To enable NSPR logging and/or the debugging aids in your application,
compile using the NSPR debug build headers and runtime. Set one of the
compile-time defines when you build your application.

Execution-time control of NSPR's logging uses two environment variables.
These variables control which modules and levels are logged as well as
the file name of the log file. By default, no logging is enabled at
execution time.

.. _Log_Types_and_Variables:

Log Types and Variables
-----------------------

Two types supporting NSPR logging are exposed in the API:

 - :ref:`PRLogModuleInfo`
 - :ref:`PRLogModuleLevel`

Two environment variables control the behavior of logging at execution
time:

 - :ref:`NSPR_LOG_MODULES`
 - :ref:`NSPR_LOG_FILE`

.. _Logging_Functions_and_Macros:

Logging Functions and Macros
----------------------------

The functions and macros for logging are:

 - :ref:`PR_NewLogModule`
 - :ref:`PR_SetLogFile`
 - :ref:`PR_SetLogBuffering`
 - :ref:`PR_LogPrint`
 - :ref:`PR_LogFlush`
 - :ref:`PR_LOG_TEST`
 - :ref:`PR_LOG`
 - :ref:`PR_Assert`
 - :ref:`PR_STATIC_ASSERT` (new in NSPR 4.6.6XXX this hasn't been released
   yet; the number is a logical guess)
 - :ref:`PR_NOT_REACHED`

.. note::

   The above documentation has not been ported to MDN yet, see
   http://www-archive.mozilla.org/projects/nspr/reference/html/prlog.html#25338.

.. _Use_Example:

Use Example
-----------

The following sample code fragment demonstrates use of the logging and
debugging aids.

-  Compile the program with DEBUG defined.
-  Before running the compiled program, set the environment variable
   NSPR_LOG_MODULES to userStuff:5

.. code::

   static void UserLogStuff( void )
   {
       PRLogModuleInfo *myLM;
       PRIntn i;

       PR_STATIC_ASSERT(5 > 4); /* NSPR 4.6.6 or newer */

       myLM = PR_NewLogModule( "userStuff" );
       PR_ASSERT( myLM );

       PR_LOG( myLM, PR_LOG_NOTICE, ("Log a Notice %d\n", 999 ));
       for (i = 0; i < 10 ; i++ )
       {
           PR_LOG( myLM, PR_LOG_DEBUG, ("Log Debug number: %d\n", i));
           PR_Sleep( 500 );
       }
       PR_LOG( myLM, PR_LOG_NOTICE, "That's all folks\n");

   } /* end UserLogStuff() */
