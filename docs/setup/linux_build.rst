Building Firefox On Linux
=========================

They aren’t complicated, but there are a few prerequisites to building Firefox on Linux. You need:

#. A 64-bit installation of Linux. You can check by opening a terminal window; if ``uname -m`` returns ``x86_64`` you can proceed.
#. Next, you’ll need Python 3.6 or later installed. You can check with ``python3 --version`` to see if you have it already. If not, see `Installing Python <#installingpython>`_.
#. Finally, a reasonably fast internet connection and 30GB of free disk space.

Getting Started
---------------

Getting set up on Linux is fast and easy.

If you don’t have one yet, create a "``src``" directory for
yourself under your home directory:

.. code-block:: shell

   mkdir src && cd src

Next `download the bootstrap.py
script <https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py>`_
and save it in the ``src/`` directory created above.

.. warning::

   Building Firefox in Linux on top of a non-native file system -
   for example, on a mounted NTFS partition - is explicitly not
   supported. While a build environment like this may succeed it
   may also fail while claiming to have succeeded, which can be
   quite difficult to diagnose and fix.

And finally, in your terminal from above start the bootstrapper
like this:

.. code-block:: shell

   python3 bootstrap.py

... and follow the prompts. This will use mercurial to checkout
the source code. If you prefer to work with git, use this command
instead:

.. code-block:: shell

   python3 bootstrap.py --vcs=git

Let’s Build Firefox
-------------------

You’re ready; now we can tie it all together. In your terminal:

.. code-block:: shell

   cd mozilla-central

If you are not working on the C/C++ files you can also opt for
:ref:`Artifact Builds <Understanding Artifact Builds>`
which are much faster. To enable artifact build set up a
`.mozconfig <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options>`_
file with the following options:

.. code-block:: shell

   # Automatically download and use compiled C++ components:
   # This option will disable C/C++ compilation
   ac_add_options --enable-artifact-builds

   # Write build artifacts to (not mandatory):
   mk_add_options MOZ_OBJDIR=./objdir-frontend

If you plan to walk through code with a debugger, set up a
`.mozconfig <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Configuring_Build_Options>`_
file with the following options:

.. code-block:: shell

   ac_add_options --disable-optimize
   ac_add_options --enable-debug


Older clang versions (especially clang 6) `from LTS linux
distributions sometimes miscompile
Firefox <https://bugzilla.mozilla.org/show_bug.cgi?id=1594686>`_,
resulting in startup crashes when starting the resulting build.
If this happens, you can force the use of the ``clang`` version
that ``./mach bootstrap`` downloaded by adding the following to
your ``.mozconfig``:

.. code-block:: shell

   export CC=path/to/home/.mozbuild/clang/bin/clang
   export CXX=path/to/home/.mozbuild/clang/bin/clang++

And finally, run the build command:

.. code-block:: shell

   ./mach build

If you encounter any error related to LLVM/Clang on Ubuntu or
Debian, download the latest version of LLVM and Clang and then
re-run ``./mach build``.

And you’re on your way, building your own copy of Firefox from
source. Don’t be discouraged if this takes a while; this takes
some time on even the fastest modern machines, and as much as two
hours or more on older hardware. When the
``--enable-artifact-builds`` option is used, builds usually finish
within a few minutes.

Now the fun starts
------------------

You have the code, you’ve compiled Firefox. Fire it up with
``./mach run`` and you’re ready to start hacking. The next steps
are up to you: join us on IRC in the ``#introduction`` channel,
and find `a bug to start working
on. <https://codetribute.mozilla.org/>`_


General considerations
----------------------

#. 4GB RAM with an additional 4GB of available swap space is the bare minimum, and more RAM is always better - having 8GB or more will dramatically improve build time.
#. A 64-bit x86 CPU and a 64-bit OS. As of early 2015 it is no longer possible to do a full build of Firefox from source on most 32-bit systems; a 64-bit OS is required. ":ref:`Artifact Builds <Understanding Artifact Builds>`" may be possible, but are not a supported configuration. On Linux you can determine this by typing "``uname -a``" in a terminal. It is possible to build a 32-bit Firefox on a 64-bit system, see :ref:`Building Firefox 32-bit on Linux 64-bit <Building Firefox 32-bit On Linux 64-bit>`.
#. A recent version of Clang is required to build Firefox. You can learn more about the features we use and their :ref:`compiler support <Using C++ in Mozilla code>`.
#. Most Linux distros now install a later version of autoconf, which the build system cannot use, reporting the error "``*** Couldn't find autoconf 2.13.  Stop.``" However a separate ``autoconf2.13`` package is usually available. To install `autoconf 2.13` in Debian based distros copy this line and paste it into a terminal window:

.. code-block:: shell

   $ sudo apt install autoconf2.13

5. If you are on a Fedora machine then simply install the following prerequisites from the terminal window:

.. code-block:: shell

   sudo dnf install @development-tools @c-development autoconf213 gtk2-devel gtk3-devel libXt-devel GConf2-devel dbus-glib-devel yasm-devel alsa-lib-devel pulseaudio-libs-devel


.. _installingpython:

Installing Python
-----------------

To build Firefox, it's necessary to have a Python of version 3.6 or later
installed. Python 2 is no longer required to build Firefox, although it is still
required for some development tasks, like testing and pushing to ``try``.

Often, you can install both Python 2 and 3 with your system package manager.
Make sure your system is up to date! However, users on older Linux distributions
might find they are unable to install a recent enough Python 3, while users on
newer Linux distributions may find that they can no longer install Python 2.7.
`pyenv <https://github.com/pyenv/pyenv>`_ is an easy way to install arbitrary
Python versions if you fall into either of these categories. Your system package
manager may or may not provide ``pyenv``, but the ``pyenv`` GitHub repository
provides detailed `manual installation instructions
<https://github.com/pyenv/pyenv#installation>`_ in any case.

Once you have ``pyenv`` configured properly and ``pyenv``'s ``shims`` directory
at the front of your ``$PATH``, you can easily install any version of Python
and configure your project to use them. For example, at the root of your
checkout, do the following:

.. code-block:: shell

    pyenv install 2.7.17
    pyenv install 3.7.8
    pyenv local 3.7.8 2.7.17


Requirements for Debian / Ubuntu users
--------------------------------------

You need a number of different packages:

.. code-block:: shell

   # the rust compiler
   aptitude install rustc

   # the rust package manager
   aptitude install cargo

   # the required (old) version of autoconf
   aptitude install autoconf2.13

   # the headers of important libs
   aptitude install libgtk-2-dev
   aptitude install libgtk-3-dev
   aptitude install libgconf2-dev
   aptitude install libdbus-glib-1-dev
   aptitude install libpulse-dev

   # rust dependencies
   cargo install cbindgen

   # an assembler for compiling webm
   aptitude install yasm

   # Python 3 dependencies. This will work on Ubuntu 18.04LTS and later or
   # Debian buster and later. For earlier releases of Ubuntu or Debian, you
   # may prefer to use pyenv.
   aptitude install python3 python3-dev python3-pip python3-setuptools

   # Python 2 dependencies. This will work on Ubuntu versions prior to 20.04 LTS
   # and Debian versions prior to bullseye. For later releases of Ubuntu or
   # Debian, you may prefer to use pyenv.
   aptitude install python python-dev python-pip python-setuptools


One-Line Bootstrapping
----------------------

Our system bootstrapping script can automatically install the required
dependencies. You can download and run it by copying this line and
pasting it into a terminal window:

.. code-block:: shell

   wget -q https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O bootstrap.py && python3 bootstrap.py

.. note::

   Note: piping bootstrap.py to stdin of a python process will cause
   interactive prompts in the bootstrap script to fail, causing the
   bootstrap process to fail. You must run Python against a local file.

If the above command fails, the reason is often because some Linux
distributions ship with an outdated list of root certificates. In this
case, you should upgrade your Linux distribution or use your browser to
download the file. That ensures that you will get it from the right
source.
If you get an error from this process, consider `filing a
bug <https://bugzilla.mozilla.org/enter_bug.cgi?product=Core&component=Build%20Config>`_
saying that the bootstrapper didn't work and `contact Mike
Hoye <mailto:mhoye@mozilla.com>` directly for help. Please include the
error message and some details about your operating system.

If you have already checked out the source code via Mercurial or Git you
can also use `mach <https://developer.mozilla.org/en-US/docs/Developer_Guide/mach>`_ with the
bootstrap command:

.. code-block:: shell

   ./mach bootstrap



Common Bootstrapper Failures
----------------------------

.. code-block:: shell

   wget: command not found

You may not have wget (or curl) installed. In that case, you can either
install it via your package manager: 

On Debian-based distros like Ubuntu:

.. code-block:: shell

   sudo apt install wget 

On Fedora-based distros:

.. code-block:: shell

   sudo dnf install wget

or you can just `download
bootstrap.py <https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py>`_
using your browser and then run it with this command:

.. code-block:: shell

   python3 bootstrap.py

In some cases people who've customized their command prompt to include
emoji or other non-text symbols have found that bootstrap.py fails with
a ``UnicodeDecodeError``. We have a bug filed for that but in the
meantime if you run into this problem you'll need to change your prompt
back to something boring.


More info
---------

The above bootstrap script supports popular Linux distributions. If it
doesn't work for you, see `Linux build
prerequisites <https://developer.mozilla.org/docs/Mozilla/Developer_guide/Build_Instructions/Linux_Prerequisites>`_ for more.
