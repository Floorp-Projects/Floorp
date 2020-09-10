How to get a stacktrace with WinDbg
===================================

+--------------------------------------------------------------------+
| This page is an import from MDN and the contents might be outdated |
+--------------------------------------------------------------------+

Introduction
------------

Sometimes you need to get a stacktrace (call stack) for a crash or hang
but `Breakpad <http://kb.mozillazine.org/Breakpad>`__ fails because it's
a special crash or a hang. This article describes how to get a
stacktrace in those cases with WinDbg on Windows. (To get a stacktrace
for Thunderbird or some other product, substitute the product name where
ever you see Firefox in this instructions.)

Requirements
------------

To get such a stacktrace you need to install the following software:

Debugging Tools for Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Microsoft distributes the Debugging Tools for Windows for free, those
include WinDbg which you will need here. Download it from `Install
Debugging Tools for
Windows <https://docs.microsoft.com/en-us/windows-hardware/drivers/download-the-wdk>`__.
(*You'll want the 32-bit version*, even if you are using a 64-bit
version of Windows) Then install it, the standard settings in the
installation process are fine.

A Firefox nightly or release
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You need a Firefox version for which symbols are availables from the
:ref:`symbol server <Using The Mozilla Symbol Server>` to use
with WinDbg. You can use any `official nightly
build <https://ftp.mozilla.org/pub/firefox/nightly/>`__ or released
version of Firefox from Mozilla. You can find the latest trunk nightly
builds under
`http://ftp.mozilla.org/pub/mozilla.o.../latest-trunk/ <https://ftp.mozilla.org/pub/firefox/nightly/latest-mozilla-central/>`__.


Debugging
---------

To begin debugging, ensure that Firefox is not already running and open
WinDbg from the Start menu. (Start->All Programs->Debugging Tools for
Windows->WinDbg) Next, open the **"File"** menu and choose **"Open
Executable..."**. In the file chooser window that appears, open the
firefox.exe executable in your Firefox program folder (C:\Program
Files\Mozilla Firefox).

You should now see a "Command" text window with debug output at the top
and an input box at the bottom. Before debugging can start, several
commands must be entered into the one-line input box at the bottom of
the Command window.

.. note::

   Tip: All commands must be entered exactly as written, one line at a
   time, into the bottom of the Command box.

   -  Copying and pasting each line is the easiest method to avoid
      mistakes
   -  Some commands start with a period (.) or a pipe character (|),
      which is required. (The keystroke for a pipe character on US
      keyboards is SHIFT+\)
   -  Submit the log file on a bug or via the support site, even if
      nothing seems to happen during the debug process


Start debugging
~~~~~~~~~~~~~~~

Now that Firefox is opened in the debugger, you need to configure your
WinDbg to download symbols from the Mozilla symbol server. To load the
symbols, enter the three commands below, pressing enter after each one.
(More details are available at :ref:`symbol server <Using The Mozilla Symbol Server>`.)

::

   .sympath SRV*c:\symbols*http://symbols.mozilla.org/firefox;SRV*c:\symbols*http://msdl.microsoft.com/download/symbols
   .symfix+ c:\symbols
   .reload /f

Now wait for the symbols to download. This may take some time depending
on your connection speed; the total size of the Mozilla and Microsoft
symbols download is around 1.4GB. WinDbg will show "Busy" at the bottom
of the application window until the download is complete.

Once the download is complete, you need to configure WinDbg to examine
child processes, ignore a specific event caused by Flash Player, and
record a log of loaded modules. You will also want to open a log file to
save data you collect. To do this, enter these four commands, pressing
enter after each one.

::

   .logopen /t c:\temp\firefox-debug.log
   .childdbg 1
   .tlist
   sxn gp
   lm

If you see firefox.exe listed in the output from .tlist more than once,
then you are already running the application and need to close the
running instance first before you start debugging, otherwise you won't
get useful results.

Now run Firefox by opening the **Debug** menu and clicking **Go**.
**While Firefox is running, you will not be able to type any commands
into the debugger.** After it starts, try to reproduce the crash or
hanging issue that you are seeing.

.. note::

   If Firefox fails to start, and you see lines of text followed by a
   command prompt in the debugger, a "breakpoint" may have been
   triggered. If you are prompted for a command but don't see an error
   about a crash, go back to the **Debug** menu and press **Go**.

Once the browser crashes, you will see an error (such as "Access
violation") in the WinDbg Command window. If Firefox hangs and there is
no command prompt available in the debugger, open the **Debug** menu and
choose **Break.** Once the browser has crashed or been stopped, continue
with the steps below.


After the crash or hang
~~~~~~~~~~~~~~~~~~~~~~~

You need to capture the debug information to include in a bug comment or
support request. Enter these three commands, one at a time, to get the
stacktrace, crash/hang analysis and log of loaded modules. (Again, press
Enter after each command.)

::

   ~* kp
   !analyze -v -f
   lm

After these steps are completed, find the file
**c:\temp\firefox-debug-(Today's Date).txt** on your hard drive. To
provide the information to the development community, submit this file
with a `support request <https://support.mozilla.com/>`__ or attach it
to a related bug on `Bugzilla <https://bugzilla.mozilla.org/>`__.


Producing a minidump
~~~~~~~~~~~~~~~~~~~~

Sometimes the stacktrace alone is not enough information for a developer
to figure out what went wrong. A developer may ask you for a "minidump"
or a "full memory dump", which are files containing more information
about the process. :ref:`You can easily produce minidumps from WinDBG and
provide them to developers <Capturing a minidump>`.

FAQ

Q: I am running Windows 7 (32-bit or 64-bit) and I see an exception in
the WinDbg command window that says 'ntdll32!LdrpDoDebuggerBreak+0x2c'
or 'ntdll32!LdrpDoDebuggerBreak+0x30'. What do I do now?

A: If you see 'int 3' after either of those exceptions, you will need to
execute the following commands in WinDbg.

::

   bp ntdll!LdrpDoDebuggerBreak+0x30
   bp ntdll!LdrpDoDebuggerBreak+0x2c
   eb ntdll!LdrpDoDebuggerBreak+0x30 0x90
   eb ntdll!LdrpDoDebuggerBreak+0x2c 0x90

| Make sure you enter them one at a time and press enter after each one.
  If you use the 64-bit version of Windows, you need to replace "ntdll"
  in these commands with "ntdll32".
| Q: The first four frames of my stack trace look like this:

::

   0012fe20 7c90e89a ntdll!KiFastSystemCallRet
   0012fe24 7c81cd96 ntdll!ZwTerminateProcess+0xc
   0012ff20 7c81cdee kernel32!_ExitProcess+0x62

   0012ff34 6000179e kernel32!ExitProcess+0x14

This looks wrong to me?!

A: You ran the application without the "Debug child processes also"
check box being checked. You need to detach the debugger and open the
application again, this time with the check box being checked.

Q: WinDbg tells me that it is unable to verify checksum for firefox.exe.
Is this normal?

A: Yes, this is normal and can be ignored.

Q: Should I click yes or no when WinDbg asks me to "Save information for
workspace?"

A: Click yes and WinDbg will save you from having to enter in the symbol
location for Firefox.exe in the future. Click no if you'd rather not
having WinDbg save this information.

Q: I'm seeing "wow64" on top of each thread, is that ok ?

A: No, you are running a 64 bit version of Windbg and trying to debug a
32 bit version of the mozilla software. Redownload and install the 32
bit version of windbg.


Troubleshooting: Symbols will not download
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If symbols will not download no matter what you do, the problem may be
that Internet Explorer has been set to the **Work Offline** mode. You
will not receive any warnings of this in Windbg, Visual C++ or Visual
Studio. Even using the command line with symchk.exe to download symbols
will fail. This is because Microsoft uses Internet Explorer's internet &
proxy settings to download the symbol files. Check the File menu of
Internet Explorer to ensure "Work Offline" is unchecked.


See also
--------

-  :ref:`symbol server <Using The Mozilla Symbol Server>` Maps addresses to human readable strings.
-  :ref:`source server <Using The Mozilla Source Server>` Maps addresses to source code lines
