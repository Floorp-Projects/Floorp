Capturing a minidump
====================

*Minidumps* are files created by various Windows tools which record the
complete state of a program as it's running, or as it was at the moment
of a crash. Small minidumps are created by the Breakpad :ref:`crash
reporting <Crash Reporter>` tool, but sometimes that's not
sufficient to diagnose a problem. For example, if the application is
hanging (not responding to input, but hasn't crashed) then Breakpad is
not triggered, and it can be difficult to determine where the problem
lies. Sometimes a more complete form of minidump is needed to see
additional details about a crash, in which case manual capture of a
minidump is desired.

This page describes how to capture these minidumps on Windows, to permit
better debugging.


Privacy and minidumps
---------------------

.. warning::

   **Warning!** Unlike the minidumps submitted by Breakpad, these
   minidumps contain the **complete** contents of program memory. They
   are therefore much more likely to contain private information, if
   there is any in the browser. For this reason, you may prefer to
   generate minidumps against a `clean
   profile <http://support.mozilla.com/en-US/kb/Managing%20profiles>`__
   where possible.


Capturing a minidump: application crash
---------------------------------------

To capture a full minidump for an application crash, you can use a tool
called windbg.


Install debugging tools for Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Microsoft distributes the Debugging Tools for Windows for free, those
include WinDbg which you will need here. Download it from `Install
Debugging Tools for
Windows <http://msdn.microsoft.com/en-us/windows/hardware/gg463009.aspx>`__.
(*You'll want the 32-bit version of WinDbg only if you are using a
32*-bit version of Firefox) Then install it, the standard settings in
the installation process are fine.


Capture a minidump
~~~~~~~~~~~~~~~~~~

#. Connect Firefox to the debugger. 

   a. If Firefox is not already running, then open WinDbg from the Start
      menu (Start->All Programs->Debugging Tools for Windows->WinDbg). 
      Next, open the **"File"** menu and choose **"Open
      Executable..."**. In the file chooser window that appears, open
      the firefox.exe executable in your Firefox program folder
      (C:\Program Files\Mozilla Firefox).

   b. If Firefox is already running, open WinDbg from the Start menu
      (Start->All Programs->Debugging Tools for Windows->WinDbg).  Next,
      open the **"File"** menu and choose **"Attach to a Process..."**.
      In the file chooser window that appears, find the firefox.exe
      executable process with the lowest PID.

#. You should now see a "Command" text window with debug output at the
   top and an input box at the bottom. From the menu, select
   ``Debug → Go``, and Firefox should start. If the debugger spits out
   some text right away and Firefox doesn't come up, select
   ``Debug → Go`` again.

#. When the program is about to crash, WinDbg will spit out more data,
   and the prompt at the bottom will change from saying "``*BUSY*``" to
   having a number in it. At this point, you should type
   "``.dump /ma c:\temp\firefoxcrash.dmp``" -- without the quotes, but
   don't forget the dot at the beginning. Once it completes, which can
   take a fair while, you will have a very large file at
   ``c:\temp\firefoxcrash.dmp`` that can be used to help debug your
   problem.  File size will depend on this size of Firefox running in
   your environment, which could several GB.

#. Ask in the relevant bug or thread how best to share this very large
   file!


Capturing a minidump: application hang
--------------------------------------

On Windows Vista and Windows 7, you can follow `these
instructions <http://support.microsoft.com/kb/931673>`__ to capture a
dump file and locate it after it's been saved.
