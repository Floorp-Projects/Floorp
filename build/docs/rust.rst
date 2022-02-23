.. _rust:

==============================
Including Rust Code in Firefox
==============================

This page explains how to add, build, link, and vendor Rust crates.

The `code documentation <../../writing-rust-code>`_ explains how to write and
work with Rust code in Firefox. The
`test documentation <../../testing-rust-code>`_ explains how to test and debug
Rust code in Firefox.

Linking Rust crates into libxul
===============================

Rust crates that you want to link into libxul should be listed in the
``dependencies`` section of
`toolkit/library/rust/shared/Cargo.toml <https://searchfox.org/mozilla-central/source/toolkit/library/rust/shared/Cargo.toml>`_.
You must also add an ``extern crate`` reference to
`toolkit/library/rust/shared/lib.rs <https://searchfox.org/mozilla-central/source/toolkit/library/rust/shared/lib.rs>`_.
This ensures that the Rust code will be linked properly into libxul as well
as the copy of libxul used for gtests. (Even though Rust 2018 mostly doesn't
require ``extern crate`` declarations, these ones are necessary because the
gkrust setup is non-typical.)

After adding your crate, execute ``cargo update -p gkrust-shared`` to update
the ``Cargo.lock`` file. You will also need to do this any time you change the
dependencies in a ``Cargo.toml`` file. If you don't, you will get a build error
saying **"error: the lock file /home/njn/moz/mc3/Cargo.lock needs to be updated
but --frozen was passed to prevent this"**.

By default, all Cargo packages in the mozilla-central repository are part of
the same
`workspace <https://searchfox.org/mozilla-central/source/toolkit/library/rust/shared/lib.rs>`_
and will share the ``Cargo.lock`` file and ``target`` directory in the root of
the repository.  You can change this behavior by adding a path to the
``exclude`` list in the top-level ``Cargo.toml`` file.  You may want to do
this if your package's development workflow includes dev-dependencies that
aren't needed by general Firefox developers or test infrastructure.

The actual build mechanism is as follows. The build system generates a special
'Rust unified library' crate, compiles that to a static library
(``libgkrust.a``), and links that into libxul, so all public symbols will be
available to C++ code. Building a static library that is linked into a dynamic
library is easier than building dynamic libraries directly, and it also avoids
some subtle issues around how mozalloc works that make the Rust dynamic library
path a little wonky.

Linking Rust crates into something else
=======================================

To link Rust code into libraries other than libxul, create a directory with a
``Cargo.toml`` file for your crate, and a ``moz.build`` file that contains:

.. code-block:: python

    RustLibrary('crate_name')

where ``crate_name`` matches the name from the ``[package]`` section of your
``Cargo.toml``. You can refer to `the moz.build file <https://searchfox.org/mozilla-central/rev/3f4c3a3cabaf94958834d3a8935adfb4a887942d/toolkit/library/rust/moz.build#7>`_ and `the Cargo.toml file <https://searchfox.org/mozilla-central/rev/3f4c3a3cabaf94958834d3a8935adfb4a887942d/toolkit/library/rust/Cargo.toml>`_ that are used for libxul.

You can then add ``USE_LIBS += ['crate_name']`` to the ``moz.build`` file
that defines the binary as you would with any other library in the tree.

.. important::

    You cannot link a Rust crate into an intermediate library that will be
    eventually linked into libxul. The build system enforces that only a single
    ``RustLibrary`` may be linked into a binary. If you need to do this, you
    will have to add a ``RustLibrary`` to link to any standalone binaries that
    link the intermediate library, and also add the Rust crate to the libxul
    dependencies as in `linking Rust Crates into libxul`_.

Conditional compilation
========================

Edit `tool/library/rust/gkrust-features.mozbuild
<https://searchfox.org/mozilla-central/source/toolkit/library/rust/gkrust-features.mozbuild>`_
to expose build flags as Cargo features.

Standalone Rust programs
========================

It is also possible to build standalone Rust programs. First, put the Rust
program (including the ``Cargo.toml`` file and the ``src`` directory) in its
own directory, and add an empty ``moz.build`` file to the same directory.

Then, if the standalone Rust program must run on the compile target (e.g.
because it's shipped with Firefox) then add this rule to the ``moz.build``
file:

.. code-block:: python

    RUST_PROGRAMS = ['prog_name']

where *prog_name* is the name of the executable as specified in the
``Cargo.toml`` (and probably also matches the name of the directory).

Otherwise, if the standalone Rust program must run on the compile host (e.g.
because it's used to build Firefox but not shipped with Firefox) then do the
same thing, but use ``HOST_RUST_PROGRAMS`` instead of ``RUST_PROGRAMS``.

Where should I put my crate?
============================

If your crate's canonical home is mozilla-central, you can put it next to the
related code in the appropriate directory.

If your crate is mirrored into mozilla-central from another repository, and
will not be actively developed in mozilla-central, you can simply list it
as a ``crates.io``-style dependency with a version number, and let it be
vendored into the ``third_party/rust`` directory.

If your crate is mirrored into mozilla-central from another repository, but
will be actively developed in both locations, you should send mail to the
dev-builds mailing list to start a discussion on how to meet your needs.

Third-party crate dependencies
==============================

Third-party dependencies for in-tree Rust crates are *vendored* into the
``third_party/rust`` directory of mozilla-central. This means that a copy of
each third-party crate's code is committed into mozilla-central. As a result,
building Firefox does not involve downloading any third-party crates. 

If you add a dependency on a new crate you must run ``mach vendor rust`` to
vendor the dependencies into that directory. (Note that ``mach vendor rust``
`may not work as well on Windows <https://bugzilla.mozilla.org/show_bug.cgi?id=1647582>`_
as on other platforms.)

When it comes to checking the suitability of third-party code for inclusion
into mozilla-central, keep the following in mind.

- ``mach vendor rust`` will check that the licenses of all crates are suitable.
- You should review the crate code to some degree to check that it looks
  reasonable (especially for unsafe code) and that it has reasonable tests.
- Third-party crate tests aren't run, which means that large test fixtures will
  bloat mozilla-central. Consider working with upstream to mark those test
  fixtures with ``[package] exclude = ...`` as described
  `here <https://doc.rust-lang.org/cargo/reference/manifest.html#the-exclude-and-include-fields>`_.
- If you specify a dependency on a branch, pin it to a specific revision,
  otherwise other people will get unexpected changes when they run ``./mach
  vendor rust`` any time the branch gets updated. See `bug 1612619
  <https://bugzil.la/1612619>`_ for a case where such a problem was fixed.
- Other than that, there is no formal sign-off procedure, but one may be added
  in the future.

Note that all dependencies will be vendored, even ones that aren't used due to
disabled features. It's possible that multiple versions of a crate will end up
vendored into mozilla-central.

Patching third-party crates
===========================

Sometimes you might want to temporarily patch a third-party crate, for local
builds or for a try push.

To do this, first add an entry to the ``[patch.crates-io]`` section of the
top-level ``Cargo.toml`` that points to the crate within ``third_party``. For
example

.. code-block:: toml

    bitflags = { path = "third_party/rust/bitflags" }

Next, run ``cargo update -p $CRATE_NAME --precise $VERSION``, where
``$CRATE_NAME`` is the name of the patched crate, and ``$VERSION`` is its
version number. This will update the ``Cargo.lock`` file.

Then, make the local changes to the crate.

Finally, make sure you don't accidentally land the changes to the crate or the
``Cargo.lock`` file.

For an example of a more complex workflow involving a third-party crate, see
`mp4parse-rust/README.md <https://searchfox.org/mozilla-central/source/media/mp4parse-rust/README.md>`_.
It describes the workflow for a crate that is hosted on GitHub, and for which
changes are made via GitHub pull requests, but all pull requests must also be
tested within mozilla-central before being merged.
