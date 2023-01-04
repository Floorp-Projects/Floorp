Dynamic Library Search
======================

This section describes NSPR's programming interface to load, unload and
resolve symbols in dynamic libraries. It also provides a method by which
to condition symbols of statically linked code so that to other clients
it appears as though they are dynamically loaded.

.. _Library_Linking_Types:

Library Linking Types
---------------------

These data types are defined for dynamic library linking:

 - :ref:`PRLibrary`
 - :ref:`PRStaticLinkTable`

.. _Library_Linking_Functions:

Library Linking Functions
-------------------------

The library linking functions are:

 - :ref:`PR_SetLibraryPath`
 - :ref:`PR_GetLibraryPath`
 - :ref:`PR_GetLibraryName`
 - :ref:`PR_FreeLibraryName`
 - :ref:`PR_LoadLibrary`
 - :ref:`PR_UnloadLibrary`
 - :ref:`PR_FindSymbol`
 - :ref:`PR_FindSymbolAndLibrary`

.. _Finding_Symbols_Defined_in_the_Main_Executable_Program:

Finding Symbols Defined in the Main Executable Program
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

:ref:`PR_LoadLibrary` cannot open a handle that references the main
executable program. (This is admittedly an omission that should be
fixed.) However, it is possible to look up symbols defined in the main
executable program as follows.

.. code::

   PRLibrary *lib;
   void *funcPtr;

   funcPtr = PR_FindSymbolAndLibrary("FunctionName", &lib);

When :ref:`PR_FindSymbolAndLibrary` returns, ``funcPtr`` is the value of
the function pointer you want to look up, and the variable lib
references the main executable program. You can then call
:ref:`PR_FindSymbol` on lib to look up other symbols defined in the main
program. Remember to call ``PR_UnloadLibrary(lib)`` to close the library
handle when you are done.

.. _Platform_Notes:

Platform Notes
--------------

To use the dynamic library loading functions on some platforms, certain
environment variables must be set at run time, and you may need to link
your executable programs using special linker options.

This section summarizes these platform idiosyncrasies. For more
information, consult the man pages for ``ld`` and ``dlopen`` (or
``shl_load`` on HP-UX) for Unix, and the ``LoadLibrary`` documentation
for Win32.

-  `Dynamic Library Search Path <#Dynamic_Library_Search_Path>`__
-  `Exporting Symbols from the Main Executable
   Program <#Exporting_Symbols_from_the_Main_Executable_Program>`__

Dynamic Library Search Path
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The dynamic library search path is the list of directories in which to
look for a dynamic library. Each platform has its own standard
directories in which to look for dynamic libraries, plus a customizable
list of directories specified by an environment variable.

-  On most Unix systems, this environment variable is
   ``LD_LIBRARY_PATH``. These systems typically use ``dlopen`` to load a
   dynamic library.
-  HP-UX uses ``shl_load`` to load dynamic libraries, and the
   environment variable specifying the dynamic library search path is
   ``SHLIB_PATH``. Moreover, the executable program must be linked with
   the +s option so that it will search for shared libraries in the
   directories specified by ``SHLIB_PATH`` at run time. Alternatively,
   you can enable the +s option as a postprocessing step using the
   ``chatr`` tool. For example, link your executable program a.out
   without the +s option, then execute the following:

.. code::

   chatr +s enable a.out

-  On Rhapsody, the environment variable is ``DYLD_LIBRARY_PATH``.
-  On Win32, the environment variable is ``PATH``. The same search path
   is used to search for executable programs and DLLs.

.. _Exporting_Symbols_from_the_Main_Executable_Program:

Exporting Symbols from the Main Executable Program
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On some systems, symbols defined in the main executable program are not
exported by default. On HP-UX, you must link the executable program with
the -E linker option in order to export all symbols in the main program
to shared libraries. If you use the GNU compilers (on any platform), you
must also link the executable program with the -E option.
