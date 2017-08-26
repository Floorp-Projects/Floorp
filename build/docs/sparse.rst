.. _build_sparse:

================
Sparse Checkouts
================

The Firefox repository is large: over 230,000 files. That many files
can put a lot of strain on machines, tools, and processes.

Some version control tools have the ability to only populate a
working directory / checkout with a subset of files in the repository.
This is called *sparse checkout*.

Various tools in the Firefox repository are configured to work
when a sparse checkout is being used.

Sparse Checkouts in Mercurial
=============================

Mercurial 4.3 introduced **experimental** support for sparse checkouts
in the official distribution (a Facebook-authored extension has
implemented the feature as a 3rd party extension for years).

To enable sparse checkout support in Mercurial, enable the ``sparse``
extension::

   [extensions]
   sparse =

The *sparseness* of the working directory is managed using
``hg debugsparse``. Run ``hg help debugsparse`` and ``hg help -e sparse``
for more info on the feature.

When a *sparse config* is enabled, the working directory only contains
files matching that config. You cannot ``hg add`` or ``hg remove`` files
outside the *sparse config*.

.. warning::

   Sparse support in Mercurial 4.3 does not have any backwards
   compatibility guarantees. Expect things to change. Scripting against
   commands or relying on behavior is strongly discouraged.

In-Tree Sparse Profiles
=======================

Mercurial supports defining the sparse config using files under version
control. These are called *sparse profiles*.

Essentially, the sparse profiles are managed just like any other file in
the repository. When you ``hg update``, the sparse configuration is
evaluated against the sparse profile at the revision being updated to.
From an end-user perspective, you just need to *activate* a profile once
and files will be added or removed as appropriate whenever the versioned
profile file updates.

In the Firefox repository, the ``build/sparse-profiles`` directory
contains Mercurial *sparse profiles* files.

Each *sparse profile* essentially defines a list of file patterns
(see ``hg help patterns``) to include or exclude. See
``hg help -e sparse`` for more.

Mach Support for Sparse Checkouts
=================================

``mach`` detects when a sparse checkout is being used and its
behavior may vary to accommodate this.

By default it is a fatal error if ``mach`` can't load one of the
``mach_commands.py`` files it was told to. But if a sparse checkout
is being used, ``mach`` assumes that file isn't part of the sparse
checkout and to ignore missing file errors. This means that
running ``mach`` inside a sparse checkout will only have access
to the commands defined in files in the sparse checkout.

Sparse Checkouts in Automation
==============================

``hg robustcheckout`` (the extension/command used to perform clones
and working directory operations in automation) supports sparse checkout.
However, it has a number of limitations over Mercurial's default sparse
checkout implementation:

* Only supports 1 profile at a time
* Does not support non-profile sparse configs
* Does not allow transitioning from a non-sparse to sparse checkout or
  vice-versa

These restrictions ensure that any sparse working directory populated by
``hg robustcheckout`` is as consistent and robust as possible.

``run-task`` (the low-level script for *bootstrapping* tasks in
automation) has support for sparse checkouts.

TaskGraph tasks using ``run-task`` can specify a ``sparse-profile``
attribute in YAML (or in code) to denote the sparse profile file to
use. e.g.::

   run:
       using: run-command
       command: <command>
       sparse-profile: tasgraph

This automagically results in ``run-task`` and ``hg robustcheckout``
using the sparse profile defined in ``build/sparse-profiles/<value>``.

Pros and Cons of Sparse Checkouts
=================================

The benefits of sparse checkout are that it makes the repository appear
to be smaller. This means:

* Less time performing working directory operations -> faster version
  control operations
* Fewer files to consult -> faster operations
* Working directories only contain what is needed -> easier to understand
  what everything does

Fewer files in the working directory also contributes to disadvantages:

* Searching may not yield hits because a file isn't in the sparse
  checkout. e.g. a *global* search and replace may not actually be
  *global* after all.
* Tools performing filesystem walking or path globbing (e.g.
  ``**/*.js``) may fail to find files because they don't exist.
* Various tools and processes make assumptions that all files in the
  repository are always available.

There can also be problems caused by mixing sparse and non-sparse
checkouts. For example, if a process in automation is using sparse
and a local developer is not using sparse, things may work for the
local developer but fail in automation (because a file isn't included
in the sparse configuration and not available to automation.
Furthermore, if environments aren't using exactly the same sparse
configuration, differences can contribute to varying behavior.

When Should Sparse Checkouts Be Used?
=====================================

Developers are discouraged from using sparse checkouts for local work
until tools for handling sparse checkouts have improved. In particular,
Mercurial's support for sparse is still experimental and various Firefox
tools make assumptions that all files are available. Developers should
use sparse checkout at their own risk.

The use of sparse checkouts in automation is a performance versus
robustness trade-off. Use of sparse checkouts will make automation
faster because machines will only have to manage a few thousand files
in a checkout instead of a few hundred thousand. This can potentially
translate to minutes saved per machine day. At the scale of thousands
of machines, the savings can be significant. But adopting sparse
checkouts will open up new avenues for failures. (See section above.)
If a process is isolated (in terms of file access) and well-understood,
sparse checkout can likely be leveraged with little risk. But if a
process is doing things like walking the filesystem and performing
lots of wildcard matching, the dangers are higher.
