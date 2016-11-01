.. _rust:

==============================
Including Rust Code in Firefox
==============================

The build system has support for building and linking Rust crates.
Rust code is built using ``cargo`` in the typical way, so it is
straightforward to take an existing Rust crate and integrate it
into Firefox.

.. important::

   Rust code is not currently enabled by default in Firefox builds.
   This should change soon (`bug 1283898 <https://bugzilla.mozilla.org/show_bug.cgi?id=1283898>`_),
   but the option to build without Rust code will likely last a little longer
   (`bug 1284816 <https://bugzilla.mozilla.org/show_bug.cgi?id=1284816>`_),
   so Rust code cannot currently be used for required components.


Linking Rust Crates into libxul
===============================

Rust crates that you want to link into libxul should be listed in the
``dependencies`` section of `toolkit/library/rust/shared/Cargo.toml <https://dxr.mozilla.org/mozilla-central/source/toolkit/library/rust/shared/Cargo.toml>`_.
This ensures that the Rust code will be linked properly into libxul as well
as the copy of libxul used for gtests.

Linking Rust Crates into something else
=======================================

There currently is not any Rust code being linked into binaries other than
libxul. If you would like to do so, you'll need to create a directory with
a ``Cargo.toml`` file for your crate, and a ``moz.build`` file that contains:

.. code-block:: python

    RustLibrary('crate_name')

Where *crate_name* matches the name from the ``[package]`` section of your
``Cargo.toml``. You can refer to `the moz.build file <https://dxr.mozilla.org/mozilla-central/rev/3f4c3a3cabaf94958834d3a8935adfb4a887942d/toolkit/library/rust/moz.build#7>`_ and `the Cargo.toml file <https://dxr.mozilla.org/mozilla-central/rev/3f4c3a3cabaf94958834d3a8935adfb4a887942d/toolkit/library/rust/Cargo.toml>`_ that are used for libxul.

You can then add ``USE_LIBS += ['crate_name']`` to the ``moz.build`` file
that defines the binary as you would with any other library in the tree.

.. important::

    You cannot link a Rust crate into an intermediate library that will wind
    up being linked into libxul. The build system enforces that only a single
    ``RustLibrary`` may be linked into a binary. If you need to do this, you
    will have to add a ``RustLibrary`` to link to any standalone binaries that
    link the intermediate library, and also add the Rust crate to the libxul
    dependencies as in `linking Rust Crates into libxul`_.

Where Should I put my Crate?
============================

If your crate's canonical home is mozilla-central, you can put it next to the
other code in the module it belongs to.

If your crate is mirrored into mozilla-central from another repository, and
will not be actively developed in mozilla-central, you can simply list it
as a ``crates.io``-style dependency with a version number, and let it be
vendored into the ``third_party/rust`` directory.

If your crate is mirrored into mozilla-central from another repository, but
will be actively developed in both locations, you should send mail to the
dev-builds mailing list to start a discussion on how to meet your needs.


Crate dependencies
==================

All dependencies for in-tree Rust crates are vendored into the
``third_party/rust`` directory. Currently if you add a dependency on a new
crate you must run ``mach vendor rust`` to vendor the dependencies into
that directory. In the future we hope to make it so that you only need to
vendor the dependencies in order to build your changes in a CI push.
