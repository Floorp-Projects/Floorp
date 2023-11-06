Building Firefox On Linux
=========================

This document will help you get set up to build Firefox on your own
computer. Getting set up can take a while - we need to download a
lot of bytes! Even on a fast connection, this can take ten to fifteen
minutes of work, spread out over an hour or two.

Requirements
------------

-  **Memory:** 4GB RAM minimum, 8GB+ recommended.
-  **Disk Space:** At least 30GB of free disk space.
-  **Operating System:** A 64-bit installation of Linux. It is strongly advised
   that you use a supported distribution; see :ref:`build_hosts`.  We also
   recommend that your system is fully up-to-date.

.. note::

    Some Linux distros are better-supported than others. Mozilla maintains
    bootstrapping code for Ubuntu, but others are managed by the
    community (thanks!). The more esoteric the distro you're using,
    the more likely that you'll need to solve unexpected problems.


1. System preparation
---------------------

1.1 Install Python
~~~~~~~~~~~~~~~~~~

To build Firefox, it's necessary to have a Python of version 3.6 or later
installed. Python 2 is no longer required to build Firefox, although it is still
required for running some kinds of tests. Additionally, you will probably need
Python development files as well to install some pip packages.

You should be able to install Python using your system package manager:

-  For Debian-based Linux (such as Ubuntu): ``sudo apt-get install curl python3 python3-pip``
-  For Fedora Linux: ``sudo dnf install python3 python3-pip``

If you need a version of Python that your package manager doesn't have (e.g.:
the provided Python 3 is too old, or you want Python 2 but it's not available),
then you can use `pyenv <https://github.com/pyenv/pyenv>`_, assuming that your
system is supported.

1.2 Install Mercurial
~~~~~~~~~~~~~~~~~~~~~

Mozilla's source code is hosted in Mercurial repositories. You will
need Mercurial to download and update the code.

Note that if you'd prefer to use the version of Mercurial that is
packaged by your distro, you can skip this section. However, keep in
mind that distro-packaged Mercurial may be outdated, and therefore
slower and less supported.

.. code-block:: shell

    python3 -m pip install --user mercurial

You can test that Mercurial is installed by running:

.. code-block:: shell

    hg version

.. note::

    If your shell is showing ``command not found: hg``, then Python's packages aren't
    being found in the ``$PATH``. You can resolve this by doing the following and
    restarting your shell:

    .. code-block:: shell

        # If you're using zsh
        echo 'export PATH="'"$(python3 -m site --user-base)"'/bin:$PATH"' >> ~/.zshenv

        # If you're using bash
        echo 'export PATH="'"$(python3 -m site --user-base)"'/bin:$PATH"' >> ~/.bashrc

        # If you're using a different shell, follow its documentation to see
        # how to configure your PATH. Ensure that `$(python3 -m site --user-base)/bin`
        # is prepended.

2. Bootstrap a copy of the Firefox source code
----------------------------------------------

Now that your system is ready, we can download the source code and have Firefox
automatically download the other dependencies it needs. The below command
will download a lot of data (years of Firefox history!) then guide you through
the interactive setup process.

.. code-block:: shell

    curl https://hg.mozilla.org/mozilla-central/raw-file/default/python/mozboot/bin/bootstrap.py -O
    python3 bootstrap.py

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

3. Build
--------

Now that your system is bootstrapped, you should be able to build!

.. code-block:: shell

    cd mozilla-unified
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

Troubleshooting
---------------

Build errors
~~~~~~~~~~~~

If you encounter a build error when trying to setup your development environment, please follow these steps:
   1. Copy the entire build error to your clipboard
   2. Paste this error to `paste.mozilla.org <https://paste.mozilla.org>`_ in the text area and change the "Expire in one hour" option to "Expire in one week". Note: it won't take a week to get help but it's better to have the snippet be around for a bit longer than expected.
   3. Go to the `introduction channel <https://chat.mozilla.org/#/room/#introduction:mozilla.org>`__ and ask for help with your build error. Make sure to post the link to the paste.mozilla.org snippet you created!

Using a non-native file system (NTFS, network drive, etc)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In our experience building Firefox in these hybrid or otherwise complex environments
always ends in unexpected, often silent and always hard-to-diagnose failure.
Building Firefox in that environment is far more likely to reveal the flaws and
shortcomings of those systems than it is to produce a running web browser.
