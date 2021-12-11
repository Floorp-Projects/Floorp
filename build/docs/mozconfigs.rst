.. _mozconfig:

===============
mozconfig Files
===============

mozconfig files are used to configure how a build works.

mozconfig files are actually shell scripts. They are executed in a
special context with specific variables and functions exposed to them.

API
===

Functions
---------

The following special functions are available to a mozconfig script.

ac_add_options
^^^^^^^^^^^^^^

This function is used to declare extra options/arguments to pass into
configure.

e.g.::

    ac_add_options --disable-tests
    ac_add_options --enable-optimize

mk_add_options
^^^^^^^^^^^^^^

This function is used to inject statements into client.mk for execution.
It is typically used to define variables, notably the object directory.

e.g.::

    mk_add_options AUTOCLOBBER=1

Special mk_add_options Variables
--------------------------------

For historical reasons, the method for communicating certain
well-defined variables is via mk_add_options(). In this section, we
document what those special variables are.

MOZ_OBJDIR
^^^^^^^^^^

This variable is used to define the :term:`object directory` for the current
build.

Finding the active mozconfig
============================

Multiple mozconfig files can exist to provide different configuration
options for different tasks. The rules for finding the active mozconfig
are defined in the
:py:func:`mozboot.mozconfig.find_mozconfig` method.

.. automodule:: mozboot.mozconfig
   :members: find_mozconfig

Loading the active mozconfig
----------------------------

.. autoclass:: mozbuild.mozconfig.MozconfigLoader
   :members: read_mozconfig
