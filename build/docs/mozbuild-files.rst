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
3. Some symbols are inherited from previously-executed ``moz.build``
   files.

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

After a ``moz.build`` file has completed execution, only the
``UPPERCASE`` variables are used to retrieve state.

The set of variables and functions available to the Python sandbox is
defined by the :py:mod:`mozbuild.frontend.context` module. The
data structures in this module are consumed by the
:py:class:`mozbuild.frontend.reader.MozbuildSandbox` class to construct
the sandbox. There are tests to ensure that the set of symbols exposed
to an empty sandbox are all defined in the ``context`` module.
This module also contains documentation for each symbol, so nothing can
sneak into the sandbox without being explicitly defined and documented.

Reading and Traversing moz.build Files
======================================

The process for reading ``moz.build`` files roughly consists of:

1. Start at the root ``moz.build`` (``<topsrcdir>/moz.build``).
2. Evaluate the ``moz.build`` file in a new sandbox.
3. Emit the main *context* and any *sub-contexts* from the executed
   sandbox.
4. Extract a set of ``moz.build`` files to execute next.
5. For each additional ``moz.build`` file, goto #2 and repeat until all
   referenced files have executed.

From the perspective of the consumer, the output of reading is a stream
of :py:class:`mozbuild.frontend.reader.context.Context` instances. Each
``Context`` defines a particular aspect of data. Consumers iterate over
these objects and do something with the data inside. Each object is
essentially a dictionary of all the ``UPPERCASE`` variables populated
during its execution.

.. note::

   Historically, there was only one ``context`` per ``moz.build`` file.
   As the number of things tracked by ``moz.build`` files grew and more
   and more complex processing was desired, it was necessary to split these
   contexts into multiple logical parts. It is now common to emit
   multiple contexts per ``moz.build`` file.

Build System Reading Mode
-------------------------

The traditional mode of evaluation of ``moz.build`` files is what's
called *build system traversal mode.* In this mode, the ``CONFIG``
variable in each ``moz.build`` sandbox is populated from data coming
from ``config.status``, which is produced by ``configure``.

During evaluation, ``moz.build`` files often make decisions conditional
on the state of the build configuration. e.g. *only compile foo.cpp if
feature X is enabled*.

In this mode, traversal of ``moz.build`` files is governed by variables
like ``DIRS`` and ``TEST_DIRS``. For example, to execute a child
directory, ``foo``, you would add ``DIRS += ['foo']`` to a ``moz.build``
file and ``foo/moz.build`` would be evaluated.

.. _mozbuild_fs_reading_mode:

Filesystem Reading Mode
-----------------------

There is an alternative reading mode that doesn't involve the build
system and doesn't use ``DIRS`` variables to control traversal into
child directories. This mode is called *filesystem reading mode*.

In this reading mode, the ``CONFIG`` variable is a dummy, mostly empty
object. Accessing all but a few special variables will return an empty
value. This means that nearly all ``if CONFIG['FOO']:`` branches will
not be taken.

Instead of using content from within the evaluated ``moz.build``
file to drive traversal into subsequent ``moz.build`` files, the set
of files to evaluate is controlled by the thing doing the reading.

A single ``moz.build`` file is not guaranteed to be executable in
isolation. Instead, we must evaluate all *parent* ``moz.build`` files
first. For example, in order to evaluate ``/foo/moz.build``, one must
execute ``/moz.build`` and have its state influence the execution of
``/foo/moz.build``.

Filesystem reading mode is utilized to power the
:ref:`mozbuild_files_metadata` feature.

Technical Details
-----------------

The code for reading ``moz.build`` files lives in
:py:mod:`mozbuild.frontend.reader`. The Python sandboxes evaluation results
(:py:class:`mozbuild.frontend.context.Context`) are passed into
:py:mod:`mozbuild.frontend.emitter`, which converts them to classes defined
in :py:mod:`mozbuild.frontend.data`. Each class in this module defines a
domain-specific component of tree metdata. e.g. there will be separate
classes that represent a JavaScript file vs a compiled C++ file or test
manifests. This means downstream consumers of this data can filter on class
types to only consume what they are interested in.

There is no well-defined mapping between ``moz.build`` file instances
and the number of :py:mod:`mozbuild.frontend.data` classes derived from
each. Depending on the content of the ``moz.build`` file, there may be 1
object derived or 100.

The purpose of the ``emitter`` layer between low-level sandbox execution
and metadata representation is to facilitate a unified normalization and
verification step. There are multiple downstream consumers of the
``moz.build``-derived data and many will perform the same actions. This
logic can be complicated, so we have a component dedicated to it.

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
