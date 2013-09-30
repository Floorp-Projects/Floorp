.. _python:

===========================
Python and the Build System
===========================

The Python programming language is used significantly in the build
system. If we need to write code for the build system or for a tool
related to the build system, Python is typically the first choice.

Python Requirements
===================

The tree requires Python 2.7.3 or greater but not Python 3 to build.
All Python packages not in the Python distribution are included in the
source tree. So all you should need is a vanilla Python install and you
should be good to go.

Only CPython (the Python distribution available from www.python.org) is
supported.

We require Python 2.7.3 (and not say 2.7.2) to build because Python
2.7.3 contains numerous bug fixes, especially around the area of Unicode
handling. These bug fixes are extremely annoying and have to be worked
around. The build maintainers were tired of doing this, so the minimum
version requirement was upped (bug 870420).

We intend to eventually support Python 3. This will come by way of dual
2.7/3.x compatibility because a single flag day conversion to 3.x will
be too cumbersome given the amount of Python that would need converted.
We will not know which 3.x minor release we are targeting until this
effort is underway. This is tracked in bug 636155.

Compiled Python Packages
========================

There are some features of the build that rely on compiled Python packages
(packages containing C source). These features are currently all
optional because not every system contains the Python development
headers required to build these extensions.

We recommend you have the Python development headers installed (``mach
bootstrap`` should do this for you) so you can take advantage of these
features.

Issues with OS X System Python
==============================

The Python that ships with OS X has historically been littered with
subtle bugs and suboptimalities. Furthermore, OS X up through 10.8 don't
ship with Python 2.7.3 (10.8 ships with 2.7.2).

OS X 10.8 and below users will be required to install a new Python
distribution. This may not be necessary for OS X 10.9+. However, we
still recommend installing a separate Python because of the history with
OS X's system Python issues.

We recommend installing Python through Homebrew or MacPorts. If you run
``mach bootstrap``, this should be done for you.

Virtualenvs
===========

The build system relies heavily on
`virtualenvs <http://www.virtualenv.org/en/latest/>`_. Virtualenvs are
standalone and isolated Python environments. The problem a virtualenv
solves is that of dependencies across multiple Python components. If two
components on a system relied on different versions of a package, there
could be a conflict. Instead of managing multiple versions of a package
simultaneously, Python and virtualenvs take the route that it is easier
to just keep them separate so there is no potential for conflicts.

Very early in the build process, a virtualenv is created inside the
:term:`object directory`. The virtualenv is configured such that it can
find all the Python packages in the source tree. The code for this lives
in :py:mod:`mozbuild.virtualenv`.

Deficiencies
------------

There are numerous deficiencies with the way virtualenvs are handled in
the build system.

* mach reinvents the virtualenv.

  There is code in ``build/mach_bootstrap.py`` that configures ``sys.path``
  much the same way the virtualenv does. There are various bugs tracking
  this. However, no clear solution has yet been devised. It's not a huge
  problem and thus not a huge priority.

* They aren't preserved across copies and packaging.

  If you attempt to copy an entire tree from one machine to another or
  from one directory to another, chances are the virtualenv will fall
  apart. It would be nice if we could preserve it somehow. Instead of
  actually solving portable virtualenvs, all we really need to solve is
  encapsulating the logic for populating the virtualenv along with all
  dependent files in the appropriate place.

* .pyc files written to source directory.

  We rely heavily on ``.pth`` files in our virtualenv. A ``.pth`` file
  is a special file that contains a list of paths. Python will take the
  set of listed paths encountered in ``.pth`` files and add them to
  ``sys.path``.

  When Python compiles a ``.py`` file to bytecode, it writes out a
  ``.pyc`` file so it doesn't have to perform this compilation again.
  It puts these ``.pyc`` files alongside the ``.pyc`` file. Python
  provides very little control for determing where these ``.pyc`` files
  go, even in Python 3 (which offers customer importers).

  With ``.pth`` files pointing back to directories in the source tree
  and not the object directory, ``.pyc`` files are created in the source
  tree. This is bad because when Python imports a module, it first looks
  for a ``.pyc`` file before the ``.py`` file. If there is a ``.pyc``
  file but no ``.py`` file, it will happily import the module. This
  wreaks havoc during file moves, refactoring, etc.

  There are various proposals for fixing this. See bug 795995.

Common Issues with Python
=========================

Upgrading your Python distribution breaks the virtualenv
--------------------------------------------------------

If you upgrade the Python distribution (e.g. install Python 2.7.5
from 2.7.3, chances are parts of the virtualenv will break.
This commonly manifests as a cryptic ``Cannot import XXX`` exception.
More often than not, the module being imported contains binary/compiled
components.

If you upgrade or reinstall your Python distribution, we recommend
clobbering your build.

Packages installed at the system level conflict with build system's
-------------------------------------------------------------------

It is common for people to install Python packages using ``sudo`` (e.g.
``sudo pip install psutil``) or with the system's package manager
(e.g. ``apt-get install python-mysql``.

A problem with this is that packages installed at the system level may
conflict with the package provided by the source tree. As of bug 907902
and changeset f18eae7c3b27 (September 16, 2013), this should no longer
be an issue since the virtualenv created as part of the build doesn't
add the system's ``site-packages`` directory to ``sys.path``. However,
poorly installed packages may still find a way to creep into the mix and
interfere with our virtualenv.

As a general principle, we recommend against using your system's package
manager or using ``sudo`` to install Python packages. Instead, create
virtualenvs and isolated Python environments for all of your Python
projects.

Python on $PATH is not appropriate
----------------------------------

Tools like ``mach`` will look for Python by performing ``/usr/bin/env
python`` or equivalent. Please be sure the appropriate Python 2.7.3+
path is on $PATH. On OS X, this likely means you'll need to modify your
shell's init script to put something ahead of ``/usr/bin``.
