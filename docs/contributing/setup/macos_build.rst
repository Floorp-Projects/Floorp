Building Firefox On MacOS
=========================

This document will help you get set up to build Firefox on your own
computer. Getting set up won't be difficult, but it can take a while -
we need to download a lot of bytes! Even on a fast connection, this can
take ten to fifteen minutes of work, spread out over an hour or two.

The details are further down this page, but this quick-start guide
should get you up and running:

.. rubric:: Quick start (Try this first!)
   :name: Quick_start_Try_this_first!

.. rubric:: Prerequisites
   :name: Prerequisites

You will need:

-  an Apple ID to download and install Apple-distributed developer tools
   mentioned below
-  from 5 minutes to 1 hour to download and install Xcode, which is
   large
-  download and install a local copy of specific macOS SDK version

You will need administrator permissions on your machine to install these
prerequisites. (You can verify that you have these permissions in System
Preferences -> Users & Groups.)

See `1.1 Install Xcode and Xcode command line tools <#xcode>` and `1.2
Get the local macOS SDK <#macOSSDK>` for more information on how to
install these prerequisites.

.. rubric:: Getting the source
   :name: Getting_the_source
   :class: heading-tertiary

Firstly you need to prepare a directory and get the bootstrap script
that will do the rest:

.. code-block:: shell

    # the bootstrap script needs this directory, but you can choose a different target directory for the Mozilla code later
    cd ~ && mkdir -p src && cd src

    # download the bootstrap script
    curl https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -o bootstrap.py

Then in your terminal from above start the bootstrapper like this:

.. code-block:: shell

    python bootstrap.py

... and follow the prompts. This will use mercurial to checkout the
source code. If you prefer to work with git, use this command instead:

.. code-block:: shell

    python bootstrap.py --vcs=git

If you don't have `Homebrew <https://brew.sh/>` or
`Ports <https://www.macports.org/>` installed - software package
managers that will let us install some programs we'll need - you'll be
asked to pick one. Either will work, but most Mozilla developers use
Homebrew.

If you don't let the ``bootstrap.py`` script clone the source for you
make sure you do it manually afterward before moving onto the next step.

.. rubric:: Build Firefox!
   :name: Build_Firefox!
   :class: heading-tertiary highlight-spanned

Now we tie it all together.

In your terminal window, ``cd`` to your Mozilla source directory chosen
before (``mozilla-unified``) and type:

.. code-block:: shell

    # create a minimal build options file
    echo "ac_add_options --with-macos-sdk=$HOME/SDK-archive/MacOSX10.11.sdk" >> mozconfig

    ./mach bootstrap

    ./mach build

The ``./mach bootstrap`` step is a catch-all for any dependencies not
covered in this documentation. If you are working on Firefox frontends
or building Firefox without any changes, select `artifact
builds <https://developer.mozilla.org/en-US/docs/Artifact_builds>` in
the first question in ``./mach bootstrap``.  Artifact builds will
complete more quickly!  Artifact builds are unsuitable for those working
on C++ code.

You’re on your way. Don’t be discouraged if this takes a while; it takes
some time even on the fastest modern machines and as much as two hours
or more on older hardware. Firefox is pretty big, because the Web is
big.

.. rubric:: Getting Connected
   :name: Getting_Connected
   :class: heading-tertiary

That last step can take some time to finish. While it’s running, take a
moment to sign in to\ `Bugzilla <https://bugzilla.mozilla.org/>`,
Mozilla’s issue tracker. To comment on a bug or submit a patch you’ll
need a Bugzilla account. You can either use your `GitHub
account <https://github.com>`, or you can `sign up for a Bugzilla
account here. <https://bugzilla.mozilla.org/createaccount.cgi>`

As well as Bugzilla, much of Mozilla’s internal communication happens
over Internet Relay Chat (IRC). You can learn how to `connect to Mozilla
with IRC here <https://wiki.mozilla.org/IRC>`. If you’re just getting
started or have questions about getting set up you can join us in the
*"#introduction channel"*, where some of our community members hang out
to try and help new contributors get rolling.

.. rubric:: Join Mozillians.org!
   :name: Join_Mozillians.org!
   :class: heading-tertiary

There’s one more thing you can do for yourself while you’re waiting:
create an account for yourself on
`Mozillians <https://mozillians.org/>`. Mozillians is the Mozilla
community directory, where you can connect with people who share your
interests, projects or countries. This step is optional, but we think it
will be worth your while.

.. rubric:: Now the Fun Starts
   :name: Now_the_Fun_Starts
   :class: heading-tertiary

You have the code, you’ve compiled Firefox. Fire it up with
``./mach run`` and you’re ready to start hacking. The next steps are up
to you: join us on IRC in the *#introduction* channel, follow
`StartMozilla on Twitter <https://twitter.com/StartMozilla>` and find
a `bug to start working
on <http://www.joshmatthews.net/bugsahoy/?simple=1>`.

.. rubric:: Thank You
   :name: Thank_You
   :class: heading-tertiary

Mozilla's strength is the community behind it; Firefox is the product of
a global development team working to `keep the Web free, open and
participatory <https://www.mozilla.org/about/manifesto/>`, and your
contributions will make Firefox and the Web better for hundreds of
millions of people around the world.

Build steps (details)
---------------------

Building on macOS is divided into the following steps:

#. Install Apple-distributed developer tools - Xcode, Xcode cli tools
   and macOS SDK locally
#. Install supplementary build tools
#. Obtain a copy of the Mozilla source code
#. Configure the Mozilla source tree to suit your needs
#. Build Firefox

1.1 Install Xcode and Xcode command line tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You first need to install Xcode, for which you have two options but both
require you to sign in with an Apple ID:

-  From Apple Developer Download page - `direct
   link <https://developer.apple.com/download/release/>`. Install the
   latest **release** (non-beta) version of Xcode, open ``Xcode.xip``,
   and then **before** **running the extracted Xcode.app, move it from
   the download folder to /Applications**. (Running it from another
   location may screw up various build paths, homebrew builds, etc. Fix
   by running ``sudo xcode-select -switch /Applications/Xcode.app`` )
-  From the Mac App Store - `direct
   link <https://apps.apple.com/us/app/xcode>`.

Open /Applications/Xcode.app and let it do its initial first run and
setup stuff.

Install the Xcode command line tools by
running \ ``xcode-select --install`` in your terminal.

1.2 Get the local macOS SDK
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Firefox currently requires a local copy of macOS 10.11 SDK to build (all
your other apps will still use your more recent version of this SDK,
most probably matching your macOS version).

There are various issues when building the Mozilla source code with
other SDKs and that's why we recommend this specific version.

To get the 10.11 SDK, first download Xcode 7.3.1 from the `More
Downloads for Apple
Developers <https://developer.apple.com/download/more/>` page. Once
downloaded, mount the .dmg file. Then in the Terminal run the following:

.. code-block:: shell

    mkdir -p $HOME/SDK-archive
    cp -a /Volumes/Xcode/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.11.sdk $HOME/SDK-archive/MacOSX10.11.sdk

2. Install supplementary build tools
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Mozilla's source tree requires a number of third-party tools and
applications to build it. You will need to install these before you can
build anything.

You have the choice of how to install all these components. You can use
a package manager like Homebrew or Ports. Or, you can obtain, compile,
and install them individually. For simplicity and to save your time,
using a package manager is recommended. The following sections describe
how to install the packages using existing package managers. Choose
whatever package manager you prefer.

2.1a Install dependencies via Homebrew
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

`Homebrew <http://brew.sh/>` is "the missing package manager for
macOS." It provides a simple command-line interface to install packages,
typically by compiling them from source.

The first step is to install Homebrew. See https://brew.sh/

Once you have Homebrew installed, you'll need to run the following:

.. code-block:: shell

    brew install yasm mercurial gawk ccache python

You will also need Autoconf 2.13, but the core Homebrew repository will
install a newer version by default, so you need to specify the version
when installing it:

.. code-block:: shell

    brew install autoconf@2.13

If you get errors trying to build, it means you have another version of
Autoconf installed and used as default. To use Autoconf 2.13, run:

.. code-block:: shell

    brew link --overwrite autoconf@2.13

2.1b Install Dependencies via MacPorts
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

MacPorts is a package manager for macOS. If you are running Homebrew,
you can ignore this section.

To install MacPorts, go to their `install
page <http://www.macports.org/install.php>`, download the .dmg for
your platform, and install it. If you already have MacPorts installed,
ensure it is up to date by running:

.. code:: eval

    sudo port selfupdate
    sudo port sync

The first of these commands will ask for your root password.

Common errors include:

-  ``sudo`` doesn't accept a blank password: create a password for your
   account in System Preferences.
-  ``port`` command not found: add it to your path (see the
   troubleshooting section below).

Use MacPorts to install the packages needed for building Firefox:

.. code:: eval

    sudo port install libidl autoconf213 yasm

You'll then see lots of output as MacPorts builds and installs these
packages and their dependencies -- it takes a while, so go grab a cup of
coffee.

**Note:** By default, this will install Python 2.7, which in turn will
pull in all of the X11 libraries, which may take a while to build.  You
don't need any of those to build Firefox; you may want to consider
adding +no\_tkinter to the install line to build a python without
support for the X11 UI packages.  This should result in a much faster
install.

**Note:** With older versions of Xcode (eg 6.4) you may need to use
MacPorts to get the proper version of clang, such as clang-3.6 or later.
See bugs in Core, Build Config referring to clang.

2.2 Install Mercurial
~~~~~~~~~~~~~~~~~~~~~

Mozilla's source code is hosted in Mercurial repositories. You use
Mercurial to interact with these repositories. There are many ways to
install Mercurial on macOS:

#. Install `official builds from
   Selenic <http://mercurial.selenic.com/>`
#. Install via MacPorts:

.. code-block:: shell

       sudo port install mercurial

#. Install via Homebrew:

.. code-block:: shell

       brew install mercurial

#. Install via Pip:

.. code-block:: shell

       easy_install pip && pip install mercurial

Once you have installed Mercurial, test it by running:

.. code-block:: shell

    hg version

If this works, congratulations! You'll want to configure your Mercurial
settings to match other developers. See `Getting Mozilla Source Code
Using Mercurial <https://developer.mozilla.org/en-US/Developer_Guide/Source_Code/Mercurial>`.

If this fails with the error "``ValueError: unknown locale: UTF-8``",
then see the
`workarounds <http://www.selenic.com/mercurial/wiki/index.cgi/UnixInstall#head-1c10f216d5b9ccdcb2613ea37d407eb45f22a394>`
on the Mercurial wiki's Unix Install page.

When trying to clone a repository you may get an HTTP 500 error
(internal server error). This seems to be due to something that Mac
Mercurial sends to the server (it's been observed both with MacPort and
selenic.com Mercurial binaries). Try restarting your shell, your
computer, or reinstall Mercurial (in that order), then report back here
what worked, please.

3. Obtain a copy of the Mozilla source code
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

You may want to read `Getting Mozilla Source Code Using
Mercurial <https://developer.mozilla.org/en-US/Developer_Guide/Source_Code/Mercurial>` for the
complete instructions.

If you are interested in Firefox development only then run the following
command, which will create a new directory, ``mozilla-central``, in the
current one with the contents of the remote repository.

Below command will take many minutes to run, as it will be copying a
couple hundred megabytes of data over the internet.

.. code:: syntaxbox

    hg clone https://hg.mozilla.org/mozilla-central/
    cd mozilla-central

 (If you are building Firefox for Android, you should now return to the
`Android build
instructions <https://wiki.mozilla.org/Mobile/Fennec/Android#Mac_OS_X>`.)

4. Configure the build options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In your checked out source tree create a new file, ``mozconfig``, which
will contain your build options. For more on this file, see `Configuring
Build Options <https://developer.mozilla.org/en/Configuring_Build_Options>`.

To get started quickly, create the file with the following contents:

.. code:: eval

    # Define where build files should go. This places them in the directory
    # "obj-ff-dbg" under the current source directory
    mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/obj-ff-dbg

    # Enable debug builds
    ac_add_options --enable-debug

    # Use the local copy of specific version of macOS SDK compatible with Mozilla source code
    ac_add_options --with-macos-sdk=$HOME/SDK-archive/MacOSX10.11.sdk

Firefox no longer builds with gcc 4.8 or earlier, but the build system
should automatically select clang if it is available in the PATH. If
that is not the case, you need to set CC and CXX. For instance, if you
installed Clang 3.7 via Homebrew, then you need to have this in your
``mozconfig``:

.. code:: eval

    CC=clang-mp-3.7
    CXX=clang++-mp-3.7

If you installed Autoconf 2.13 with the Homebrew recipe linked above,
you may need to add the following to your ``mozconfig``:

.. code:: eval

    mk_add_options AUTOCONF=/usr/local/Cellar/autoconf@2.13/2.13/bin/autoconf213

5. Build
~~~~~~~~

Once you have your ``mozconfig`` file in place, you should be able to
build!

.. code-block:: shell

    ./mach build

If the build step works, you should be able to find the built
application inside ``obj-ff-dbg/dist/``. If building the browser with
``--enable-debug``, the name of the application is ``NightlyDebug.app``.
To launch the application, try running the following:

.. code-block:: shell

    ./mach run

**Note:** The compiled application may also be named after the branch
you're building; for example, if you changed these instructions to fetch
the ``mozilla-1.9.2`` branch, the application will be named
``Namoroka.app`` or ``NamorokaDebug.app``.

Hardware requirements
---------------------

There are no specific hardware requirements, provided that the hardware
accommodates all of the `software <#Software_Requirements>` required
to build Firefox. Firefox can take a long time to build, so more CPU,
more RAM and lots of fast disks are always recommended.

-  **Processor:** Intel CPUs are required. Building for PowerPC chips is
   not supported.
-  **Memory:** 2GB RAM minimum, 8GB recommended.
-  **Disk Space:** At least 30GB of free disk space.

Software requirements
---------------------

-  **Operating System:** Mac OS X 10.9 or later. It is advisable to
   upgrade to the latest “point” release by running Software Update,
   found in the Apple menu. You will need administrative privileges to
   set up your development environment
-  **Development Environment:** Xcode. You can obtain from the App
   Store.
-  **Package Management:** Either
   *`MacPorts <http://www.macports.org/>`* or Homebrew.

These options are specific to Mozilla builds for macOS. For a more
general overview of build options and the ``mozconfig`` file, see
`Configuring Build Options <https://developer.mozilla.org/en/Configuring_Build_Options>`. For
specific information on configuring to build a universal binary, see
`Mac OS X Universal Binaries <https://developer.mozilla.org/en/Mac_OS_X_Universal_Binaries>`.

-  **Compiler:** Firefox releases are no longer built with gcc-4.8 or
   earlier. A recent copy of clang is needed.

   -  There are some options on where to get clang:

      -  Newer versions of Xcode. The one in Xcode 7.0 or newer and the
         open source 3.6 release should work.
         (Xcode 6.4 is based on pre-release of clang 3.6, that doesn't
         match to requirement.)
      -  Following the instructions in the `clang
         website <http://clang.llvm.org/get_started.html>` for
         information on how to get it.
      -  Using some of the package managers (see above).

   -  Once clang is installed, make sure it is on the PATH and configure
      should use it.

The following options, specified with ``ac_add_options``, are lines that
are intended to be added to your ``mozconfig`` file.

-  macOS **SDK:** This selects the version of the system headers and
   libraries to build against, ensuring that the product you build will
   be able to run on older systems with less complete APIs available.
   Selecting an SDK with this option overrides the default headers and
   libraries in ``/usr/include``, ``/usr/lib``, and ``/System/Library``.
   Mac macOS SDKs are installed in ``/Developer/SDKs`` during the `Xcode
   installation <#Software_Requirements>` by selecting the **Cross
   Development** category in the installer’s **Customize** screen.

.. code-block:: shell

       ac_add_options --with-macos-sdk=/path/to/SDK

   Official trunk builds use ``/Developer/SDKs/MacOSX10.11.sdk``. Check
   ```build/macosx/universal/mozconfig.common`` <https://dxr.mozilla.org/mozilla-central/source/build/macosx/cross-mozconfig.common#23>`
   for the SDK version used for official builds of any particular source
   release.

   Applications built against a particular SDK will usually run on
   earlier versions of Mac macOS as long as they are careful not to use
   features or frameworks only available on later versions. Note that
   some frameworks (notably AppKit) behave differently at runtime
   depending on which SDK was used at build time. This may be the source
   of bugs that only appear on certain platforms or in certain builds.

For macOS builds, defines are set up as follows:

-  ``XP_MACOSX`` is defined
-  ``XP_UNIX`` is defined
-  ``XP_MAC`` is **not** defined. ``XP_MAC`` is obsolete and has been
   removed from the source tree (see {{ Bug(281889) }}). It was used for
   CFM (non-Mach-O) builds for the classic (pre-X) Mac OS.

This requires care when writing code for Unix platforms that exclude
Mac:

.. code-block:: shell

    #if defined(XP_UNIX) && !defined(XP_MACOSX)

Troubleshooting
---------------

-  **If configure (or generally building with clang) fails with
   ``fatal error: 'stdio.h' file not found``:** Make sure the Xcode
   command line tools are installed by running.
   ``xcode-select --install``. [jgilbert] found this necessary during an
   install for 10.9.
-  **For inexplicable errors in the configure phase:** Review all
   modifications of your PATH in .bash\_profile, .bash\_rc or whatever
   configuration file you're using for your chosen shell. Removing all
   modifications and then re-adding them one-by-one can narrow down
   problems.

