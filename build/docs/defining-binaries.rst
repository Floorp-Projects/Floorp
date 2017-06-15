.. _defining_binaries:

======================================
Defining Binaries for the Build System
======================================

One part of what the build system does is compile C/C++ and link the resulting
objects to produce executables and/or libraries. This document describes the
basics of defining what is going to be built and how. All the following
describes constructs to use in moz.build files.


Source files
============

Source files to be used in a given directory are registered in the ``SOURCES``
and ``UNIFIED_SOURCES`` variables. ``UNIFIED_SOURCES`` have a special behavior
in that they are aggregated by batches of 16, requiring, for example, that there
are no conflicting variables in those source files.

``SOURCES`` and ``UNIFIED_SOURCES`` are lists which must be appended to, and
each append requires the given list to be alphanumerically ordered.

.. code-block:: python

   UNIFIED_SOURCES += [
       'FirstSource.cpp',
       'SecondSource.cpp',
       'ThirdSource.cpp',
   ]

   SOURCES += [
       'OtherSource.cpp',
   ]

``SOURCES`` and ``UNIFIED_SOURCES`` can contain a mix of different file types,
for C, C++, and Objective C.


Static Libraries
================

To build a static library, other than defining the source files (see above), one
just needs to define a library name with the ``Library`` template.

.. code-block:: python

   Library('foo')

The library file name will be ``libfoo.a`` on UNIX systems and ``foo.lib`` on
Windows.

If the static library needs to aggregate other static libraries, a list of
``Library`` names can be added to the ``USE_LIBS`` variable. Like ``SOURCES``, it
requires the appended list to be alphanumerically ordered.

.. code-block:: python

   USE_LIBS += ['bar', 'baz']

If there are multiple directories containing the same ``Library`` name, it is
possible to disambiguate by prefixing with the path to the wanted one (relative
or absolute):

.. code-block:: python

   USE_LIBS += [
       '/path/from/topsrcdir/to/bar',
       '../relative/baz',
   ]

Note that the leaf name in those paths is the ``Library`` name, not an actual
file name.

Note that currently, the build system may not create an actual library for
static libraries. It is an implementation detail that shouldn't need to be
worried about.

As a special rule, ``USE_LIBS`` is allowed to contain references to shared
libraries. In such cases, programs and shared libraries linking this static
library will inherit those shared library dependencies.


Intermediate (Static) Libraries
===============================

In many cases in the tree, static libraries are built with the only purpose
of being linked into another, bigger one (like libxul). Instead of adding all
required libraries to ``USE_LIBS`` for the bigger one, it is possible to tell
the build system that the library built in the current directory is meant to
be linked to that bigger library, with the ``FINAL_LIBRARY`` variable.

.. code-block:: python

   FINAL_LIBRARY = 'xul'

The ``FINAL_LIBRARY`` value must match a unique ``Library`` name somewhere
in the tree.

As a special rule, those intermediate libraries don't need a ``Library`` name
for themselves.


Shared Libraries
================

Sometimes, we want shared libraries, a.k.a. dynamic libraries. Such libraries
are defined similarly to static libraries, using the ``SharedLibrary`` template
instead of ``Library``.

.. code-block:: python

   SharedLibrary('foo')

When this template is used, no static library is built. See further below to
build both types of libraries.

With a ``SharedLibrary`` name of ``foo``, the library file name will be
``libfoo.dylib`` on OSX, ``libfoo.so`` on ELF systems (Linux, etc.), and
``foo.dll`` on Windows. On Windows, there is also an import library named
``foo.lib``, used on the linker command line. ``libfoo.dylib`` and
``libfoo.so`` are considered the import library name for, resp. OSX and ELF
systems.

On OSX, one may want to create a special kind of dynamic library: frameworks.
This is done with the ``Framework`` template.

.. code-block:: python

   Framework('foo')

With a ``Framework`` name of ``foo``, the framework file name will be ``foo``.
This template however affects the behavior on all platforms, so it needs to
be set only on OSX.


Executables
===========

Executables, a.k.a. programs, are, in the simplest form, defined with the
``Program`` template.

.. code-block:: python

   Program('foobar')

On UNIX systems, the executable file name will be ``foobar``, while on Windows,
it will be ``foobar.exe``.

Like static and shared libraries, the build system can be instructed to link
libraries to the executable with ``USE_LIBS``, listing various ``Library``
names.

In some cases, we want to create an executable per source file in the current
directory, in which case we can use the ``SimplePrograms`` template

.. code-block:: python

   SimplePrograms([
       'FirstProgram',
       'SecondProgram',
   ])

Contrary to ``Program``, which requires corresponding ``SOURCES``, when using
``SimplePrograms``, the corresponding ``SOURCES`` are implied. If the
corresponding ``sources`` have an extension different from ``.cpp``, it is
possible to specify the proper extension:

.. code-block:: python

   SimplePrograms([
       'ThirdProgram',
       'FourthProgram',
   ], ext='.c')

Please note this construct was added for compatibility with what already lives
in the mozilla tree ; it is recommended not to add new simple programs with
sources with a different extension than ``.cpp``.

Similar to ``SimplePrograms``, is the ``CppUnitTests`` template, which defines,
with the same rules, C++ unit tests programs. Like ``SimplePrograms``, it takes
an ``ext`` argument to specify the extension for the corresponding ``SOURCES``,
if it's different from ``.cpp``.


Linking with system libraries
=============================

Programs and libraries usually need to link with system libraries, such as a
widget toolkit, etc. Those required dependencies can be given with the
``OS_LIBS`` variable.

.. code-block:: python

   OS_LIBS += [
       'foo',
       'bar',
   ]

This expands to ``foo.lib bar.lib`` when building with MSVC, and
``-lfoo -lbar`` otherwise.

For convenience with ``pkg-config``, ``OS_LIBS`` can also take linker flags
such as ``-L/some/path`` and ``-llib``, such that it is possible to directly
assign ``LIBS`` variables from ``CONFIG``, such as:

.. code-block:: python

   OS_LIBS += CONFIG['MOZ_PANGO_LIBS']

(assuming ``CONFIG['MOZ_PANGO_LIBS']`` is a list, not a string)

Like ``USE_LIBS``, this variable applies to static and shared libraries, as
well as programs.


Libraries from third party build system
=======================================

Some libraries in the tree are not built by the moz.build-governed build
system, and there is no ``Library`` corresponding to them.

However, ``USE_LIBS`` allows to reference such libraries by giving a full
path (like when disambiguating identical ``Library`` names). The same naming
rules apply as other uses of ``USE_LIBS``, so only the library name without
prefix and suffix shall be given.

.. code-block:: python

   USE_LIBS += [
       '/path/from/topsrcdir/to/third-party/bar',
       '../relative/third-party/baz',
   ]

Note that ``/path/from/topsrcdir/to/third-party`` and
``../relative/third-party/baz`` must lead under a subconfigured directory (a
directory with an AC_OUTPUT_SUBDIRS in configure.in), or ``security/nss``.


Building both static and shared libraries
=========================================

When both types of libraries are required, one needs to set both
``FORCE_SHARED_LIB`` and ``FORCE_STATIC_LIB`` boolean variables.

.. code-block:: python

   FORCE_SHARED_LIB = True
   FORCE_STATIC_LIB = True

But because static libraries and Windows import libraries have the same file
names, either the static or the shared library name needs to be different
than the name given to the ``Library`` template.

The ``STATIC_LIBRARY_NAME`` and ``SHARED_LIBRARY_NAME`` variables can be used
to change either the static or the shared library name.

.. code-block:: python

  Library('foo')
  STATIC_LIBRARY_NAME = 'foo_s'

With the above, on Windows, ``foo_s.lib`` will be the static library,
``foo.dll`` the shared library, and ``foo.lib`` the import library.

In some cases, for convenience, it is possible to set both
``STATIC_LIBRARY_NAME`` and ``SHARED_LIBRARY_NAME``. For example:

.. code-block:: python

  Library('mylib')
  STATIC_LIBRARY_NAME = 'mylib_s'
  SHARED_LIBRARY_NAME = CONFIG['SHARED_NAME']

This allows to use ``mylib`` in the ``USE_LIBS`` of another library or
executable.

When refering to a ``Library`` name building both types of libraries in
``USE_LIBS``, the shared library is chosen to be linked. But sometimes,
it is wanted to link the static version, in which case the ``Library`` name
needs to be prefixed with ``static:`` in ``USE_LIBS``

::

   a/moz.build:
      Library('mylib')
      FORCE_SHARED_LIB = True
      FORCE_STATIC_LIB = True
      STATIC_LIBRARY_NAME = 'mylib_s'
   b/moz.build:
      Program('myprog')
      USE_LIBS += [
          'static:mylib',
      ]


Miscellaneous
=============

The ``SONAME`` variable declares a "shared object name" for the library. It
defaults to the ``Library`` name or the ``SHARED_LIBRARY_NAME`` if set. When
linking to a library with a ``SONAME``, the resulting library or program will
have a dependency on the library with the name corresponding to the ``SONAME``
instead of the ``Library`` name. This only impacts ELF systems.

::

   a/moz.build:
      Library('mylib')
   b/moz.build:
      Library('otherlib')
      SONAME = 'foo'
   c/moz.build:
      Program('myprog')
      USE_LIBS += [
          'mylib',
          'otherlib',
      ]

On e.g. Linux, the above ``myprog`` will have DT_NEEDED markers for
``libmylib.so`` and ``libfoo.so`` instead of ``libmylib.so`` and
``libotherlib.so`` if there weren't a ``SONAME``. This means the runtime
requirement for ``myprog`` is ``libfoo.so`` instead of ``libotherlib.so``.


Gecko-related binaries
======================

Some programs or libraries are totally independent of Gecko, and can use the
above mentioned templates. Others are Gecko-related in some way, and may
need XPCOM linkage, mozglue. These things are tedious. A set of additional
templates exists to ease defining such programs and libraries. They are
essentially the same as the above mentioned templates, prefixed with "Gecko":

  - ``GeckoProgram``
  - ``GeckoSimplePrograms``
  - ``GeckoCppUnitTests``
  - ``GeckoSharedLibrary``
  - ``GeckoFramework``

All the Gecko-prefixed templates take the same arguments as their
non-Gecko-prefixed counterparts, and can take a few more arguments
for non-standard cases. See the definition of ``GeckoBinary`` in
build/gecko_templates.mozbuild for more details, but most usecases
should not require these additional arguments.
