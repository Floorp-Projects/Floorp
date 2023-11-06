Building Firefox On macOS
=========================

This document will help you get set up to build Firefox on your own
computer. Getting set up can take a while - we need to download a
lot of bytes! Even on a fast connection, this can take ten to fifteen
minutes of work, spread out over an hour or two.

Requirements
------------

-  **Memory:** 4GB RAM minimum, 8GB+ recommended.
-  **Disk Space:** At least 30GB of free disk space.
-  **Operating System:** macOS - most recent or prior release. It is advisable
   to upgrade to the latest “point” release.  See :ref:`build_hosts` for more
   information.


1. System preparation
---------------------

1.1. Install Brew
~~~~~~~~~~~~~~~~~

Mozilla's source tree requires a number of third-party tools.
You will need to install `Homebrew <https://brew.sh/>`__ so that we
can automatically fetch the tools we need.

1.2. Install Xcode
~~~~~~~~~~~~~~~~~~

Install Xcode from the App Store.
Once done, finalize the installation in your terminal:

.. code-block:: shell

    sudo xcode-select --switch /Applications/Xcode.app
    sudo xcodebuild -license

1.3 Install Mercurial
~~~~~~~~~~~~~~~~~~~~~

Mozilla's source code is hosted in Mercurial repositories. You will
need Mercurial to download and update the code. Additionally, we'll
put user-wide python package installations on the ``$PATH``, so that
both ``hg`` and ``moz-phab`` will be easily accessible:

.. code-block:: shell

    echo 'export PATH="'"$(python3 -m site --user-base)"'/bin:$PATH"' >> ~/.zshenv
    python3 -m pip install --user mercurial

Now, restart your shell so that the ``PATH`` change took effect.
You can test that Mercurial is installed by running:

.. code-block:: shell

    hg version

.. note::

    If you're using a shell other than ``zsh``, you'll need to manually add Python's
    ``bin`` directory to your ``PATH``, as your shell probably won't pick up our
    changes in ``~/.zshenv``.

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
