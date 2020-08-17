Debugging Firefox with GDB
==========================

This page details how you can more easily debug Firefox and work around
some GDB problems.

Use GDB 5, or higher. A more recent version of GDB can be obtained from
`sourceware <https://sourceware.org/gdb/>`__ or your Linux distro repo.
If you are running less than 256 MB of RAM, be sure to see `Using gdb on
wimpy computers </en/Using_gdb_on_wimpy_computers>`__.

Where can I find general gdb documentation?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Using GDB is beyond the scope of this document. Documentation is likely
available on your system if you have GDB installed, in the form of
**info,** **man** pages, or the gnome help browser. Additionally, you
can use a graphical front-end to GDB like
`ddd <https://www.gnu.org/software/ddd/>`__ or
`insight <https://sourceware.org/insight/>`__. For more information see
https://sourceware.org/gdb/current/onlinedocs/gdb/

How do I run Firefox under gdb?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The preferred method, is using the
`mach </en-US/docs/Developer_Guide/mach>`__ command-line tool to run the
debugger, which can bypass several optional defaults.  Use "mach help
run" to get more details.  If inside the source directory, you would use
"./mach".  If you have previously `added mach to your
path </en-US/docs/Developer_Guide/mach#Adding_mach_to_your_shell's_search_path>`__,
then just use "mach". Please note that `mach is aware of
mozconfigs </en-US/docs/Developer_Guide/mach#mach_and_mozconfigs>`__.

.. code:: eval

   $ ./mach run --debug [arguments to pass to firefox]

If you need to direct arguments to gdb, you can use '--debugger-args'
options via the command line parser, taking care to adhere to shell
splitting rules. For example, if you wanted to run the command 'show
args' when gdb starts, you would use:

.. code:: eval

   $ ./mach run --debug --debugger-args "-ex 'show args'"

Alternatively, you can run gdb directly against Firefox. However, you
won't get some of the more useful capabilities this way. For example,
mach sets an environment variable (see below) to stop the JS engine from
generating synthetic segfaults to support the slower script dialoging
mechanism.

.. code:: eval

   $ gdb OBJDIR/dist/bin/firefox

See this `old
version </index.php?title=en/Debugging_Mozilla_with_gdb&revision=43>`__
for specialized instructions on older builds of Firefox.

How do I pass arguments in prun?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the arguments in GDB before calling prun. Here's an example on how
to do that:

.. code:: eval

   (gdb) set args https://www.mozilla.org
   (gdb) prun

How do I set a breakpoint in a library that hasn't been loaded?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

GDB 6.1 and above has support for "pending breakpoints". This is
controlled by the "``set breakpoint pending``" setting, and is enabled
by default.  If a breakpoint cannot be immediately resolved, it will be
re-checked each time a shared library is loaded, by the process being
debugged. If your GDB is older than this, you should upgrade.

In older versions, there isn't a way to set breakpoints in a library
that has not yet been loaded. See more on `setting a breakpoint when a
component is
loaded <#How_do_I_set_a_breakpoint_when_a_component_is_loaded.3F>`__. If
you have to set a breakpoint you can set a breakpoint in ``_dl_open``.
This function is called when a new library is loaded, when you can
finally set your breakpoint.

How do I set a breakpoint when a component is loaded? 
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. note::

   In Firefox Version 57 (and possibly earlier) XPCOM_BREAK_ON_LOAD does
   not seem to exist.

There's a facility in XPCOM which allows you to set an environment
variable to drop into the debugger when loading a certain component. You
have to set ``XPCOM_BREAK_ON_LOAD`` variable before you run Firefox,
setting it to a string containing the names of libraries you want to
load. For example, if you wish to stop when a library named ``raptor``
or ``necko`` is loaded, you set the variable to ``raptor:necko``. Here's
an example:

.. code:: eval

   (gdb) set env XPCOM_BREAK_ON_LOAD raptor:necko
   (gdb) prun

Why can't I set a breakpoint?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You probably can't set a breakpoint because its library hasn't been
loaded. Most Firefox functionality is in libraries loaded mid-way
through the ``main()``\ function. If you break on ``main(),``\ and step
through until the libraries are loaded, with a call to
``InitCOMGlue()``, you should be able to set breakpoints on many more
symbols, source files, and continue running.

::

   (gdb) break main
   (gdb) run
   Breakpoint 1, main(argc=4, argv=0x7fffffffde98, envp=0x7ffffffffdec0) .....
   256    {
   (gdb) next
   ...
   293      nsresult rv = InitXPCOMGlue()
   (gdb) next

If you still can't set the breakpoints, you need to confirm the library
has loaded. You can't proceed until the library loads. See more on
`loading shared libraries <#How_do_I_load_shared_libraries.3F>`__. If
you wish to break as soon as the library is loaded, see the section on
`breaking when a component is
loaded <#How_do_I_set_a_breakpoint_when_a_component_is_loaded.3F>`__ and
`breaking on a library
load <#How_do_I_set_a_breakpoint_when_a_component_is_loaded.3F>`__.

How do I display PRUnichar's?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

One suggestion is this:

.. code:: eval

   (gdb) print ((PRUnichar*)uri.mBuffer)[0]@16
   $47 = {114, 100, 102, 58, 110, 117, 108, 108, 0, 0, 8, 0, 0, 0, 37432,
   16514}

 

.. code:: eval

   (gdb) print aURI
   $1 = (const PRUnichar *) 0x855e6e0
   (gdb) x/32ch aURI
   0x855e6e0:      104 'h' 116 't' 116 't' 112 'p' 58 ':'  47 '/'  47 '/'  119 'w'
   0x855e6f0:      119 'w' 119 'w' 46 '.'  109 'm' 111 'o' 122 'z' 105 'i' 108 'l'
   0x855e700:      108 'l' 97 'a'  46 '.'  111 'o' 114 'r' 103 'g' 47 '/'  115 's'
   0x855e710:      116 't' 97 'a'  114 'r' 116 't' 47 '/'  0 '\0'  25 '\031'       0 '\0'
   (gdb)

-  Define helper functions in your .gdbinit

.. code:: brush:

   # Define a "pu" command to display PRUnichar * strings (100 chars max)
   # Also allows an optional argument for how many chars to print as long as
   # it's less than 100.
   def pu
     set $uni = $arg0
     if $argc == 2
       set $limit = $arg1
       if $limit > 100
         set $limit = 100
       end
     else
       set $limit = 100
     end
     # scratch array with space for 100 chars plus null terminator.  Make
     # sure to not use ' ' as the char so this copy/pastes well.
     set $scratch = "____________________________________________________________________________________________________"
     set $i = 0
     set $scratch_idx = 0
     while (*$uni && $i++ < $limit)
       if (*$uni < 0x80)
         set $scratch[$scratch_idx++] = *(char*)$uni++
       else
         if ($scratch_idx > 0)
       set $scratch[$scratch_idx] = '\0'
       print $scratch
       set $scratch_idx = 0
         end
         print /x *(short*)$uni++
       end
     end
     if ($scratch_idx > 0)
       set $scratch[$scratch_idx] = '\0'
       print $scratch
     end
   end

   # Define a "ps" command to display subclasses of nsAC?String.  Note that
   # this assumes strings as of Gecko 1.9 (well, and probably a few
   # releases before that as well); going back far enough will get you
   # to string classes that this function doesn't work for.
   def ps
     set $str = $arg0
     if (sizeof(*$str.mData) == 1 && ($str.mFlags & 1) != 0)
       print $str.mData
     else
       pu $str.mData $str.mLength
     end
   end

`This is hard. Give me a .gdbinit that already has the
functions. <#This_is_hard._Give_me_a_.gdbinit_that_works.>`__

-  Define a small helper function "punichar" in #ifdef NS_DEBUG code
   somewhere.

How do I display an nsString?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can call the ToNewCString() method on the nsString. It leaks a
little memory but it shouldn't hurt anything if you only do it a few
times in one gdb session. (via akkana@netscape.com)

.. code:: eval

   (gdb) p string.ToNewCString()

Another method (via bent) is the following (replace ``n`` with: the
returned length of your string):

::

   (gdb) p string.Length()
   $1 = n
   (gdb) x/ns string.BeginReading()

You can of course use any of the above unichar-printing routines instead
of x/s.

This is hard. Give me a .gdbinit that works.
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

See `Boris Zbarsky's
.gdbinit <http://web.mit.edu/bzbarsky/www/gdbinit>`__. It contained
several function definitions including:

-  "prun" to start the browser and disable library loading.
-  "pu" which will display a (PRUnichar \*) string.
-  "ps" which will display a nsString.

How do I determine the concrete type of an object pointed to by an interface pointer?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You can determine the concrete type of any object pointed to, by an
XPCOM interface pointer, by looking at the mangled name of the symbol
for the object's vtable:

.. code:: eval

   (gdb) p aKidFrame
   $1 = (nsIFrame *) 0x85058d4
   (gdb) x/wa *(void**)aKidFrame
   0x4210d380 <__vt_14nsRootBoxFrame>: 0x0
   (gdb) p *(nsRootBoxFrame*)aKidFrame
    [ all the member variables of aKidFrame ]

If you're using gcc 3.x, the output is slightly different from the gcc
2.9x output above. Pay particular attention to the vtable symbol, in
this case ``__vt_14nsRootBoxFrame``. You won't get anything useful if
the shared library containing the object is not loaded. See `How do I
load shared libraries? <#How_do_I_load_shared_libraries.3F>`__ and `How
do I see what libraries I already have
loaded? <#How_do_I_see_what_libraries_I_already_have_loaded.3F>`__

Or use the gdb command ``set print object on``.

How can I debug JavaScript from gdb?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you have JavaScript Engine code on the stack, you'll probably want a
JS stack in addition to the C++ stack.

::

   (gdb) call DumpJSStack() 

See `https://developer.mozilla.org/en-US/docs/Mozilla/Debugging/Debugging_JavaScript </en-US/docs/Mozilla/Debugging/Debugging_JavaScript>`__
for more JS debugging tricks.

How can I debug race conditions and/or how can I make something different happen at NS_ASSERTION time?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

| [submitted by Dan Mosedale]
| As Linux is unable to generate useful core files for multi-threaded
  applications, tracking down race-conditions which don't show up under
  the debugger can be a bit tricky. Unless you've given the
  ``--enable-crash-on-assert`` switch to ``configure``, you can now
  change the behavior of ``NS_ASSERTION`` (nsDebug::Break) using the
  ``XPCOM_DEBUG_BREAK`` environment variable.

How do I run the debugger in emacs/xemacs?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Emacs and XEmacs contain modes for doing visual debugging. However, you
might want to set up environment variables, specifying the loading of
symbols and components. The easiest way to set up these is to use the
``run-mozilla.sh`` script, located in the dist/bin directory of your
build. This script sets up the environment to run the editor, shell,
debugger, or defining a preferred setup and running any commands you
wish. For example:

.. code:: eval

   $ ./run-mozilla.sh /bin/bash
   MOZILLA_FIVE_HOME=/home/USER/src/mozilla/build/dist/bin
     LD_LIBRARY_PATH=/home/USER/src/mozilla/build/dist/bin
        LIBRARY_PATH=/home/USER/src/mozilla/build/dist/bin
          SHLIB_PATH=/home/USER/src/mozilla/build/dist/bin
             LIBPATH=/home/USER/src/mozilla/build/dist/bin
          ADDON_PATH=/home/USER/src/mozilla/build/dist/bin
         MOZ_PROGRAM=/bin/bash
         MOZ_TOOLKIT=
           moz_debug=0
        moz_debugger=

GDB 5 used to work for me, but now Firefox won't start. What can I do?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A recent threading change (see `bug
57051 <https://bugzilla.mozilla.org/show_bug.cgi?id=57051>`__ for
details) caused a problem on some systems. Firefox would get part-way
through its initialization, then stop before showing a window. A recent
change to gdb has fixed this. Download and build `the latest version of
Insight <https://sources.redhat.com/insight/>`__, or if you don't want a
GUI, `the latest version of gdb <https://sources.redhat.com/gdb/>`__.

"run" or "prun" in GDB fails with "error in loading shared libraries."
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Running mozilla-bin inside GDB fails with an error message like:

.. code:: eval

   Starting program:
   /u/dmose/s/mozilla/mozilla-all/mozilla/dist/bin/./mozilla-bin
   /u/dmose/s/mozilla/mozilla-all/mozilla/dist/bin/./mozilla-bin: error
   in loading shared libraries: libraptorgfx.so: cannot open shared
   object file: No such file or directory

Your LD_LIBRARY_PATH is probably being reset by your .cshrc or .profile.
From the GDB manual:

*\*Warning:\* GDB runs your program using the shell indicated by your
'SHELL' environment variable if it exists (or '/bin/sh' if not). If your
'SHELL' variable names a shell that runs an initialization file -- such
as '.cshrc' for C-shell, or '.bashrc' for BASH--any variables you set in
that file affect your program. You may wish to move the setting of
environment variables to files that are only run when you sign on, such
as '.login' or '.profile'.*

Debian's GDB doesn't work. What do I do?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Debian's unstable distribution currently uses glibc 2.1 and GDB 4.18.
However, there is no package of GDB for Debian with the appropriate
threads patches that will work with glibc 2.1. I was able to get this to
work by getting the GDB 4.18 RPM from Red Hat's rawhide server and
installing that. It has all of the patches necessary for debugging
threaded software. These fixes are expected to be merged into GDB, which
will fix the problem for Debian Linux. (via `Bruce
Mitchener <mailto:bruce@cybersight.com>`__)

Firefox is aborting. Where do I set a breakpoint to find out where it is exiting?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

On Linux there are two possible symbols that are causing this:
``PR_ASSERT()`` and ``NS_ASSERTION()``. To see where it's asserting you
can stop at two places:

.. code:: eval

   (gdb) b abort
   (gdb) b exit

I keep getting a SIGSEGV in JS/JIT code under gdb even though there is no crash when gdb is not attached.  How do I fix it?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Set the JS_DISABLE_SLOW_SCRIPT_SIGNALS environment variable (in FF33,
the shorter and easier-to-remember JS_NO_SIGNALS).  For an explanation,
read `Jan's blog
post <https://www.jandemooij.nl/blog/2014/02/18/using-segfaults-to-interrupt-jit-code/>`__.

I keep getting a SIG32 in the debugger. How do I fix it?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If you are getting a SIG32 while trying to debug Firefox you might have
turned off shared library loading before the pthreads library was
loaded. For example, ``set auto-solib-add 0`` in your ``.gdbinit`` file.
In this case, you can either:

-  Remove it and use the method explained in the section about `GDB's
   memory
   usage <#The_debugger_uses_a_lot_of_memory._How_do_I_fix_it.3F>`__
-  Use ``handle SIG32 noprint`` either in gdb or in your ``.gdbinit``
   file

Alternatively, the problem might lie in your pthread library. If this
library has its symbols stripped, then GDB can't hook into thread
events, and you end up with SIG32 signals. You can check if your
libpthread is stripped in ``file /lib/libpthread*`` and looking for
``'stripped'.``\ To fix this problem on Gentoo Linux, you can re-emerge
glibc after adding ``"nostrip"`` to your ``FEATURES`` in
``/etc/make.conf``.

How do I get useful stack traces inside system libraries?
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Many Linux distributions provide separate packages with debugging
information for system libraries, such as gdb, Valgrind, profiling
tools, etc., to give useful stack traces via system libraries.

Fedora
^^^^^^

On Fedora, you need to enable the debuginfo repositories, as the
packages are in separate repositories. Enable them permanently, so when
you get updates you also get security updates for these packages. A way
to do this is edit ``/etc/yum.repos.d/fedora.repo`` and
``fedora-updates.repo`` to change the ``enabled=0`` line in the
debuginfo section to ``enabled=1``. This may then flag a conflict when
upgrading to a new distribution version. You would the need to perform
this edit again.

You can finally install debuginfo packages with yum or other package
management tools. The best way is install the ``yum-utils`` package, and
then use the ``debuginfo-install`` command to install all the debuginfo:

::

   # yum install yum-utils
   # debuginfo-install firefox 

This can be done manually using:

.. code:: eval

    # yum install GConf2-debuginfo ORBit2-debuginfo atk-debuginfo \
    cairo-debuginfo dbus-debuginfo dbus-glib-debuginfo expat-debuginfo \
    fontconfig-debuginfo freetype-debuginfo gcc-debuginfo glib2-debuginfo \
    glibc-debuginfo gnome-vfs2-debuginfo gtk2-debuginfo gtk2-engines-debuginfo \
    hal-debuginfo libX11-debuginfo libXcursor-debuginfo libXext-debuginfo \
    libXfixes-debuginfo libXft-debuginfo libXi-debuginfo libXinerama-debuginfo \
    libXrender-debuginfo libbonobo-debuginfo libgnome-debuginfo \
    libselinux-debuginfo pango-debuginfo popt-debuginfo scim-bridge-debuginfo

Ubuntu 8.04
^^^^^^^^^^^

Ubuntu provides similar debug symbol packages for many of its libraries,
though not all of them. To install them, run:

.. code:: eval

    $ sudo apt-get install libatk1.0-dbg libc6-dbg libcairo2-dbg \
    libfontconfig1-dbg libgcc1-dbg libglib2.0-0-dbg libgnomeui-0-dbg \
    libgnomevfs2-0-dbg libgnutls13-dbg libgtk2.0-0-dbg libice6-dbg \
    libjpeg62-dbg libpango1.0-0-dbg libpixman-1-0-dbg libstdc++6-4.2-dbg \
    libx11-6-dbg libx11-xcb1-dbg libxcb1-dbg libxft2-dbg zlib1g-dbg

Debugging electrolysis (e10s)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

``mach run`` and ``mach test`` both accept a ``--disable-e10s``
argument. Some debuggers can't catch child-process crashes without it.

You can find some (outdated) information on
https://wiki.mozilla.org/Electrolysis/Debugging. You may also like to
read
https://mikeconley.ca/blog/2014/04/25/electrolysis-debugging-child-processes-of-content-for-make-benefit-glorious-browser-of-firefox
for a more up-to-date blog post.

To get the child process id use:

::

   MOZ_DEBUG_CHILD_PROCESS=1 mach run

 See also
~~~~~~~~~

-  `Debugging </En/Debugging>`__
-  `Performance tools <https://wiki.mozilla.org/Performance:Tools>`__
-  `Fun with
   gdb <https://blog.mozilla.com/sfink/2011/02/22/fun-with-gdb/>`__ by
   Steve Fink
-  `Archer pretty printers for
   SpiderMonkey <https://hg.mozilla.org/users/jblandy_mozilla.com/archer-mozilla>`__
   (`blog
   post <https://itcouldbesomuchbetter.wordpress.com/2010/12/20/debugging-spidermonkey-with-archer-2/>`__)
-  `More pretty
   printers <https://hg.mozilla.org/users/josh_joshmatthews.net/archer-mozilla/>`__
   for Gecko internals (`blog
   post <https://www.joshmatthews.net/blog/2011/06/nscomptr-has-never-been-so-pretty/>`__)

.. container:: originaldocinfo

   .. rubric:: Original Document Information
      :name: Original_Document_Information

   -  `History <http://bonsai-www.mozilla.org/cvslog.cgi?file=mozilla-org/html/unix/debugging-faq.html&rev=&root=/www/>`__
   -  Copyright Information: © 1998-2008 by individual mozilla.org
      contributors; content available under a `Creative Commons
      license <https://www.mozilla.org/foundation/licensing/website-content.html>`__

 
