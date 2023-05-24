Debugging Firefox with rr
=========================

This page is intended to help Firefox/Gecko developers get started using rr to debug Firefox.

Prerequisites
-------------

You must have Linux installed with a recent kernel. If you're not running Linux already, an option is set up a virtual machine in which to record Firefox. Be forewarned though that

  * rr requires a VM hypervisor that virtualizes CPU performance counters. VMWare Workstation supports that.
  * there's a 20% or so performance hit from running in a VM; generally speaking recorder overhead increases from ~1.2x to ~1.4x. (It's a feather in the cap of the hypervisor authors that the hit is that small, though!)
  * Some features (reverse execution) `may not work well in VMWare <https://robert.ocallahan.org/2014/09/vmware-cpuid-conditional-branch.html>`__ due to a VMWare optimization that can be disabled `this way <http://robert.ocallahan.org/2015/11/rr-in-vmware-solved.html>`__.

When using VSCode, consider adding the `Midas <https://github.com/farre/midas>`__ addon to debug rr traces from the editor. You can use Midas to install / build rr and to configure the :code:`auto-load safe path` via the setup commands.

Ensure that you've `installed <http://rr-project.org/>`__ or `built <https://github.com/mozilla/rr/wiki/Building-And-Installing>`__ rr and have `used it successfully <https://github.com/mozilla/rr/wiki/Usage>`__. Check that you have the latest version.

You likely need to configure your :code:`auto-load safe path` for rr / gdb to work correctly. If you are on Linux and your builds are in ~/moz, then add the following to ~/.gdbinit

.. code::

   add-auto-load-safe-path ~/moz

Firefox developers are strongly encouraged to build rr from source. If your Firefox patch triggers a bug in rr, rr developers will fix that bug with high priority. You might be able to pull a fix within a few hours or days instead of waiting for the next release.

Recording Firefox
-----------------


To record Firefox running normally, simply launch it under rr as you would if running it under valgrind or gdb

.. code:: bash

   $ rr $ff-objdir/dist/bin/firefox ...

or use mach

.. code:: bash

   $ ./mach run --debugger=rr

This will save a trace to your working directory as described in the `usage instructions <https://github.com/mozilla/rr/wiki/Usage>`__. Please refer to `those instructions <https://github.com/mozilla/rr/wiki/Usage>`__ for details on how to debug the recording, which isn't covered in this document.

Sandboxing reduces recording performance because of the SIGSYS signals and extra syscall. Disabling it can help.

The background hang monitor might also be making things worse by causing a lot of extra syscalls. It can be disabled by setting
toolkit.content-background-hang-monitor.disabled=true.

SIGSYS
------

When recording and replaying Firefox running with the Linux sandbox, you will get SIGSYS signals frequently. This is expected behavior caused by the sandbox. In gdb, use handle SIGSYS noprint nostop to suppress the signals.

Recording test suites
---------------------

You can use the test runners' --debugger feature to punch rr down through the layers of python script to where Firefox is launched. This is used in the same way you would use --debugger to run valgrind or gdb, for example:

.. code:: bash

   $ ./mach mochitest --debugger=rr ...

The test harnesses disable the slow-script timeout when the --debugger argument is passed. That's usually sensible, because you don't want those warnings being generated while Firefox is stopped in gdb. However, this has been `observed to change Gecko behavior <https://bugzilla.mozilla.org/show_bug.cgi?id=986673>`__. rr doesn't need to have the slow-script timeout disabled, so to avoid those kinds of pitfalls, pass the --slowscript argument to the test harness.

To run rr in chaos mode:

.. code:: bash

   $ ./mach mochitest --debugger=rr --debugger-args="record --chaos"

You can also run the entire test harness in rr:

.. code:: bash

   $ rr ./mach mochitest ...

The trace will contain many processes, so to debug the correct one, you'll want to use rr ps or rr replay -p firefox etc.

Working with multiple processes
------------------------------

rr should work out of the box with multi-process Firefox. Once you have a recording you can use rr ps to show all the process that were recorded and rr replay -p <pid> to attach to a particular process.

If you want to debug a particular part of code, you can use the :code:`MOZ_DBG` macro and :code:`getpid()` function to write the process id to stderr. `MOZ_LOG <https://firefox-source-docs.mozilla.org/xpcom/logging.html>`__ will include the pid in log messages by default.

You can combine that with the -M and -g flags to jump to a particular point in a particular process's lifetime.

Get help!
---------

If you encounter a problem with rr, please `file an issue <https://github.com/mozilla/rr/issues>`__. Firefox bugs are high priority, so usually your issue can be fixed very quickly.

If you want to chat with rr developers, because you need more help or want to contribute or want to complain, we hang out in the `#rr channel <https://chat.mozilla.org/#/room/#rr:mozilla.org>`__. There is also a channel for `#midas <https://chat.mozilla.org/#/room/#midas:mozilla.org>`__.

You also may find `these debugging protips <https://github.com/mozilla/rr/wiki/Debugging-protips>`__ helpful, though many are for rr developers, not users.

Happy debugging!
