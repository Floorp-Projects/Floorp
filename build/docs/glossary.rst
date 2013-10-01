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

   install manifest
       A file containing metadata describing file installation rules.
       A large part of the build system consists of copying files
       around to appropriate places. We write out special files
       describing the set of required operations so we can process the
       actions effeciently. These files are install manifests.

   clobber build
      A build performed with an initially empty object directory. All
      build actions must be performed.

   incremental build
      A build performed with the result of a previous build in an
      object directory. The build should not have to work as hard because
      it will be able to reuse the work from previous builds.

   mozinfo
      An API for accessing a common and limited subset of the build and
      run-time configuration. See :ref:`mozinfo`.
