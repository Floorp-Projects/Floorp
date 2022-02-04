NSPR
====

**Netscape Portable Runtime (NSPR)** provides a platform-neutral API for
system level and libc-like functions. The API is used in the Mozilla
clients, many of Red Hat's and Oracle's server applications, and other
software offerings.

Documentation
-------------

:ref:`About NSPR`
   This topic describes, in general terms, the goals of NSPR and a bit
   about how it does it.
:ref:`NSPR API Reference`
   The reference describes each API public macro, structure and function
   in the NSPR API.
:ref:`NSPR build instructions`
   How to checkout and build from source.
:ref:`NSPR listing`
   All NSPR pages

.. _Getting_NSPR:

Getting NSPR
------------

NSPR is available in various source and binary packages, depending on
your platform:

-  **Windows:** Build the source package, using the :ref:`NSPR build
   instructions`.
-  **Mac:** Install the `MacPorts <http://www.macports.org/>`__ *nspr*
   package, or the `Homebrew <http://brew.sh>`__ *nspr* package.
-  **Ubuntu:** Install the *libnspr4-dev* package via ``apt-get.``
-  **Debian:** Install the *libnspr4-dev* package via ``apt-get``.
-  **openSUSE Linux:** Install one or more of the following via ``yast``
   or ``zypper`` :

   -  *mozilla-nspr* : Binary libraries for your platform
   -  *mozilla-nspr-32bit* : Binary libraries needed to run 32-bit
      programs on a 64-bit OS
   -  *mozilla-nspr-devel* : Files needed (in addition to the above
      libraries) to compile programs using NSPR
   -  *mozilla-nspr-debuginfo* : Debug information (including build
      symbols) for package *mozilla-nspr*
   -  *mozilla-nspr-debuginfo-32bit* : Debug information (including
      build symbols) for package *mozilla-nspr-32bit*
   -  *mozilla-nspr-debugsource* : Debug sources for all of the above

Community
---------

View Mozilla forums:

-  `Mailing list <https://lists.mozilla.org/listinfo/dev-tech-nspr>`__
-  `Newsgroup <http://groups.google.com/group/mozilla.dev.tech.nspr>`__
-  `RSS
   feed <http://groups.google.com/group/mozilla.dev.tech.nspr/feeds>`__

.. _Related_Topics:

Related Topics
--------------

-  :ref:`Networking`, :ref:`Network Security Services (NSS)`
