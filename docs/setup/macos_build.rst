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
-  **Operating System:** macOS 10.12 or later. It is advisable to
   upgrade to the latest “point” release.


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

    echo "export PATH=\"$(python3 -m site --user-base)/bin:$PATH\"" >> ~/.zshenv
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

3. Build
--------

Now that your system is bootstrapped, you should be able to build!

.. code-block:: shell

    rm bootstrap.py
    cd mozilla-unified
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

macOS SDK is unsupported
~~~~~~~~~~~~~~~~~~~~~~~~

.. only:: comment

    If you are editing this section to bump the SDK and Xcode version, I'd recommend
    following the steps to ensure that they're not obsolete. Apple doesn't guarantee
    the structure of Xcode, so the SDK could be moved to a different location or
    stored differently.

If the SDK included with your Xcode installation is not supported by Firefox,
you'll need to manually install one that is compatible.
We're currently using the 10.12 SDK on our build servers, so that's the one that you
should install:

1. Go to the `More Downloads for Apple Developers <https://developer.apple.com/download/more/>`_ page
   and download Xcode 8.2.
2. Once downloaded, extract ``Xcode_8.2.xip``.
3. In your terminal, copy the SDK from the installer:

.. code-block:: shell

    mkdir -p ~/.mozbuild/macos-sdk
    # This assumes that Xcode is in your "Downloads" folder
    cp -aH ~/Downloads/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX10.12.sdk ~/.mozbuild/macos-sdk/

4. Finally, inform the Firefox build about this SDK by creating (or editing) a file called ``mozconfig`` file
   in the Firefox source code directory. Add the following line:

.. code-block::

    ac_add_options --with-macos-sdk=$HOME/.mozbuild/macos-sdk/MacOSX10.12.sdk

5. Now, you should be able to successfully run ``./mach build``.
