.. _mozbuild_files_metadata:

==============
Files Metadata
==============

:ref:`mozbuild-files` provide a mechanism for attaching metadata to
files. Essentially, you define some flags to set on a file or file
pattern. Later, some tool or process queries for metadata attached to a
file of interest and it does something intelligent with that data.

Defining Metadata
=================

Files metadata is defined by using the
:ref:`Files Sub-Context <mozbuild_subcontext_Files>` in ``moz.build``
files. e.g.::

    with Files('**/Makefile.in'):
        BUG_COMPONENT = ('Core', 'Build Config')

This working example says, *for all Makefile.in files in every directory
underneath this one - including this directory - set the Bugzilla
component to Core :: Build Config*.

For more info, read the
:ref:`docs on Files <mozbuild_subcontext_Files>`.

How Metadata is Read
====================

``Files`` metadata is extracted in :ref:`mozbuild_fs_reading_mode`.

Reading starts by specifying a set of files whose metadata you are
interested in. For each file, the filesystem is walked to the root
of the source directory. Any ``moz.build`` encountered during this
walking are marked as relevant to the file.

Let's say you have the following filesystem content::

   /moz.build
   /root_file
   /dir1/moz.build
   /dir1/foo
   /dir1/subdir1/foo
   /dir2/foo

For ``/root_file``, the relevant ``moz.build`` files are just
``/moz.build``.

For ``/dir1/foo`` and ``/dir1/subdir1/foo``, the relevant files are
``/moz.build`` and ``/dir1/moz.build``.

For ``/dir2``, the relevant file is just ``/moz.build``.

Once the list of relevant ``moz.build`` files is obtained, each
``moz.build`` file is evaluated. Root ``moz.build`` file first,
leaf-most files last. This follows the rules of
:ref:`mozbuild_fs_reading_mode`, with the set of evaluated ``moz.build``
files being controlled by filesystem content, not ``DIRS`` variables.

The file whose metadata is being resolved maps to a set of ``moz.build``
files which in turn evaluates to a list of contexts. For file metadata,
we only care about one of these contexts:
:ref:`Files <mozbuild_subcontext_Files>`.

We start with an empty ``Files`` instance to represent the file. As
we encounter a *files sub-context*, we see if it is appropriate to
this file. If it is, we apply its values. This process is repeated
until all *files sub-contexts* have been applied or skipped. The final
state of the ``Files`` instance is used to represent the metadata for
this particular file.

It may help to visualize this. Say we have 2 ``moz.build`` files::

    # /moz.build
    with Files('*.cpp'):
        BUG_COMPONENT = ('Core', 'XPCOM')

    with Files('**/*.js'):
        BUG_COMPONENT = ('Firefox', 'General')

    # /foo/moz.build
    with Files('*.js'):
        BUG_COMPONENT = ('Another', 'Component')

Querying for metadata for the file ``/foo/test.js`` will reveal 3
relevant ``Files`` sub-contexts. They are evaluated as follows:

1. ``/moz.build - Files('*.cpp')``. Does ``/*.cpp`` match
   ``/foo/test.js``? **No**. Ignore this context.
2. ``/moz.build - Files('**/*.js')``. Does ``/**/*.js`` match
   ``/foo/test.js``? **Yes**. Apply ``BUG_COMPONENT = ('Firefox', 'General')``
   to us.
3. ``/foo/moz.build - Files('*.js')``. Does ``/foo/*.js`` match
   ``/foo/test.js``? **Yes**. Apply
   ``BUG_COMPONENT = ('Another', 'Component')``.

At the end of execution, we have
``BUG_COMPONENT = ('Another', 'Component')`` as the metadata for
``/foo/test.js``.

One way to look at file metadata is as a stack of data structures.
Each ``Files`` sub-context relevant to a given file is applied on top
of the previous state, starting from an empty state. The final state
wins.

.. _mozbuild_files_metadata_finalizing:

Finalizing Values
=================

The default behavior of ``Files`` sub-context evaluation is to apply new
values on top of old. In most circumstances, this results in desired
behavior. However, there are circumstances where this may not be
desired. There is thus a mechanism to *finalize* or *freeze* values.

Finalizing values is useful for scenarios where you want to prevent
wildcard matches from overwriting previously-set values. This is useful
for one-off files.

Let's take ``Makefile.in`` files as an example. The build system module
policy dictates that ``Makefile.in`` files are part of the ``Build
Config`` module and should be reviewed by peers of that module. However,
there exist ``Makefile.in`` files in many directories in the source
tree. Without finalization, a ``*`` or ``**`` wildcard matching rule
would match ``Makefile.in`` files and overwrite their metadata.

Finalizing of values is performed by setting the ``FINAL`` variable
on ``Files`` sub-contexts. See the
:ref:`Files documentation <mozbuild_subcontext_Files>` for more.

Here is an example with ``Makefile.in`` files, showing how it is
possible to finalize the ``BUG_COMPONENT`` value.::

    # /moz.build
    with Files('**/Makefile.in'):
        BUG_COMPONENT = ('Core', 'Build Config')
        FINAL = True

    # /foo/moz.build
    with Files('**'):
        BUG_COMPONENT = ('Another', 'Component')

If we query for metadata of ``/foo/Makefile.in``, both ``Files``
sub-contexts match the file pattern. However, since ``BUG_COMPONENT`` is
marked as finalized by ``/moz.build``, the assignment from
``/foo/moz.build`` is ignored. The final value for ``BUG_COMPONENT``
is ``('Core', 'Build Config')``.

Here is another example::

    with Files('*.cpp'):
        BUG_COMPONENT = ('One-Off', 'For C++')
        FINAL = True

    with Files('**'):
        BUG_COMPONENT = ('Regular', 'Component')

For every files except ``foo.cpp``, the bug component will be resolved
as ``Regular :: Component``. However, ``foo.cpp`` has its value of
``One-Off :: For C++`` preserved because it is finalized.

.. important::

   ``FINAL`` only applied to variables defined in a context.

   If you want to mark one variable as finalized but want to leave
   another mutable, you'll need to use 2 ``Files`` contexts.

Guidelines for Defining Metadata
================================

In general, values defined towards the root of the source tree are
generic and become more specific towards the leaves. For example,
the ``BUG_COMPONENT`` for ``/browser`` might be ``Firefox :: General``
whereas ``/browser/components/preferences`` would list
``Firefox :: Preferences``.
