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
just needs to define a library name with the ``LIBRARY_NAME`` variable.

   LIBRARY_NAME = 'foo'

The library file name will be ``libfoo.a`` on UNIX systems and ``foo.lib`` on
Windows.

If the static library needs to aggregate other static libraries, a list of
``LIBRARY_NAME`` can be added to the ``USE_LIBS`` variable. Like ``SOURCES``, it
requires the appended list to be alphanumerically ordered.

   USE_LIBS += ['bar', 'baz']

If there are multiple directories containing the same ``LIBRARY_NAME``, it is
possible to disambiguate by prefixing with the path to the wanted one (relative
or absolute):

   USE_LIBS += [
       '/path/from/topsrcdir/to/bar',
       '../relative/baz',
   ]

Note that the leaf name in those paths is the ``LIBRARY_NAME``, not an actual
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

   FINAL_LIBRARY = 'xul'

The ``FINAL_LIBRARY`` value must match a unique ``LIBRARY_NAME`` somewhere
in the tree.

As a special rule, those intermediate libraries don't need a ``LIBRARY_NAME``
for themselves.


Shared Libraries
================

Sometimes, we want shared libraries, a.k.a. dynamic libraries. Such libraries
are defined with the same variables as static libraries, with the addition of
the ``FORCE_SHARED_LIB`` boolean variable:

   FORCE_SHARED_LIB = True

When this variable is set, no static library is built. See further below to
build both types of libraries.

With a ``LIBRARY_NAME`` of ``foo``, the library file name will be
``libfoo.dylib`` on OSX, ``libfoo.so`` on ELF systems (Linux, etc.), and
``foo.dll`` on Windows. On Windows, there is also an import library named
``foo.lib``, used on the linker command line. ``libfoo.dylib`` and
``libfoo.so`` are considered the import library name for, resp. OSX and ELF
systems.

On OSX, one may want to create a special kind of dynamic library: frameworks.
This is done with the ``IS_FRAMEWORK`` boolean variable.

   IS_FRAMEWORK = True

With a ``LIBRARY_NAME`` of ``foo``, the framework file name will be ``foo``.
This variable however affects the behavior on all platforms, so it needs to
be set only on OSX.

Another special kind of library, XPCOM-specific, are XPCOM components. One can
build such a component with the ``IS_COMPONENT`` boolean variable.

   IS_COMPONENT = True


Executables
===========

Executables, a.k.a. programs, are, in the simplest form, defined with the
``PROGRAM`` variable.

   PROGRAM = 'foobar'

On UNIX systems, the executable file name will be ``foobar``, while on Windows,
it will be ``foobar.exe``.

Like static and shared libraries, the build system can be instructed to link
libraries to the executable with ``USE_LIBS``, listing various ``LIBRARY_NAME``.

In some cases, we want to create an executable per source file in the current
directory, in which case we can use the ``SIMPLE_PROGRAMS`` list:

   SIMPLE_PROGRAMS = [
       'FirstProgram',
       'SecondProgram',
   ]

The corresponding ``SOURCES`` must match:

   SOURCES += [
       'FirstProgram.cpp',
       'SecondProgram.c',
   ]

Similar to ``SIMPLE_PROGRAMS``, is ``CPP_UNIT_TESTS``, which defines, with the
same rules, C++ unit tests programs.


Linking with system libraries
=============================

Programs and libraries usually need to link with system libraries, such as a
widget toolkit, etc. Those required dependencies can be given with the
``OS_LIBS`` variable.

   OS_LIBS += [
       'foo',
       'bar',
   ]

This expands to ``foo.lib bar.lib`` when building with MSVC, and
``-lfoo -lbar`` otherwise.

For convenience with ``pkg-config``, ``OS_LIBS`` can also take linker flags
such as ``-L/some/path`` and ``-llib``, such that it is possible to directly
assign ``LIBS`` variables from ``CONFIG``, such as:

   OS_LIBS += CONFIG['MOZ_PANGO_LIBS']

(assuming ``CONFIG['MOZ_PANGO_LIBS']`` is a list, not a string)

Like ``USE_LIBS``, this variable applies to static and shared libraries, as
well as programs.


Libraries from third party build system
=======================================

Some libraries in the tree are not built by the moz.build-governed build
system, and there is no ``LIBRARY_NAME`` corresponding to them.

However, ``USE_LIBS`` allows to reference such libraries by giving a full
path (like when disambiguating identical ``LIBRARY_NAME``). The same naming
rules apply as other uses of ``USE_LIBS``, so only the library name without
prefix and suffix shall be given.

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

   FORCE_SHARED_LIB = True
   FORCE_STATIC_LIB = True

But because static libraries and Windows import libraries have the same file
names, either the static or the shared library name needs to be different
than ``LIBRARY_NAME``.

The ``STATIC_LIBRARY_NAME`` and ``SHARED_LIBRARY_NAME`` variables can be used
to change either the static or the shared library name.

  LIBRARY_NAME = 'foo'
  STATIC_LIBRARY_NAME = 'foo_s'

With the above, on Windows, ``foo_s.lib`` will be the static library,
``foo.dll`` the shared library, and ``foo.lib`` the import library.

In some cases, for convenience, it is possible to set both
``STATIC_LIBRARY_NAME`` and ``SHARED_LIBRARY_NAME``. For example:

  LIBRARY_NAME = 'mylib'
  STATIC_LIBRARY_NAME = 'mylib_s'
  SHARED_LIBRARY_NAME = CONFIG['SHARED_NAME']

This allows to use ``mylib`` in the ``USE_LIBS`` of another library or
executable.

When refering to a ``LIBRARY_NAME`` building both types of libraries in
``USE_LIBS``, the shared library is chosen to be linked. But sometimes,
it is wanted to link the static version, in which case the ``LIBRARY_NAME``
needs to be prefixed with ``static:`` in ``USE_LIBS``

   a/moz.build:
      LIBRARY_NAME = 'mylib'
      FORCE_SHARED_LIB = True
      FORCE_STATIC_LIB = True
      STATIC_LIBRARY_NAME = 'mylib_s'
   b/moz.build:
      PROGRAM = 'myprog'
      USE_LIBS += [
          'static:mylib',
      ]


Miscellaneous
=============

The ``SDK_LIBRARY`` boolean variable defines whether the library in the current
directory is going to be installed in the SDK.

The ``SONAME`` variable declares a "shared object name" for the library. It
defaults to the ``LIBRARY_NAME`` or the ``SHARED_LIBRARY_NAME`` if set. When
linking to a library with a ``SONAME``, the resulting library or program will
have a dependency on the library with the name corresponding to the ``SONAME``
instead of ``LIBRARY_NAME``. This only impacts ELF systems.

   a/moz.build:
      LIBRARY_NAME = 'mylib'
   b/moz.build:
      LIBRARY_NAME = 'otherlib'
      SONAME = 'foo'
   c/moz.build:
      PROGRAM = 'myprog'
      USE_LIBS += [
          'mylib',
          'otherlib',
      ]

On e.g. Linux, the above ``myprog`` will have DT_NEEDED markers for
``libmylib.so`` and ``libfoo.so`` instead of ``libmylib.so`` and
``libotherlib.so`` if there weren't a ``SONAME``. This means the runtime
requirement for ``myprog`` is ``libfoo.so`` instead of ``libotherlib.so``.
