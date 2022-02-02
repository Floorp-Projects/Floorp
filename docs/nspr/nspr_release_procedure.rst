NSPR release procedure
======================

Release checklist
~~~~~~~~~~~~~~~~~

#. Change the NSPR version in ``mozilla/nsprpub/pr/include/prinit.h``.
#. Change the NSPR version in
   ``mozilla/nsprpub/{configure.in,configure}``.
#. Change the NSPR version in ``mozilla/nsprpub/pr/tests/vercheck.c``.
#. Change the NSPR version in ``mozilla/nsprpub/admin/repackage.sh``.

.. _Source_tarball:

Source tarball
~~~~~~~~~~~~~~

.. _Binary_distributions:

Binary distributions
~~~~~~~~~~~~~~~~~~~~

Right now I use the ``mozilla/nsprpub/admin/repackage.sh`` script to
generate the binary distributions published on ftp.mozilla.org. As the
name of the shell script implies, ``repackage.sh`` merely repackages
binary distributions in a different format.

Before you run ``repackage.sh``, you need to have built the binary
distributions using the "gmake release" makefile target. These binary
distributions are jar files, which are really zip files, and they are
published in the directory ``/share/builds/components``. This design
comes from the Netscape days.

The ``repackage.sh`` script repackages the jar files into the form most
commonly used on that platform. So on Unix it repackages the jar files
into gzipped tar files, and on Windows it repackages the jar files into
zip files.

Edit the ``repackage.sh`` script to customize it for your environment.

After you have run ``repackage.sh``, follow the
`instructions <http://www.mozilla.org/build/ftp-stage.html>`__ in to
upload the files to ftp.mozilla.org's staging server, so that they
eventually show up on ftp.mozilla.org. The host ftp.mozilla.org can be
accessed via the ftp, http, and https protocols. We recommend using
https://ftp.mozilla.org/.

**Note:** For NSS, the script equivalent to NSPR's ``repackage.sh`` is
``/u/robobld/bin/sbsinit/nss/push/buildbindist.sh`` in the "SVBuild"
source tree.
