Debugging A Minidump
====================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

The
`minidump <http://msdn.microsoft.com/en-us/library/windows/desktop/ms680369%28v=vs.85%29.aspx>`__
file format contains data about a crash on Windows. It is used by
`Breakpad <https://wiki.mozilla.org/Breakpad>`__ and also by various
Windows debugging tools. Each minidump includes the following data.

-  Details about the exception which led to the crash.
-  Information about each thread in the process: the address which was
   executing and the register state at the time the process stopped.
-  A list of shared libraries loaded into the process at the time of the
   crash.
-  The stack memory of each thread.
-  The memory right around the crashing address.
-  (Optional) Other memory regions, if requested by the application.
-  (Optional) Other platform-specific data.

Accessing minidumps from crash reports
--------------------------------------

Minidumps are not available to everyone. For details on how to gain
access and where to find minidump files for crash reports, consult the
:ref:`crash report documentation <Understanding Crash Reports>`

Using the MS Visual Studio debugger
-----------------------------------

#. Set up the debugger to `use the Mozilla symbol
   server <https://developer.mozilla.org/en/Using_the_Mozilla_symbol_server>`__ and `source
   server <https://developer.mozilla.org/en-US/docs/Mozilla/Using_the_Mozilla_source_server>`__\.
#. Double-click on the minidump file to open it in the debugger.
#. When it loads, click the green icon in the visual studio debugger
   toolbar that looks like a play button.

For Firefox releases older than Firefox 41, you will also need to
install the relevant release of Firefox (for example from
`here <https://ftp.mozilla.org/pub/mozilla.org/firefox/releases/>`__),
and add the directory it is in (e.g., "C:\Program Files\Mozilla
Firefox 3.6 Beta 1\") to the same dialog in which you set up the
symbol server (in case the binary location in the minidump is not the
same as the one on your machine). Note that you can install the
relevant release anywhere. Just make sure to configure the symbol
server to the directory where you installed it. For releases from 41
onward, the binaries are available on the symbol server.

If this doesn't work, downloading the exact build and crashreporter
symbols full files. These can be found in treeherder / build folder.
Load Visual Studio, and go to file -> open -> minidump location. Click
on "Run Native", and Visual Studio will ask for the corresponding symbol
files. For each .dll you wish to have symbols for, you must go to a
console and go to the corresponding directory. E.g. (xul.dll should go
to xul.pdf in the crashreporter symbols directory). Each directory will
have a .pd\_ file. In a command shell run: "expand /r foo.pd\_". Then
point Visual Studio to this directory.

Then you'll be able to examine:

+------------------+-------------------------------------------------------------------------+
| stack trace      |    The debugger shows the stack trace. You can right-click on any frame |
|                  |    in the stack, and then choose "go to disassembly" or "go to source". |
|                  |    (Choosing "go to disassembly" from the source view might not get you |
|                  |    to the right place due to optimizations.) When looking at the        |
|                  |    source, beware that the debugging information will associate all     |
|                  |    inlined functions as part of the line into which they were inlined,  |
|                  |    and compiler (with PGO) inlines *very* aggressively (including       |
|                  |    inlining virtual functions). You can often figure out where you      |
|                  |    *really* are by reading the disassembly.                             |
+------------------+-------------------------------------------------------------------------+
| registers        |    In the Registers tab (Debug->Windows->Registers if you don't have    |
|                  |    it open), you can look at the registers associated with each stack   |
|                  |    frame, but only at the current state (i.e., the time of the crash).  |
|                  |    Registers that Visual Studio can't figure out will be grayed-out and |
|                  |    have the value 00000000.                                             |
+------------------+-------------------------------------------------------------------------+
| stack memory     |    You open a window (Memory 1, etc.) that shows contiguous segments of |
|                  |    memory using the Debug->Windows->Memory menu item. You can then      |
|                  |    enter the address of the stack pointer (ESP register) in this window |
|                  |    and look at the memory on the stack. (The minidump doesn't have the  |
|                  |    memory on the heap.) It's a good idea to change the "width" dropdown |
|                  |    in the top right corner of the window from its default "Auto" to     |
|                  |    either "8" or "16" so that the memory display is word-aligned. If    |
|                  |    you're interested in pointers, which is usually the case, you can    |
|                  |    right click in this window and change the display to show 4-byte     |
|                  |    words (so that you don't have to reverse the order due to            |
|                  |    little-endianness). This view, combined with the disassembly, can    |
|                  |    often be used to reconstruct information beyond what in shown the    |
|                  |    function parameters.                                                 |
+------------------+-------------------------------------------------------------------------+
| local variables  |    In the Watch 1 (etc.) window (which, if you don't have open, you can |
|                  |    iget from Debug->Windows->Watch), you can type an expression         |
|                  |    (e.g., the name of a local variable) and the debugger will show you  |
|                  |    its value (although it sometimes gets confused). If you're looking   |
|                  |    at a pointer to a variable that happens to be on the stack, you can  |
|                  |    even examine member variables by just typing expressions. If Visual  |
|                  |    Studio can't figure something out from the minidump, it might show   |
|                  |    you 00000000 (is this true?).                                        |
+------------------+-------------------------------------------------------------------------+

Using minidump-2-core on Linux
------------------------------

The `Breakpad
source <https://chromium.googlesource.com/breakpad/breakpad/+/master/>`__
contains a tool called
`minidump-2-core <https://chromium.googlesource.com/breakpad/breakpad/+/master/src/tools/linux/md2core/>`__,
which converts Linux minidumps into core files. If you checkout and
build Breakpad, the binary will be at
``src/tools/linux/md2core/minidump-2-core``. Running the binary with the
path to a Linux minidump will generate a core file on stdout which can
then be loaded in gdb as usual. You will need to manually download the
matching Firefox binaries, but then you can use the `GDB Python
script <https://developer.mozilla.org/en/Using_the_Mozilla_symbol_server#Downloading_symbols_on_Linux_Mac_OS_X>`__
to download symbols.

The ``minidump-2-core`` source does not currently handle processing
minidumps from a different CPU architecture than the system it was
built for. If you want to use it on an ARM dump, for example, you may
need to build the tool for ARM and run it under QEMU.

Using other tools to inspect minidump data
------------------------------------------

Breakpad includes a tool called ``minidump_dump`` built alongside
``minidump_stackwalk`` which will verbosely print the contents of a
minidump. This can sometimes be useful for finding specific information
that is not exposed on crash-stats.

Ted has a few tools that can be built against an already-built copy of
Breakpad to do more targeted inspection. All of these tools assume you
have checked out their source in a directory next to the breakpad
checkout, and that you have built Breakpad in an objdir named
``obj-breakpad`` at the same level.

-  `stackwalk-http <https://hg.mozilla.org/users/tmielczarek_mozilla.com/stackwalk-http/>`__
   is a version of minidump_stackwalk that can fetch symbols over HTTP,
   and also has the Mozilla symbol server URL baked in. If you run it
   like ``stackwalk /path/to/dmp /tmp/syms`` it will print the stack
   trace and save the symbols it downloaded in ``/tmp/syms``. Note that
   symbols are only uploaded to the symbol server for nightly and
   release builds, not per-change builds.
-  `dump-lookup <https://hg.mozilla.org/users/tmielczarek_mozilla.com/dump-lookup/>`__
   takes a minidump and prints values on the stack that are potential
   return addresses. This is useful when a stack trace looks truncated
   or otherwise wrong. It needs symbol files to produce useful output,
   so you will generally want to have run ``stackwalk-http`` to download
   them first.
-  `get-minidump-instructions <https://hg.mozilla.org/users/tmielczarek_mozilla.com/get-minidump-instructions/>`__
   retrieves and displays the memory range surrounding the faulting
   instruction pointer from a minidump. You will almost always want to
   run it with the ``--disassemble`` option, which will make it send the
   bytes through ``objdump`` to display the disassembled instructions.
   If you also give it a path to symbols (see ``stackwalk-http`` above)
   it can download the matching source files from hg.mozilla.org and
   display source interleaved with the disassembly.
-  `minidump-modules <http://hg.mozilla.org/users/tmielczarek_mozilla.com/minidump-modules>`__
   takes a minidump and prints the list of modules from the crashed
   process. It will print the full path to each module, whereas the
   Socorro UI only prints the filename for each module for privacy
   reasons. It also accepts a -v option to print the debug ID for each
   module, and a -d option to print relative paths to the symbol files
   that would be used instead of the module filenames.

Getting a stack trace from a crashed B2G process
------------------------------------------------

#. Get the minidump file in the phone at
   /data/b2g/mozilla/\*.default/minidump/. You can use `adb
   pull <http://developer.android.com/tools/help/adb.html>`__ for that.
#. Build the debug symbols using the command ./build.sh buildsymbols
   inside the B2G tree. The symbol files will be generated in
   $OBJDIR/dist/crashreporter-symbols.
#. Build and install
   `google-breakpad <https://code.google.com/p/google-breakpad/>`__.
#. Use the
   `minidump_stackwalk <https://code.google.com/p/google-breakpad/wiki/LinuxStarterGuide>`__
   breakpad tool to get the stack trace.

.. code:: bash

   Example:

   $ cd B2G
   $ adb pull /data/b2g/mozilla/*.default/minidump/*.dmp .
   $ls *.dmp
   71788789-197e-d769-67167423-4e7aef32.dmp
   $ minidump_stackwalk 71788789-197e-d769-67167423-4e7aef32.dmp objdir-debug/dist/crashreporter-symbols/
