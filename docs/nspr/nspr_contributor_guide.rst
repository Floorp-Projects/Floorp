NSPR contributor guide
======================

**Abstract:**

   NSPR accepts contributions in the form of bugfixes, new features,
   libraries, platform ports, documentation, test cases and other items
   from many sources. We (the NSPR module owners) sometimes disappoint
   our contributors when we must reject their contributions. We reject
   contributions for a variety of reasons. Some of these reasons are not
   obvious to an outside observer. NSPR wishes to document some
   guidelines for those who would contribute to NSPR. These guidelines
   should help the contributor in crafting his contribution, increasing
   its likelihood for acceptance.

General Guidelines
~~~~~~~~~~~~~~~~~~

*Downward Compatibility*
^^^^^^^^^^^^^^^^^^^^^^^^

Because many different applications, besides the mozilla client, use the
NSPR API, the API must remain downward compatible across even major
releases. This means that the behavior of an existing public API item in
NSPR cannot change. Should you need to have a similar API, with some
slightly different behavior or different function prototype, then
suggest a new API with a different name.

*C Language API*
^^^^^^^^^^^^^^^^

The NSPR API is a C Language API. Please do not contribute Java, C or
other language wrappers.

*Coding Style*
^^^^^^^^^^^^^^

NSPR does not have a documented coding style guide. Look at the extant
code. Make yours look like that. Some guidelines concerning naming
conventions can be found in :ref:`NSPR_Naming_Conventions`.
in the :ref:`NSPR API Reference`.

*Ownership of your contribution*
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

When you contribute something to NSPR, you must have intellectual
property rights to that contribution. This means that you cannot give us
something you snatched from somewhere else;. it must be your own
invention, free and clear of encumberment of anyone or anything else;
pay close attention to the rights of your "Day-Job" employer. If you
snatched it from somewhere else, tell us where; show us where the right
to incorporate it into NSPR exists.

*License under MPL or GPL*
^^^^^^^^^^^^^^^^^^^^^^^^^^

When you contribute material to NSPR, you agree to allow your
contribution to be licensed under the MPL or GPL.

BugFixes
~~~~~~~~

Use `Bugzilla <https://bugzilla.mozilla.org/>`__ to track bugs. Document
the bug or use an existing report. Be verbose in describing what you are
doing and why.

Include your changes as diffs in an attachment to the BugZilla report.

Use a coding style consistent with the source file you are changing.

New Features
~~~~~~~~~~~~

For purposes of this paper, a "new feature" is defined as some API
addition that goes into the core NSPR library, for example:
``libnspr4.dll``

NSPR is mostly complete. New APIs are driven mostly by the OS vendors as
they add new features. Should you decide that there's something that
NSPR does not cover that should be covered, let's talk. Your proposed
API should encapsulate a relatively low level capability as would be
found in a system call or libc.

Your new feature must be implemented on all platforms supported by NSPR.
When you consider a new API for NSPR ask yourself if your proposed
feature can implement it across all platforms supported by NSPR. If
several platforms cannot be made to implement your API, then it is not a
good candidate for inclusion in NSPR.

Before you begin what may be a substantial effort in making a candidate
feature for NSPR, talk to us. We may tell you that you have a good idea;
we may say that it really is not a good candidate for inclusion in NSPR;
we may give you suggestions on what would make it more generalized,
hence a good candidate for inclusion in NSPR.

Use `Bugzilla <https://bugzilla.mozilla.org>`__ to track your work. Be
verbose.

NSPR wants you to document your work. If we accept it, we are going to
have to answer questions about it and/or maintain it. These are some
guidelines for new APIs that you may add to NSPR.

**Header File Descriptions**. Provide header file descriptions that
fully document your public typedefs, enums, macros and functions.

See:
`prshm.h <http://lxr.mozilla.org/nspr/source/nsprpub/pr/include/prshm.h>`__
as an example of how your header file(s) should be documented.

*Source File Descriptions*o. Provide descriptive documentation in your
source (*.c) files. Alas, we have no source files documented as we think
they should be.

The following are some general guidelines to use when implementing new
features:

-  Don't export global variables
-  Your code must be thread safe
-  You must provide test cases that test all APIs you are adding. See:
   [#TestCases Test Cases]

New Libraries
~~~~~~~~~~~~~

All the guidelines applicable to [#NewFeatures New Features] applies to
new libraries.

For purposes of this paper, a "new library" is defined as a library under
the ``mozilla/nsprpub/lib`` directory tree and built as a separate
library. These libraries exist, for the most part, as "legacy" code from
NSPR 1.0. [Note that the current NSPR module owners do not now nor never
have been involved with NSPR 1.0.]. Such is life. That said: There are
some libraries that implement functions intended for use with
applications using NSPR, such as ``...nsprpub/lib/libc/plgetopt.*.``

-  generally useful
-  platform abstractions
-  you agree to sustain, bug fix
-  May rely on the NSPR API
-  May NOT rely on any other library API

New Platform Ports
~~~~~~~~~~~~~~~~~~

-  all NSPR API items must be implemented
-  platform specific headers in ``pr/include/md/_platformname.[h!cfg]``
-  platform specific code in ``pr/src/md/platform/*.c``
-  make rules in ``config/_platform.mk``

Documentation
~~~~~~~~~~~~~

The files for NSPR's documentation are maintained using a proprietary
word processing system [don't ask]. Document your work as described in
[#NewFeatures New Features]. Use the style of other NSPR documentation.
We will see that your documentation is transcribed into the appropriate
word processor and the derived HTML shows up on mozilla.org

Test Cases
~~~~~~~~~~

You should provide test cases for all new features and new libraries.

Give consideration to providing a test case when fixing a bug if an
existing test case did not catch a bug it should have caught.

The new test cases should be implemented in the style of other NSPR test
cases.

Test cases should prove that the added API items work as advertised.

Test cases should serve as an example of how to use the API items.

Test cases should provoke failure of every API item and report its
failure.

Frequently Asked Questions (FAQ)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

**Q:** Why was my contribution rejected?

**A:** Check the Bugzilla report covering your contribution.
