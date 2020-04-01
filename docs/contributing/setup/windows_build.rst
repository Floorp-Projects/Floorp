Building Firefox On Windows
===========================

Thank you for helping us build the world's best browser on the world's
most popular OS. This document will help you get set up to build and
hack on your own version of Firefox on your local machine.

Getting set up won't be difficult, but it can take a while - we need to
download a lot of bytes! Even on a fast connection, this can take ten to
fifteen minutes of work, spread out over an hour or two.

The details are further down this page, but this quick start guide
should get you up and running:

Getting ready
-------------

To build Firefox on Windows, you need a 64-bit version of Windows 7 or
later and about 40 GB of free space on your hard drive. You can make
sure your version of Windows is 64-bit on Windows 7 by right-clicking on
“Computer” in your “Start” menu, clicking “Properties” and then
“System”. On Windows 8.1 and Windows 10, right-clicking the “Windows”
menu button and choosing “System” will show you the same information.
You can alternatively press the “Windows” and “Pause Break” buttons
simultaneously on your keyboard on any version of Windows.

Next, we want to start on a solid foundation: make sure you’re up to
date with Windows Update and then we’ll get moving.

Visual Studio 2019
~~~~~~~~~~~~~~~~~~

As of `bug
1483835 <https://bugzilla.mozilla.org/show_bug.cgi?id=1483835>`, local
Windows builds `use clang-cl by
default <https://groups.google.com/d/topic/mozilla.dev.platform/MdbLAcvHC0Y/discussion>`
as compiler. Visual Studio is still necessary for build tools, headers,
and SDK.

Automation builds still use Visual Studio 2017, so there may be some
divergence until we upgrade. `Bug
1581930 <https://bugzilla.mozilla.org/show_bug.cgi?id=1581930>` tracks
various issues building with 2019. Please file your issue there and
downgrade in the interim if you encounter build failures.

`Download and install the Community
edition <https://visualstudio.microsoft.com/downloads/>` of Visual
Studio 2019. Professional and Enterprise are also supported if you have
either of those editions.

When installing, the following workloads must be checked:

-  "Desktop development with C++" (under the Windows group)
-  "Game development with C++" (under the Mobile & Gaming group)

In addition, go to the Individual Components tab and make sure the
following components are selected under the "SDKs, libraries, and
frameworks" group:

-  "Windows 10 SDK" (at least version **10.0.17134.0**)
-  "C++ ATL for v142 build tools (x86 and x64)" (also select ARM64 if
   you'll be building for ARM64)

Make sure you run Visual Studio once after installing, so it finishes
any first-run tasks and associates the installation with your account.

Other Required Tools
~~~~~~~~~~~~~~~~~~~~

MozillaBuild
^^^^^^^^^^^^

Finally, download the `MozillaBuild
Package <https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`
from Mozilla. Accept the default settings, in particular the default
installation directory: ``c:\mozilla-build\``. On some versions of
Windows an error dialog will give you the option to ‘reinstall with the
correct settings’ - you should agree and proceed.

Once this is done, creating a shortcut to
``c:\mozilla-build\start-shell.bat`` on your desktop will make your life
easier.

Getting the source
~~~~~~~~~~~~~~~~~~

This is the last big step. Double-clicking **start-shell.bat** in
``c:\mozilla-build`` (or the shortcut you’ve created above) will open a
terminal window.

Start by creating a "mozilla-source" directory off of ``C:\`` and cd
into it like so

.. code-block:: shell

    cd c:/

    mkdir mozilla-source

    cd mozilla-source

Next, you can get the Firefox source code with Mercurial. There is a
wiki page on how to `get the source using
Mercurial <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Source_Code/Mercurial>`
but, in summary, if your network connection is good enough to download
1+ GB without interuption and you want the main development repository,
then you can just use:

.. code-block:: shell

    hg clone https://hg.mozilla.org/mozilla-central

While you’re waiting for that process to finish, take a look at `our
Mercurial
documentation <http://mozilla-version-control-tools.readthedocs.org/en/latest/hgmozilla/index.html>`.
It explains how we use version control at Mozilla to manage our code and
land changes to our source tree.

Build Firefox!
~~~~~~~~~~~~~~

Now we tie it all together. In your terminal window, ``cd`` to your
source directory as before and type

.. code-block:: shell

    cd mozilla-central

    ./mach bootstrap

    ./mach build

The ``./mach bootstrap`` step is a catch-all for any dependencies not
covered in this documentation. Note that, bootstrap works **only with
the Mercuial repo of the source**, not with source tar balls, nor the
github mirror. If you are working on Firefox or Firefox for Android
frontends or building Firefox without any changes, select `artifact
builds <https://developer.mozilla.org/en-US/docs/Artifact_builds>` in
the first question in ``./mach bootstrap``.  Artifact builds will
complete more quickly!  Artifact builds are unsuitable for those working
on C++ code.

You’re on your way. Don’t be discouraged if this takes a while; it takes
some time even on the fastest modern machines and as much as two hours
or more on older hardware. Firefox is pretty big, because the Web is
big.

Getting connected
~~~~~~~~~~~~~~~~~

That last step can take some time. While it’s finishing you should take
a moment to sign up for a Bugzilla account!

`Bugzilla.mozilla.org <https://bugzilla.mozilla.org/>` is Mozilla’s
issue tracker. To comment on a bug or submit a patch you’ll need a
Bugzilla account; you can use your GitHub account if you have one, or
`sign up for a Bugzilla account
directly. <https://bugzilla.mozilla.org/createaccount.cgi>`

As well as Bugzilla, much of Mozilla’s internal communication happens
over Internet Relay Chat (IRC). You can learn how to `connect to Mozilla
with IRC here <https://wiki.mozilla.org/IRC>`. If you’re just getting
started or have questions about getting set up you can join us in the
*"#introduction channel"*, where some of our community members hang out
to try and help new contributors get rolling.

You're ready
~~~~~~~~~~~~

When mach build completes, you'll have your own version of Firefox built
from the source code on your hard drive, ready to run. You can run it
with

.. code-block:: shell

    ./mach run

Now you have your own home-built version of Firefox.

If you saw an error here, look further down in this document for the
"Troubleshooting" section - some antivirus software quarantine some of
our tests, so you need to create exceptions for the "mozilla-source" and
"mozilla-build" directories. Don't turn your antivirus off! Just add the
exceptions.

Now the fun starts
~~~~~~~~~~~~~~~~~~

You have the code and you’ve compiled Firefox. Just fire it up with
``./mach run`` and you’re ready to start hacking. The next steps are up
to you: join us on IRC in the “\ *#introduction* *channel*\ ”, follow
`StartMozilla on Twitter <https://twitter.com/StartMozilla>` and find
`a bug to start working
on <http://www.joshmatthews.net/bugsahoy/?simple=1>`.

Thank you for joining us and helping us make Firefox and the open Web
better for everyone.


Details and troubleshooting
---------------------------

Hardware and software requirements
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Firefox build process is both I/O and CPU-intensive, and can take a
long time to build even on modern hardware. The minimum and recommended
hardware requirements for Mozilla development are:

-  At least 4 GB of RAM. 8 GB or more is recommended, and more is always
   better.
-  35 GB free disk space. This amount of disk space accommodates Visual
   Studio 2019 Community Edition, the required SDKs, the MozillaBuild
   package, the Mercurial source repository and enough free disk space
   to compile. A solid-state hard disk is recommended as the Firefox
   build process is I/O-intensive.
-  A 64-bit version of Windows 7 (Service Pack 1) or later. You can
   still build 32-bit Firefox on a 64-bit Windows installation.

Overview
~~~~~~~~

The Mozilla build process requires many tools that are not pre-installed
on most Windows systems. In addition to Visual Studio, install
MozillaBuild - a software bundle that includes the required versions of
bash, GNU make, autoconf, Mercurial, and much more.

Firefox 61+ require Visual Studio 2017 Update 6 or newer to build.

Firefox 48 to 60 build with Visual Studio 2015. Visual Studio 2017 also
works for building Firefox 58 or newer.

Firefox 37 through to 47 build with Visual Studio 2013 (VC12) and
possibly Visual Studio 2015 (although Visual Studio 2015 may not build
every revision).

Earlier versions of Firefox build with older versions of Visual Studio.

Installing the build prerequisites
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Complete each of these steps otherwise, you may not be able to build
successfully. There are notes on these software requirements below.

#. Make sure your system is up-to-date through Windows Update.
#. Install `Visual Studio Community
   2019 <https://www.visualstudio.com/downloads/>` (free).
   Alternatively, you can also use a paid version of Visual Studio. Some
   additional components may be required to build Firefox, as noted in
   the "Visual Studio 2019" section above. Earlier versions of Visual
   Studio are not supported; the Firefox codebase relies on C++ features
   that are not supported in earlier releases.
#. Optionally, in addition to VS2019, you may want to install `Visual
   C++ 2008 Express <http://go.microsoft.com/?linkid=7729279>` (free)
   to compile some Python extensions used in the build system as Python
   2.7.x for Windows is built with that compiler by default. Note, if
   you want to use "mach resource-usage", "mach doctor", "mach
   android-emulator", or run talos tests locally, you should install it
   for building psutil.
#. Download and install the
   `MozillaBuild <https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`
   package, containing additional build tools. If you have Cygwin
   installed, read the note in the tips section. If you see a Windows
   error dialog giving you the option to re-install with the 'correct
   settings', after the MozillaBuild's installer exits, choose the
   option and after that all should be well. More information about
   MozillaBuild and links to newer versions are available at
   https://wiki.mozilla.org/MozillaBuild.

Troubleshooting
~~~~~~~~~~~~~~~

In some circumstances, the following problems can arise:

**Some antivirus and system protection software can dramatically slow or
break the build process**

-  Windows Defender and some scanning antivirus products are known to
   have a major impact on build times. For example, if you have cloned
   ``mozilla-central`` successfully but ``./mach build`` fails reporting
   a missing file, you are likely experiencing this problem. Our
   regression tests, for well-known security bugs, can include code
   samples that some antivirus software will identify as a threat, and
   will either quarantine or otherwise corrupt the files involved. To
   resolve this you will need to whitelist your source and object
   directories (the ``mozilla-source`` and ``mozilla-build``
   directories) in Windows Defender or your antivirus software and if
   you're missing files, revert your source tree with the
   "``hg update -C" ``\ command. Once this is done your next
   ``./mach build`` should complete successfully.

**Installing Visual Studio in a different language than the system can
cause issues**

-  For example, having Visual Studio in French when the system is in
   English causes the build to spew a lot of include errors and finishes
   with a link error.

.. note::

**Note:** **Mozilla will not build** if the path to the installation
tool folders contains **spaces** or other breaking characters such as
pluses, quotation marks, or metacharacters.  The Visual Studio tools and
SDKs are an exception - they may be installed in a directory which
contains spaces. It is strongly recommended that you accept the default
settings for all installation locations.

MozillaBuild
~~~~~~~~~~~~

The MozillaBuild package contains other software prerequisites necessary
for building Mozilla, including the MSYS build environment,
`Mercurial <https://www.mercurial-scm.org/>`, autoconf-2.13, CVS,
Python, YASM, NSIS, and UPX, as well as optional but useful tools such
as wget and emacs.

`Download the current MozillaBuild
package. <https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`

By default, the package installs to ``c:\mozilla-build`` and it is
recommended to use the default path. Don't use a path that contains
spaces. The installer does not modify the Windows registry. Note that
some binaries may require `Visual C++ Redistributable
package <https://www.microsoft.com/downloads/en/details.aspx?FamilyID=a5c84275-3b97-4ab7-a40d-3802b2af5fc2&displaylang=en>` to
run.

**MozillaBuild command prompt expectation setting:** Note that the
"UNIX-like" environment provided by MozillaBuild is only really useful
for building and committing to the Mozilla source. Most command line
tools you would expect in a modern Linux distribution are not present,
and those tools that are provided can be as much as a decade or so old
(especially those provided by MSYS). It's the old tools in particular
that can cause problems since they often don't behave as expected, are
buggy, or don't support command line arguments that have been taken for
granted for years. For example, copying a source tree using
``cp -rf src1 src2`` does not work correctly because of an old version
of cp (it gives "cp: will not create hard link" errors for some files).
In short, MozillaBuild supports essential developer interactions with
the Mozilla code, but beyond that don't be surprised if it trips you up
in all sorts of exciting and unexpected ways.

Opening a MozillaBuild command prompt
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

After the prerequisites are installed, launch
the \ **``start-shell.bat``** batch file using the Windows command
prompt in the directory to which you installed MozillaBuild
(``c:\mozilla-build`` by default). This will launch an MSYS/BASH command
prompt properly configured to build Firefox. All further commands should
be executed in this command prompt window. (Note that this is not the
same as what you get with the Windows CMD.EXE shell.)

.. note::

Note: This is not the same as what you get with the Windows CMD.EXE
shell.

Create a directory for the source
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Note:** You won't be able to build the Firefox source code if it's
under a directory with spaces in the path such as "Documents and
Settings". You can pick any other location, such as a new directory
c:/mozilla-source or c:/thunderbird-src. The build command prompt also
tolerates "c:\\" and "/c/", but the former gives confusion in the
Windows command prompt, and the latter is misinterpreted by some tools
(at least MOZ\_OBJDIR). The "C:/" syntax helps draw attention that the
**MozillaBuild** command prompt is assumed from here on out since it
provides configured environment and tools.


It's a sensible idea to create a new shallow directory, like
"c:/mozilla-source" dedicated solely to the
code:

.. code-block:: shell

    cd c:/; mkdir mozilla-source; cd mozilla-source

Keeping in mind the diagnostic hints below should you have issues. You
are now ready to get the Firefox source and build.

Command prompt tips and caveats
-------------------------------

-  To paste into this window, you must right-click on the window's title
   bar, move your cursor to the “Edit” menu, and click “Paste”. You can
   also set “Quick Edit Mode” in the “Properties” menu and right-click
   the window to paste your selection.
-  If you have Cygwin installed, make sure that the MozillaBuild
   directories come before any Cygwin directories in the search path
   enhanced by \ ``start-shell-msvc2015.bat`` (use ``echo $PATH`` to see
   your search path).
-  In the MSYS / BASH shell started by ``start-shell-msvc2015.bat``,
   UNIX-style forward slashes (/) are used as path separators instead of
   the Windows-style backward slashes (\\).  So if you want to change to
   the directory ``c:\mydir``, in the MSYS shell to improve clarity, you
   would use ``cd /c/mydir ``\ though both ``c:\mydir`` and ``c:/mydir``
   are supported.
-  The MSYS root directory is located in ``/c/mozilla-build/msys`` if
   you used the default installation directory. It's a good idea not to
   build anything under this directory. Instead use something like
   ``/c/mydir``.

Common problems, hints, and restrictions
----------------------------------------

-  `Debugging Firefox on Windows
   FAQ <https://developer.mozilla.org/en-US/docs/Mozilla/Debugging/Debugging_Mozilla_on_Windows_FAQ>`:
   Tips on how to debug Mozilla on Windows.
-  Your installed MozillaBuild may be too old. The build system may
   assume you have new features and bugfixes that are only present in
   newer versions of MozillaBuild. Instructions for how to update
   MozillaBuild `can be found
   here <https://wiki.mozilla.org/MozillaBuild>`.
-  The build may fail if your machine is configured with the wrong
   architecture. If you want to build 64-bit Firefox, add the two lines
   below to your mozconfig file:

.. code-block:: shell

       ac_add_options --target=x86_64-pc-mingw32
       ac_add_options --host=x86_64-pc-mingw32

-  The build may fail if your ``PATH`` environment variable contains
   quotation marks("). Quotes are not properly translated when passed
   down to MozillaBuild sub-shells and they are usually not needed so
   they can be removed.
-  The build may fail if you have a ``PYTHON`` environment variable set.
   It displays an error almost immediately that says
   "``The system cannot find the file specified``." Typing
   "``unset PYTHON``" before running the Mozilla build tools in the same
   command shell should fix this. Make sure that ``PYTHON`` is unset,
   rather than set to an empty value.
-  The build may fail if you have Cygwin installed. Make sure that the
   MozillaBuild directories (``/c/mozilla-build`` and subdirectories)
   come before any Cygwin directories in your PATH environment variable.
   If this does not help, remove the Cygwin directories from PATH, or
   try building on a clean PC with no Cygwin.
-  Building with versions of NSIS other than the version that comes with
   the latest supported version of MozillaBuild is not supported and
   will likely fail.
-  If you intend to distribute your build to others, set
   ``WIN32_REDIST_DIR=$VCINSTALLDIR\redist\x86\Microsoft.VC141.CRT`` in
   your mozconfig to get the Microsoft CRT DLLs packaged along with the
   application. Note the exact .CRT file may depend on your Visual
   Studio version.
-  The Microsoft Antimalware service can interfere with compilation,
   often manifesting as an error related to ``conftest.exe`` during
   build. To remedy this, add at your object directory at least to the
   exclusion settings.
-  Errors like "second C linkage of overloaded function
   '\_interlockedbittestandset' not allowed", are encountered when
   intrin.h and windows.h are included together. Use a\ *#define* to
   redefine one instance of the function's name.
-  Parallel builds (``-jN``) do not work with GNU makes on Windows. You
   should use the ``mozmake`` command included with current versions of
   MozillaBuild. Building with the ``mach`` command will always use the
   best available make command.
-  If you encounter a build failure like "ERROR: Cannot find
   makecab.exe", try applying the patch from `bug
   1383578 <https://bugzilla.mozilla.org/show_bug.cgi?id=1383578>`,
   i.e. change: ``SET PATH="%PATH%;!LLVMDIR!\bin"``  to
   ``SET "PATH=%PATH%;!LLVMDIR!\bin"``.
-  If you encounter a build failure with
   ``LINK: fatal error LNK1181: cannot open input file ..\..\..\..\..\security\nss3.lib``,
   it may be related to your clone of ``mozilla-central`` being located
   in the Users folder (possibly encrypted). Try moving it outside of
   the Users folder. The docs recommend
   ``C:\mozilla-source\mozilla-central`` which should work.
-  If you encounter a build failure with
   ``ERROR: GetShortPathName returned a long path name.``.You need
   create a 8dot3name short name for the path which has space.For
   example : fsutil file setshortname "C:\\Program Files (x86)"
   PROGRA~2.  If you got "access denied", try to restart your computer
   to safe mode and try again.

