Debugging On Windows
====================

This document explains how to debug Gecko based applications such as
Firefox, Thunderbird, and SeaMonkey on Windows using the Visual Studio IDE.

If VS and your Gecko application hang shortly after you launch the
application under the debugger, see `Problems Loading Debug
Symbols <#problems-loading-debug-symbols>`__.

Ways to start the debugger
~~~~~~~~~~~~~~~~~~~~~~~~~~

First of all, it's necessary to install a Visual Studio extension to be
able to follow child processes as they are created. Firefox, in general,
and even in non-e10s mode, does not start the main process directly, it
starts it via a Launcher Process. This means that Visual Studio will
only attach to the first process it finds, and will not hit any
break-point (and even notifies you that it cannot find their location).
`Microsoft Child Process Debugging Power
Tool <https://marketplace.visualstudio.com/items?itemName=vsdbgplat.MicrosoftChildProcessDebuggingPowerTool>`__
allows automatically attaching to child processes, such as Web Content
process, GPU process, etc. Enable it by going its configuration menu in
"Debug > Other debugging targets > Child process debugging settings",
and ticking the box.

If you have followed the steps in :ref:`Building Firefox for
Windows <Building Firefox On Windows>`
and have a local debug build, you can **execute this command from same command line.**

.. code::

   ./mach run --debug

It would open Visual Studio with Firefox's
run options configured. You can **click "Start" button** to run Firefox
then, already attached in the debugger.

Alternatively, if you have generated the Visual Studio solution, via
``./mach build-backend -b VisualStudio``, opening this solution allows
you to run ``firefox.exe`` directly in the debugger. To make it the
startup project, right click on the project and select ``Set As Startup
Project``. It appears bold when it's the case. Breakpoints are kept
across runs, this can be a good way to debug startup issues.

**Run the program until you hit an assertion.** You will get a dialog
box asking if you would like to debug. Hit "Cancel". The MSDEV IDE will
launch and load the file where the assertion happened. This will also
create a Visual Studio Mozilla project in the directory of the executable
by default.

**Attach the debugger to an existing Mozilla process**.  In the Visual
Studio, select Debug > Attach to Process. If you want to debug a content
process, you can **hover on the tab** of page you want to debug, which
would show the pid. You can then select the process from dialog opened
from "Attach to Process". You can open ``about:processes`` to see the pid
for all subprocesses, including tabs but also GPU, networking etc.
For more information, see `Attach to Running Processes with the Visual Studio
Debugger <http://msdn.microsoft.com/en-us/library/vstudio/3s68z0b3.aspx>`__.

**Starting an MSIX installed Firefox with the debugger**. In Visual
Studio, select Debug -> Other Debug Targets -> Debug Installed App Package.
In the dialog, select the installed Firefox package you wish to debug
and click "Start".

Debugging Release and Nightly Builds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Refer to the steps to :ref:`use the Mozilla symbol
server <Using The Mozilla Symbol Server>` and :ref:`source
server <Using The Mozilla Source Server>`

Creating a Visual Studio project for Firefox
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Please refer to :ref:`this <Visual Studio Projects>`.

Changing/setting the executable to debug
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To change or set the executable to debug, go to Project > Properties >
Debugging > Command. (As of Visual Studio 2022.)

It should show the executable you are debugging. If it is empty or
incorrect, manually add the correct path to the executable.

Command line parameters and environment variables
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

To change or set the command line options, go to Project > Properties >
Debugging > Command Arguments.

Some common options would be the URL of the file you want the browser to
open as soon as it starts, starting the Profile Manager, or selecting a
profile. You can also redirect the console output to a file (by adding
"``> filename.txt``" for example, without the quotes).

Customizing the debugger's variable value view
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can customize how Visual Studio displays classes in the variable view.
By default VS displays "{...}" and you need to click the small + icon
to expand the members. You can change this behaviour, and make Visual
Studio display whatever data member you want in whatever order, formatted
however you like instead of just "{...}".

You need to locate a file called "gecko.natvis" under toolkit/library.
The file contains a list of types and how they should be displayed in
the debugger. It is XML and after a little practice you should be well
on your way.

To understand the file in detail refer to `Create custom views of C++
objects in the debugger using the Natvis framework
<https://docs.microsoft.com/en-us/visualstudio/debugger/create-custom-views-of-native-objects>`__

The file already comes with a number of entries that will make your life
easier, like support for several string types. If you need to add a custom
type, or want to change an existing entry for debugging purposes, you can
easily edit the file. For your convenience it is included in all generated
Visual Studio projects, and if you edit and save it within Visual Studio, it
will pick up the changes immediately.

Handling multiple processes in Visual Studio
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
Turn off "Break all processes when one process breaks" to single step a single
process.

Turning off "Break all processes when one process breaks" adds "Step Into
Current Process", "Step Over Current Process" and "Step Out Current Process" to
the "Debug" menu.

To single step a single process with the other processes paused:

- Turn on "Break all processes when one process breaks"
- Hit a breakpoint which stops all processes
- Turn off "Break all processes when one process breaks"
- Now using "Step Into Current Process" will leave the other processes stopped
  and just advance the current one.

Obtaining ``stdout`` and other ``FILE`` handles
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Running the following command in the Command Window in Visual Studio
returns the value of ``stdout``, which can be used with various
debugging methods (such as ``nsGenericElement::List``) that take a
``FILE*`` param:

.. code::

   Debug.EvaluateStatement {,,msvcr80d}(&__iob_func()[1])

(Alternatively you can evaluate ``{,,msvcr80d}(&__iob_func()[1])`` in
the Immediate window)

Similarly, you can open a file on the disk using ``fopen``:

.. code::

   >Debug.EvaluateStatement {,,msvcr80d}fopen("c:\\123", "w")
   0x10311dc0 { ..snip.. }
   >Debug.EvaluateStatement ((nsGenericElement*)0x03f0e710)->List((FILE*)0x10311dc0, 1)
   <void>
   >Debug.EvaluateStatement {,,msvcr80d}fclose((FILE*)0x10311dc0)
   0x00000000

Note that you may not see the debugging output until you flush or close
the file handle.

Disabling ASSERTIONS
~~~~~~~~~~~~~~~~~~~~

There are basically two ways to disable assertions. One requires setting
an environment variable, while the other affects only the currently
running program instance in memory.

Environment variable
^^^^^^^^^^^^^^^^^^^^

There is an environment variable that can disable breaking for
assertions. This is how you would normally set it:

.. code::

   set XPCOM_DEBUG_BREAK=warn

The environment variable takes also other values besides ``warn``, see
``XPCOM_DEBUG_BREAK`` for more details.

Note that unlike Unix, the default for Windows is not warn, it's to pop
up a dialog. To set the environment variable for Visual Studio, use
Project > Properties > Debugging > Environment and click the little box.
Then use

.. code::

   XPCOM_DEBUG_BREAK=warn

Changing running code
^^^^^^^^^^^^^^^^^^^^^

You normally shouldn't need to do this (just quit the application, set
the environment variable described above, and run it again). And this
can be **dangerous** (like **trashing your hard disc and corrupting your
system**). So unless you feel comfortable with this, don't do it. **You
have been warned!**

It is possible to change the interrupt code in memory (which causes you
to break into debugger) to be a NOP (no operation).

You do this by running the program in the debugger until you hit an
assertion. You should see some assembly code. One assembly code
instruction reads "int 3". Check the memory address for that line. Now
open memory view. Type/copy/drag the memory address of "int 3" into the
memory view to get it to update on that part of the memory. Change the
value of the memory to "90", close the memory view and hit "F5" to
continue.

Automatically handling ASSERTIONS without a debugger attached
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When an assertion happens and there is not a debugger attached, a small
helper application
(```windbgdlg.exe`` </En/Automatically_Handle_Failed_Asserts_in_Debug_Builds>`__)
is run. That application can automatically select a response to the "Do
you want to debug" dialog instead of prompting if you configure it, for
more info, see
```windbgdlg.exe`` </En/Automatically_Handle_Failed_Asserts_in_Debug_Builds>`__.

Debugging optimized builds
~~~~~~~~~~~~~~~~~~~~~~~~~~

To effectively debug optimized builds, you should enable debugging
information which effectively leaves the debug symbols in optimized code
so you can still set breakpoints etc. Because the code is optimized,
stepping through the code may occasionally provide small surprises when
the debugger jumps over something.

You need to make sure this configure parameter is set:

.. code::

   ac_add_options --enable-debug

You can also choose to include or exclude specific modules.

Console debugging
~~~~~~~~~~~~~~~~~

When printing to STDOUT from a content process, the console message will
not appear on Windows. One way to view it is simply to disable e10s
(``./mach run --disable-e10s``) but in order to debug with e10s enabled
one can run

::

   ./mach run ... 2>&1 | tee

It may also be necessary to disable the content sandbox
(``MOZ_DISABLE_CONTENT_SANDBOX=1 ./mach run ...``).

Running two instances of Mozilla simultaneously
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can run two instances of Mozilla (e.g. debug and optimized)
simultaneously by setting the environment variable ``MOZ_NO_REMOTE``:

.. code::

   set MOZ_NO_REMOTE=1

Or, starting with Firefox 2 and other Gecko 1.8.1-based applications,
you can use the ``-no-remote`` command-line switch instead (implemented
in
`bug 325509 <https://bugzilla.mozilla.org/show_bug.cgi?id=325509>`__).

You can also specify the profile to use with the ``-P profile_name``
command-line argument.

Debugging JavaScript
~~~~~~~~~~~~~~~~~~~~

You can use helper functions from
`nsXPConnect.cpp <https://searchfox.org/mozilla-central/source/js/xpconnect/src/nsXPConnect.cpp>`__
to inspect and modify the state of JavaScript code from the MSVS
debugger.

For example, to print current JavaScript stack to stdout, evaluate this
in Immediate window:

.. code::

   {,,xul}DumpJSStack()

Visual Studio will show you something in the quick watch window, but
not the stack, you have to look in the OS console for the output.

Also this magical command only works when you have JS on the VS stack.

Debugging minidumps
~~~~~~~~~~~~~~~~~~~

See :ref:`debugging a minidump <Debugging A Minidump>`.

Problems post-mortem debugging on Windows 7 SP1 x64?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you attempt to use ``NS_DebugBreak`` etc to perform post-mortem
debugging on a 64bit Windows 7, but as soon as you try and continue
debugging the program crashes with an Access Violation, you may be
hitting a Windows bug relating to AVX support.  For more details,
including a work-around see `this blog
post <http://www.os2museum.com/wp/?p=960>`__ or `this social.msdn
thread <http://social.msdn.microsoft.com/Forums/vstudio/en-US/392ca62c-e502-42d9-adbc-b4e22d5da0c3/jit-debugging-32bit-app-crashing-with-access-violation>`__.
(And just in-case those links die, the work-around is to execute

::

   bcdedit /set xsavedisable 1

from an elevated command-prompt to disable AVX support.)

Got a tip?
~~~~~~~~~~

If you think you know a cool Mozilla debugging trick, feel free to
discuss it with `#developers <https://chat.mozilla.org/#/room/#developers:mozilla.org>`__ and
then post it here.

.. |Screenshot of disabling assertions| image:: https://developer.mozilla.org/@api/deki/files/420/=Win32-debug-nop.png
   :class: internal
