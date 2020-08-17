Record and Replay Debugging Firefox
===================================

This guide details how to setup `VMWare Workstation
7 <http://www.vmware.com/products/workstation/>`__ to use Record and
Replay Debugging on Firefox on Windows. It's possible to replay debug
with gdb on Linux --- but only with extraordinary pain, and only on
hosts with gdb >= 7.0 (see
http://www.vmware.com/pdf/ws7_replay_linux_technote.pdf and
http://stackframe.blogspot.com/ for more details).

.. warning::

   This feature `was removed from VMWare Workstation
   8 <http://www.replaydebugging.com/2011/09/goodbye-replay-debugging.html>`__
   (released 2011/09/14). For similar functionality check out the `rr
   project <http://rr-project.org/>`__ (see also `Recording
   Firefox <https://github.com/mozilla/rr/wiki/Recording-Firefox>`__).

| With replay-debugging you can exactly record the behavior of a program
  and replay it in a debugger. During replay you can step through code,
  just as if you were debugging natively. This is invaluable when
  debugging non-deterministic bugs, like random test failures, as if you
  catch a random failure in a recording, you can reproduce it
  deterministically in a debugger, and debug forwards and backwards in
  the recording.
| This guide outlines the steps required to setup replay debugging to
  debug random mochitest failures, but you can use it to setup
  replay-debugging on Firefox in general as well.

See also the official VMWare documentation
http://www.vmware.com/pdf/ws7_visualstudio_debug.pdf.

Hardware Requirements
~~~~~~~~~~~~~~~~~~~~~

You need a modern multi-core CPU with VT-x support to get adequate
performance when replay debugging. We recommend a quad core i7 or Xeon
chip. We also recommend at least 8 GB RAM and an SSD with at least 256
GB space.

Setting up the Host computer
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The computer in which you run VMWare Workstation is known as the Host
computer. The virtual machine in which you'll be recording playback is
known as the Guest.

Install Visual Studio Professional on the Host. We've had success with
VS2005 and VS2010. This must be installed before VMWare Workstation. You
cannot use Visual Studio Express.

**Note:** Theoretically, all this works on VS2008 as well, but many
VS2008 users get an error message "VMware: A valid executable name has
not been specified in Debugger settings. You can change this in Project
> Properties > Debugging." when they try to actually do replay
debugging. The only known solution at this time is to use VS2005 or
VS2010 instead.

Follow the steps in `Windows Build
Prerequisites </En/Developer_Guide/Build_Instructions/Windows_Prerequisites>`__
to get a working build setup on the Host.

Install VMWare Workstation 7 for Windows 32-bit and 64-bit, Main
Installation file with Tools. We've had success with version 7.1.3. This
must be installed after Visual Studio so that Workstation's Virtual
Integrated Debugger plugin gets installed into Visual Studio.

Setting up the Guest Virtual Machine
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a Windows virtual machine in VMWare Workstation. This is known as
the "Guest". Make your account username and password the same in the
Guest as your account on the host. Give the virtual machine 512MB of
memory and exactly one virtual CPU. (Record-and-replay does not work
with multiple virtual CPUs due to fundamental limitations of the
technology that can't really be fixed without elaborate hardware
support. You can change the amount of memory in the VM, but giving it
too much or too little will hurt performance during recording,
snapshotting and/or debugging.) Once you've installed the Guest OS, shut
down the Guest, and open the Virtual Machine Settings dialog. In the
Hardware tab, change Processor > Virtualization Engine > Preferred Mode
to Automatic with Replay. In the Options tab set Replay > Snapshot
Frequency to 5 min. This helps to improve performance when replaying in
reverse.

In the Host, create a directory somewhere. This will be your GuestDLLs
directory. All DLLs used by the debug-target in the Guest must be in
your GuestDLLs directory on the Host. Copy the \*.DLL and \*.DRV files
from the Guest's C:\Windows\System32 directory to your GuestDLLs
directory on the Host. You probably also need the Host's MSVC Runtime
DLLs in the GuestDLLs folder. These are in C:\Program Files
(x86)\Microsoft Visual Studio
8\VC\redist\Debug_NonRedist\x86\Microsoft.VC80.DebugCRT\\

Install MozillaBuild in the Guest.

Install Visual Studio in the Guest. This isn't strictly necessary, but
the MozillaBuild shell start batch files won't startup unless you've got
Visual Studio installed (though you should be able to use
start-shell-l10n.bat instead -- could someone confirm this?). If you
want to do remote debugging in the Guest, you need to either have Visual
Studio Pro installed in the Guest, so that the remote debug monitor is
installed, or copy over the remote debug monitor from the host (find it
in C:\Program Files\Microsoft Visual Studio 9.0\Common7\IDE on both
Windows x86 and x64). You can also use Visual Studio's debugger in the
Guest to determine what DLLs are loaded by the debug-target, so that you
know what DLLs you need to copy over to the Host.

In the Guest, install `Debugging Tools for
Windows <http://www.microsoft.com/whdc/DevTools/Debugging/default.mspx>`__.
Then run C:\Program Files\Debugging Tools for Windows (x86)\gflags.exe.
From the "System Registry" tab, ensure that "Disable paging of kernel
mode stacks" is turned on (checked). Press OK to close gflags.exe. You
must reboot the Guest for this to take effect. This makes the call
stacks reported by the debugger more accurate.

You may want to turn off screen saver and monitor power off in the
Guest, to help preserve your sanity.

Turn off "Take snapshots in background" in Workstation > Edit >
Preferences > Priority. This makes snapshots faster, but blocks the VM
while taking them.

Record and Replay of Nightly Builds
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Because nightly builds have debug information available from the Mozilla
`symbol server </en/Using_the_Mozilla_symbol_server>`__, it is
relatively easy to use them for record and replay debugging.

-  Create a shared directory which is available on both the host and
   guest. Install Firefox to this location.
-  If debugging a test failure, also download and unpack the test
   package created with the nightly build.
-  In the guest, create a recording of the issue.
-  In MSVC on the host, Configure Project > Properties > Debugging and
   enter the Command as the path to firefox.exe on the host.
-  Replay! When prompted, you may have to inform MSVC of the location of
   guestdlls and installdir/components.

Building Firefox for Record Replay
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Checkout a mozilla-central repository to your Host. Create a MOZCONFIG,
specifying an object dir, and the following options:

| ``mk_add_options MOZ_OBJDIR=/path/to/your/objdir/  ac_add_options --enable-libxul  ac_add_options --enable-debug --disable-optimize  ac_add_options --enable-tests``
| You must use ``--enable-libxul``. This reduces the number of DLLs
  Firefox builds. Visual Studio has a limited number of breakpoints, and
  must reserve some of them for itself. The more DLLs the debug-target
  has, the more breakpoints MSVC seems to reserve, and so the fewer
  breakpoints you'll have for your own use. (We have seen MSVC reserve
  all the breakpoints so we couldn't set any of our own.)

| Build your mozilla tree on the Host, and package it up along with
  mochitests:
| ``$ cd /path/to/your/mozilla-central  $ MOZCONFIG=$PATH_TO_YOUR_CONFIG make -f client.mk build  $ MOZCONFIG=$PATH_TO_YOUR_CONFIG make -C /path/to/your/objdir/ package  $ MOZCONFIG=$PATH_TO_YOUR_CONFIG make -C /path/to/your/objdir/ package-tests``
| Copy ``$objdir/dist/firefox-3.7a1pre.en-US.win32.tests.zip`` and
  ``$objdir/dist/firefox-3.7a1pre.en-US.win32.zip`` to the Guest.
| You probably need to copy the MSVC C Runtime DLLs over to the Guest
  (unless you have the *exact* same version of MSVC on both the Host and
  the Guest). If you get errors from the Msys shell saying something
  like "bad file number", or "the application isn't configured
  correctly", you need the MSVC runtime DLLs in the Guest.
| ``$ zip -9 -j vc_redist.zip "$VCINSTALLDIR"/redist/Debug_NonRedist/x86/Microsoft.VC80.DebugCRT/*``
| Copy ``vc_redist.zip`` to the Guest.
| In the Guest, extract the Firefox build:
| ``$ mkdir recordable  $ cd recordable  $ unzip ../firefox-3.7a1pre.en-US.win32.zip  $ unzip ../firefox-3.7a1pre.en-US.win32.tests.zip``
| ``$ cp -r bin/* firefox/``
| ``$ cd firefox  $ unzip ../../vc_redist.zip  $ cd ..``
| You can run now mochitests with:
| ``$ python mochitest/runtests.py --appname firefox/firefox.exe --utility-path=firefox --certificate-path=certs --autorun --close-when-done --console-level=INFO --log-file=./mochitest-plain.log --file-level=INFO``

Configuring Visual Studio for Replay Debugging
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Create a new project in Visual Studio. You can't just create a project
by opening an EXE file, the VMware menu is greyed out if you do this.
You must create a new project file using the File > New > Project >
Win32 > Win32 Console Application. You can opt to create an empty
project, and that works fine for our purposes.

Configure Project > Properties > Debugging and enter the Command as the
path to firefox.exe on the Host.

Open VMWare > Options > Replay Debugging in VM > General. Set the Guest
Command to be the path to the firefox.exe in the guest. This can be
different from the path in the host.

Set the "Host Executable Search Path" to be the path to your GuestDLLs
directory. Also add ``$objdir/firefox/components``. The list is
semicolon delimited, e.g. mine is
``C:\cpearce\vm\WinXPSP3\guestdlls;C:\cpearce\objdirs\red\dist\firefox\components``.

Set VMWare > Options > Replay Debugging in VM > Advanced > Process
Instance to Debug to ``n=2``. This is because Firefox starts up twice
during a mochitest run, and you only want to debug the second instance.

Setup symbols. Tools > Options > Debugging > Symbols, add
http://msdl.microsoft.com/download/symbols as a symbol location. If you
run Firefox from MSVC in the Host, and the symbols will be downloaded
immediately.

Creating a Recording
~~~~~~~~~~~~~~~~~~~~

Once you've got a build setup in the Guest and mochitests running, you
want to try reproducing and recording failures. It's a good idea to take
a snapshot before starting a recording, so that you have a known state
to which you can return to after replaying the recording.

We can't initiate a recording from Visual Studio, as the Firefox
executable must be wrapped by the mochitest ``runtests.py`` script. So
instead you must start the recording from Workstation, and then start
the test run with runtests.py (as above). Once you've recorded a test
failure, you can shut down Firefox and stop the recording.

You'll want to enable a lot of logging in the modules you're debugging,
redirect it to a log file, and copy the log file out to your Host after
you've recorded a test failure. If you don't copy the log file out to
the Host, you can't view the log file while replaying.

Replaying a Recording
~~~~~~~~~~~~~~~~~~~~~

You probably want to take a snapshot before you start replay debugging,
so that you can return your system to the same state after you've
replayed.

Open VMWare > Options > Replay Debugging in VM, and set "Virtual
Machine" to point to your Guest's VMX file. Select the recording to
replay. Ensure "Local or Remote" is Local.

To replay debug a mochitest run, Select VMWare > Start Replay Debugging
in VM. This will suspend your existing VM (if it's running) and replay
the recording. You should be able to open up a Firefox source file in
Visual Studio, and set and hit break points.

If Visual Studio prompts you with errors when you start to replay debug
saying that it can't find a DLL, start Firefox in the Guest, and attach
Visual Studio, and check the "Modules" debug pane. This will tell you
the path to all the DLLs that the process has loaded. Make sure you've
got a copy of every DLL loaded in the guest in your GuestDLLs directory
on the host. There's probably a DLL in the Guest's C:\Windows\SxS
directory that you need in the Host's GuestDLL folder.

Workflow
~~~~~~~~

We're still working out a good workflow and what tools we require to
make replay debugging the most effective.

Our current approach is to edit dom/base/nsGlobalWindow::Dump() so that
it increments and prints a counter every time it's called. This means
whenever Javascript calls dump() to log a message (in particular a test
pass/fail message) we increment and print a counter as part of that
message. You can then review the console log and set a conditional
breakpoint in the nsGlobalWindow::dump() to break based on the value of
the counter variable. You can use this to set a breakpoint on the
message which comes before the first test failure. Once you hit that
breakpoint, you then set other breakpoints in relevant code paths, and
debug forwards (and backwards!) from there.

| We also have a patch to make mochitest to loop forever on a directory,
  which still needs cleaning up but hopefully will land shortly...
| When you hit a breakpoint, you can save a snapshot of the VM. You can
  then resume playback from that snapshot (rather than from the
  beginning of the recording) using the VMWare > Attach to process in
  Recording. This means you won't have to sit through the playback of
  your entire recording before getting to the interesting bits. Creating
  snapshots is usually very fast (a few seconds) so it's worth creating
  one at every interesting point during debugging, just in case you need
  to get back there later. As noted above, automatically taking
  snapshots every five minutes during recording (the minimum
  inter-snapshot delay) is also highly recommended.
