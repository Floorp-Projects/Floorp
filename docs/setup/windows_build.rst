Building Firefox On Windows
======================================

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
-  **Operating System:** Windows 10. It is advisable to have Windows Update be fully
   up-to-date. See :ref:`build_hosts` for more information.

1. Install MozillaBuild
-----------------------

Install `MozillaBuild
<https://ftp.mozilla.org/pub/mozilla/libraries/win32/MozillaBuildSetup-Latest.exe>`_.

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

    # Using the C:\mozilla-build\start-shell.bat shell from step 1:
    cd c:/
    mkdir mozilla-source
    cd mozilla-source
    wget https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py
    python3 bootstrap.py
.. note::

    When running ``bootstrap.py`` there will be a `UAC (User Account Control) prompt <https://docs.microsoft.com/en-us/windows/security/identity-protection/user-account-control/how-user-account-control-works>`_ for PowerShell after
    selecting the destination directory for the source code clone. This is
    necessary to add the Microsoft Defender Antivirus exclusions automatically. You
    should select ``Yes`` on the UAC prompt, otherwise you will need
    to :ref:`follow some manual steps below <Ensure antivirus exclusions>`.

.. note::

    To use ``git``, you can grab the source code in "git" form by running the
    bootstrap script with the ``vcs`` parameter:

    .. code-block:: shell

        python3 bootstrap.py --vcs=git

    This uses `Git Cinnabar <https://github.com/glandium/git-cinnabar/>`_ under the hood.

Choosing a build type
~~~~~~~~~~~~~~~~~~~~~

If you aren't modifying the Firefox backend, then select one of the
:ref:`Artifact Mode <Understanding Artifact Builds>` options. If you are
building Firefox for Android, you should also see the :ref:`GeckoView Contributor Guide <geckoview-contributor-guide>`.

.. _Ensure antivirus exclusions:

Ensure antivirus exclusions
~~~~~~~~~~~~~~~~~~~~~~~~~~~

Microsoft Defender Antivirus and some third-party antivirus products
are known to significantly degrade build times and sometimes even cause failed
builds (due to a "missing file"). This is usually because we have tests for
well-known security bugs that have code samples that antivirus software identifies
as a threat, automatically quarantining/corrupting the files.

To avoid this, add the following folders to your third-party antivirus exclusion list:

-  The ``C:\mozilla-build`` folder.
-  The directory where the Firefox code is (probably ``C:\mozilla-source``).
-  The ``%USERPROFILE%/.mozbuild`` directory (probably ``C:\Users\<user>\.mozbuild``).

The ``bootstrap.py`` script attempts to add the above folders to the Microsoft
Defender Antivirus exclusion list automatically. You should check that they were
successfully added, but if they're missing you will need to `add the exclusions to
Microsoft Defender Antivirus manually
<https://support.microsoft.com/en-ca/help/4028485/windows-10-add-an-exclusion-to-windows-security>`_.

.. note::

    If you are using Mercurial and you're already missing files (you'll see them listed in ``hg status``), you can have them
    brought back by reverting your source tree: ``hg update -C``.

    If you are using Git and you're already missing files (you'll see them listed in ``git status``), you can have them brought back by discarding changes in your source tree: ``git restore .``.

1. Build
--------

Now that your system is bootstrapped, you should be able to build!

.. code-block:: shell

    cd c:/mozilla-source/mozilla-unified
    hg up -C central
    ./mach build

🎉 Congratulations! You've built your own home-grown Firefox!
You should see the following message in your terminal after a successful build:

.. code-block:: console

    Your build was successful!
    To take your build for a test drive, run: |mach run|
    For more information on what to do now, see https://firefox-source-docs.mozilla.org/setup/contributing_code.html

You can now use the ``./mach run`` command to run your locally built Firefox!

If your build fails, please reference the steps in the `Troubleshooting section <#troubleshooting>`_.

Now the fun starts
------------------

Time to start hacking! You should join us on `Matrix <https://chat.mozilla.org/>`_,
say hello in the `Introduction channel
<https://chat.mozilla.org/#/room/#introduction:mozilla.org>`_, and `find a bug to
start working on <https://codetribute.mozilla.org/>`_.
See the :ref:`Firefox Contributors' Quick Reference` to learn how to test your changes,
send patches to Mozilla, update your source code locally, and more.

.. note::

    If you'd like to interact with Mach from a different command line environment
    than MozillaBuild, there's experimental support for it described
    :ref:`over here <Using Mach on Windows Outside MozillaBuild>`.

Troubleshooting
---------------

Build errors
~~~~~~~~~~~~

If you encounter a build error when trying to setup your development environment, please follow these steps:
   1. Copy the entire build error to your clipboard
   2. Paste this error on `paste.mozilla.org <https://paste.mozilla.org>`_ in the text area and change the "Expire in one hour" option to "Expire in one week". Note: it won't take a week to get help but it's better to have the snippet be around for a bit longer than expected.
   3. Go to the `introduction channel <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ and ask for help with your build error. Make sure to post the link to the paste.mozilla.org snippet you created!

MozillaBuild out-of-date
~~~~~~~~~~~~~~~~~~~~~~~~

The build system expects that you're using the most-recent MozillaBuild release.
However, MozillaBuild doesn't auto-update. If you're running into local issues,
they may be resolved by `upgrading your MozillaBuild <https://wiki.mozilla.org/MozillaBuild>`_.

Spaces in folder names
~~~~~~~~~~~~~~~~~~~~~~

**Firefox will not build** if the path to MozillaBuild or the Firefox source
contain **spaces** or other breaking characters such as pluses, quotation marks,
or metacharacters.  The Visual Studio tools and SDKs are an exception - they may
be installed in a directory which contains spaces. It is strongly recommended
that you accept the default settings for all installation locations.

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
