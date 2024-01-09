Debugging Firefox with LLDB
===========================

See http://lldb.llvm.org/index.html.

Mozilla-specific lldb settings
------------------------------

There's an
``.lldbinit`` `file <https://searchfox.org/mozilla-central/source/.lldbinit>`_
in the Mozilla source tree, which applies recommended settings and
includes a few type summaries and Mozilla-specific debugging commands
via the lldbutils module (see
`python/lldbutils/README.txt <https://searchfox.org/mozilla-central/source/python/lldbutils/README.txt>`__).
For information about available features see the links above and the `Using
LLDB to debug Gecko <http://mcc.id.au/blog/2014/01/lldb-gecko>`__ blog
post.

The in-tree ``.lldbinit`` should be loaded automatically in most cases
when running lldb from the command line (e.g. using
:ref:`mach`), but **not**
when using Xcode. See :ref:`Debugging on macOS` for information on setting up
Xcode.

.. warning::

   LLDB warning: Xcode 5 only comes with lldb (gdb is gone). The
   introduction and use of UNIFIED_SOURCES in the source starting around
   November 2013 has broken the default LLDB configuration so that it
   will not manage to resolve breakpoints in files that are build using
   UNIFIED_SOURCES (the breakpoints will be listed as "pending", and
   lldb will not stop at them). To fix this add the following to your
   $HOME/.lldbinit file:

   .. code::

      # Mozilla's use of UNIFIED_SOURCES to include multiple source files into a
      # single compiled file breaks lldb breakpoint setting. This works around that.
      # See http://lldb.llvm.org/troubleshooting.html for more.
      settings set target.inline-breakpoint-strategy always

   Restart Xcode/lldb and restart your debugging session. If that still
   doesn't fix things then try closing Xcode/lldb, doing a clobber
   build, reopening Xcode/lldb, and restarting your debugging session.

Starting a debugging session
----------------------------

Attaching to an existing process
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can attach to Firefox with following command:

.. code::

   (lldb) process attach --name firefox

Some versions of lldb causes crashes after attaching to Firefox.

Running a new process
~~~~~~~~~~~~~~~~~~~~~

To start Firefox under the debugger, run ``lldb`` followed by "--",
followed by the command line you'd like to run, like this:

.. code:: bash

   $ lldb -- obj-ff-dbg/dist/Nightly.app/Contents/MacOS/firefox -no-remote -profile /path/to/profile

Then set breakpoints you need and start the process:

.. code::

   (lldb) breakpoint set --name nsInProcessTabChildGlobal::InitTabChildGlobal
   Breakpoint created: 1: name = 'nsInProcessTabChildGlobal::InitTabChildGlobal', locations = 0 (pending)
   WARNING: Unable to resolve breakpoint to any actual locations.

   (lldb) r
   Process 7602 launched: '/.../obj-ff-opt/dist/Nightly.app/Contents/MacOS/firefox' (x86_64)
   1 location added to breakpoint 1
