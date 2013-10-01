.. _mozbuild-files:

===============
moz.build Files
===============

``moz.build`` files are the mechanism by which tree metadata (notably
the build configuration) is defined.

Directories in the tree contain ``moz.build`` files which declare
functionality for their respective part of the tree. This includes
things such as the list of C++ files to compile, where to find tests,
etc.

``moz.build`` files are actually Python scripts. However, their
execution is governed by special rules. This is explained below.

moz.build Python Sandbox
========================

As mentioned above, ``moz.build`` files are Python scripts. However,
they are executed in a special Python *sandbox* that significantly
changes and limits the execution environment. The environment is so
different, it's doubtful most ``moz.build`` files would execute without
error if executed by a vanilla Python interpreter (e.g. ``python
moz.build``.

The following properties make execution of ``moz.build`` files special:

1. The execution environment exposes a limited subset of Python.
2. There is a special set of global symbols and an enforced naming
   convention of symbols.

The limited subset of Python is actually an extremely limited subset.
Only a few symbols from ``__builtins__`` are exposed. These include
``True``, ``False``, and ``None``. Global functions like ``import``,
``print``, and ``open`` aren't available. Without these, ``moz.build``
files can do very little. *This is by design*.

The execution sandbox treats all ``UPPERCASE`` variables specially. Any
``UPPERCASE`` variable must be known to the sandbox before the script
executes. Any attempt to read or write to an unknown ``UPPERCASE``
variable will result in an exception being raised. Furthermore, the
types of all ``UPPERCASE`` variables is strictly enforced. Attempts to
assign an incompatible type to an ``UPPERCASE`` variable will result in
an exception being raised.

The strictness of behavior with ``UPPERCASE`` variables is a very
intentional design decision. By ensuring strict behavior, any operation
involving an ``UPPERCASE`` variable is guaranteed to have well-defined
side-effects. Previously, when the build configuration was defined in
``Makefiles``, assignments to variables that did nothing would go
unnoticed. ``moz.build`` files fix this problem by eliminating the
potential for false promises.

In the sandbox, all ``UPPERCASE`` variables are globals and all
non-``UPPERCASE`` variables are locals. After a ``moz.build`` file has
completed execution, only the globals are used to retrieve state.

The set of variables and functions available to the Python sandbox is
defined by the :py:mod:`mozbuild.frontend.sandbox_symbols` module. The
data structures in this module are consumed by the
:py:class:`mozbuild.frontend.reader.MozbuildSandbox` class to construct
the sandbox. There are tests to ensure that the set of symbols exposed
to an empty sandbox are all defined in the ``sandbox_symbols`` module.
This module also contains documentation for each symbol, so nothing can
sneak into the sandbox without being explicitly defined and documented.

Reading and Traversing moz.build Files
======================================

The process responsible for reading ``moz.build`` files simply starts at
a root ``moz.build`` file, processes it, emits the globals namespace to
a consumer, and then proceeds to process additional referenced
``moz.build`` files from the original file. The consumer then examines
the globals/``UPPERCASE`` variables set as part of execution and then
converts the data therein to Python class instances.

The executed Python sandbox is essentially represented as a dictionary
of all the special ``UPPERCASE`` variables populated during its
execution.

The code for reading ``moz.build`` files lives in
:py:mod:`mozbuild.frontend.reader`. The evaluated Python sandboxes are
passed into :py:mod:`mozbuild.frontend.emitter`, which converts them to
classes defined in :py:mod:`mozbuild.frontend.data`. Each class in this
module define a domain-specific component of tree metdata. e.g. there
will be separate classes that represent a JavaScript file vs a compiled
C++ file or test manifests. This means downstream consumers of this data
can filter on class types to only consume what they are interested in.

There is no well-defined mapping between ``moz.build`` file instances
and the number of :py:mod:`mozbuild.frontend.data` classes derived from
each. Depending on the content of the ``moz.build`` file, there may be 1
object derived or 100.

The purpose of the ``emitter`` layer between low-level sandbox execution
and metadata representation is to facilitate a unified normalization and
verification step. There are multiple downstream consumers of the
``moz.build``-derived data and many will perform the same actions. This
logic can be complicated, so we a component dedicated to it.

Other Notes
===========

:py:class:`mozbuild.frontend.reader.BuildReader`` and
:py:class:`mozbuild.frontend.reader.TreeMetadataEmitter`` have a
stream-based API courtesy of generators. When you hook them up properly,
the :py:mod:`mozbuild.frontend.data` classes are emitted before all
``moz.build`` files have been read. This means that downstream errors
are raised soon after sandbox execution.

Lots of the code for evaluating Python sandboxes is applicable to
non-Mozilla systems. In theory, it could be extracted into a standalone
and generic package. However, until there is a need, there will
likely be some tightly coupled bits.
