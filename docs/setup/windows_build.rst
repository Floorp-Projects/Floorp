Building Firefox On Windows
===========================

This document will help you get set up to build Firefox on your own
computer. Getting set up can take a while - we need to download a
lot of bytes! Even on a fast connection, this can take ten to fifteen
minutes of work, spread out over an hour or two.

If you'd prefer to build Firefox for Windows in a virtual machine,
you may be interested in the `Windows images provided by Microsoft
<https://developer.microsoft.com/en-us/windows/downloads/virtual-machines/>`_.

Requirements
------------

-  **Memory:** 4GB RAM minimum, 8GB+ recommended.
-  **Disk Space:** At least 40GB of free disk space.
-  **Operating System:** Windows 7 or later. It is advisable to have
   Windows Update be fully up-to-date.

1. System preparation
---------------------

1.1 Install Visual Studio
~~~~~~~~~~~~~~~~~~~~~~~~~

`Download and install the Community edition
<https://visualstudio.microsoft.com/downloads/>`_ of Visual
Studio 2019. If you have the Professional or Enterprise edition, that
is also supported.

Ensure you've checked the following items for installation:

-  In the Workloads tab:
    -  Desktop development with C++.
    -  Game development with C++.
-  In the Individual components tab:
    -  Windows 10 SDK (at least **10.0.17134.0**).
    -  C++ ATL for v142 build tools (x86 and x64).

1.2 Install MozillaBuild
~~~~~~~~~~~~~~~~~~~~~~~~

Install `MozillaBuild
<https://ftp.mozilla.org/pub/mozilla.org/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`_.

Accept the default installation directory.
Windows may prompt you to "reinstall with the correct settings", which you
should click to accept.

When working with Firefox tooling, you'll need to do so from within the MozillaBuild
shell. You can start it by running ``C:\mozilla-build\start-shell.bat`` (you may want
to make a shortcut to this file so it's easier to start).

.. note::

    The MozillaBuild shell is a lot more like a Linux shell than the Windows ``cmd``. You can
    learn more about it `here <https://wiki.mozilla.org/MozillaBuild>`_.

2. Bootstrap a copy of the Firefox source code
----------------------------------------------

Now that your system is ready, we can download the source code and have Firefox
automatically download the other dependencies it needs. The below command
will download a lot of data (years of Firefox history!) then guide you through
the interactive setup process.

.. code-block:: shell

    cd c:/
    mkdir mozilla-source
    cd mozilla-source
    curl https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O
    python3 bootstrap.py

.. note::

    In general, the Firefox workflow works best with Mercurial. However,
    if you'd prefer to use ``git``, you can grab the source code in
    "git" form by running the bootstrap script with the ``vcs`` parameter:

    .. code-block:: shell

        python3 bootstrap.py --vcs=git

    This uses `Git Cinnabar <https://github.com/glandium/git-cinnabar/>`_ under the hood.

Choosing a build type
~~~~~~~~~~~~~~~~~~~~~

If you aren't modifying the Firefox backend, then then select one of the
:ref:`Artifact Mode <Understanding Artifact Builds>` options. If you are
building Firefox for Android, you should also see the :ref:`GeckoView Contributor Guide`.

Set antivirus exclusions
~~~~~~~~~~~~~~~~~~~~~~~~

Windows Defender and some scanning antivirus products are known to significantly degrade
build times and sometimes even cause failed builds (due to a "missing file").
This is usually because we have tests for well-known security bugs that have
code samples that antivirus software identifies as a threat, automatically
quarantining/corrupting the files.

To avoid this, add two folders to your antivirus exclusion list:

-  The ``C:\mozilla-build`` folder.
-  The directory where the Firefox code is (probably ``C:\mozilla-source``).

If you haven't installed an antivirus, then you will need to `add the exclusions
to Windows Defender
<https://support.microsoft.com/en-ca/help/4028485/windows-10-add-an-exclusion-to-windows-security>`_.

.. note::

    If you're already missing files (you'll see them listed in ``hg status``, you can have them
    brought back by reverting your source tree: ``hg update -C``).

3. Build
--------

Now that your system is bootstrapped, you should be able to build!

.. code-block:: shell

    rm bootstrap.py
    cd mozilla-central
    ./mach build
    ./mach run

🎉 Congratulations! You've built your own home-grown Firefox!

Now the fun starts
------------------

Time to start hacking! You should join us on `Matrix <https://chat.mozilla.org/>`_,
say hello in the `Introduction channel
<https://chat.mozilla.org/#/room/#introduction:mozilla.org>`_, and `find a bug to
start working on <https://codetribute.mozilla.org/>`_.
See the :ref:`Firefox Contributors' Quick Reference` to learn how to test your changes,
send patches to Mozilla, update your source code locally, and more.

Troubleshooting
---------------

MozillaBuild out-of-date
~~~~~~~~~~~~~~~~~~~~~~~~

The build system expects that you're using the most-recent MozillaBuild release.
However, MozillaBuild doesn't auto-update. If you're running into local issues,
they may be resolved by `upgrading your MozillaBuild <https://wiki.mozilla.org/MozillaBuild>`_.

Spaces in folder names
~~~~~~~~~~~~~~~~~~~~~~

**Firefox will not build** if the path to the installation
tool folders contains **spaces** or other breaking characters such as
pluses, quotation marks, or metacharacters.  The Visual Studio tools and
SDKs are an exception - they may be installed in a directory which
contains spaces. It is strongly recommended that you accept the default
settings for all installation locations.

Installing Visual Studio in a different language than Windows
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If Visual Studio is using a different language than the system, then your build
may fail with a link error after reporting a bunch of include errors.

Quotation marks in ``PATH``
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Quotation marks (") aren't translated properly when passed to MozillaBuild
sub-shells. Since they're not usually necessary, you should ensure they're
not in your ``PATH`` environment variable.

``PYTHON`` environment variable
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

If ``PYTHON`` is set, the build may fail with the error: "``The system
cannot find the file specified``." Ensure that you aren't having
a ``PYTHON`` environment variable set.

Cygwin interference
~~~~~~~~~~~~~~~~~~~

If you happen to have Cygwin installed, its tools may erroneously be
used when building Firefox. Ensure that MozillaBuild directories (in
``C:\mozilla-build\``) are before Cygwin directories in the ``PATH``
environment variable.

Building from within Users
~~~~~~~~~~~~~~~~~~~~~~~~~~

If you encounter a build failure with:
``LINK: fatal error LNK1181: cannot open input file ..\..\..\..\..\security\nss3.lib``
and the Firefox code is underneath the ``C:\Users`` folder, then you should try
moving the code to be underneath ``C:\\mozilla-source`` instead.
