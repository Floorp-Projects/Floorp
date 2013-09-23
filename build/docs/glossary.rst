========
Glossary
========

.. glossary::
   :sorted:

   object directory
       A directory holding the output of the build system. The build
       system attempts to isolate all file modifications to this
       directory. By convention, object directories are commonly
       directories under the source directory prefixed with **obj-**.
       e.g. **obj-firefox**.

   mozconfig
       A shell script used to configure the build system.

   configure
       A generated shell script which detects the current system
       environment, applies a requested set of build configuration
       options, and writes out metadata to be consumed by the build
       system.

   config.status
       An executable file produced by **configure** that takes the
       generated build config and writes out files used to build the
       tree. Traditionally, config.status writes out a bunch of
       Makefiles.
