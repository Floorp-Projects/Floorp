.. _jar_manifests:

=============
JAR Manifests
=============

JAR Manifests are plaintext files in the tree that are used to package chrome
files into the correct JARs, and create
`Chrome Registration <https://developer.mozilla.org/en-US/docs/Chrome_Registration>`_
manifests. JAR Manifests are commonly named ``jar.mn``. They are
declared in ``moz.build`` files using the ``JAR_MANIFESTS`` variable.

``jar.mn`` files are automatically processed by the build system when building a
source directory that contains one. The ``jar``.mn is run through the
:ref:`preprocessor` before being passed to the manifest processor. In order to
have ``@variables@`` expanded (such as ``@AB_CD@``) throughout the file, add
the line ``#filter substitution`` at the top of your ``jar.mn`` file.

The format of a jar.mn is fairly simple; it consists of a heading specifying
which JAR file is being packaged, followed by indented lines listing files and
chrome registration instructions.

To see a simple ``jar.mn`` file at work, see ``toolkit/profile/jar.mn``. A much
more complex ``jar.mn`` is at ``toolkit/locales/jar.mn``.

Shipping Chrome Files
=====================

To ship chrome files in a JAR, an indented line indicates a file to be packaged::

   <jarfile>.jar:
     path/in/jar/file_name.xul     (source/tree/location/file_name.xul)

The JAR location may be preceded with a base path between square brackets::
   [base/path] <jarfile>.jar:
     path/in/jar/file_name.xul     (source/tree/location/file_name.xul)

In this case, the jar will be directly located under the given ``base/bath``,
while without a base path, it will be under a ``chrome`` directory.

If the JAR manifest and packaged file live in the same directory, the path and
parenthesis can be omitted. In other words, the following two lines are
equivalent::

   path/in/jar/same_place.xhtml     (same_place.xhtml)
   path/in/jar/same_place.xhtml

The source tree location may also be an *absolute* path (taken from the
top of the source tree::

   path/in/jar/file_name.xul     (/path/in/sourcetree/file_name.xul)

An asterisk marker (``*``) at the beginning of the line indicates that the
file should be processed by the :ref:`preprocessor` before being packaged::

   * path/in/jar/preprocessed.xul  (source/tree/location/file_name.xul)

A plus marker (``+``) at the beginning of the line indicates that the file
should replace an existing file, even if the source file's timestamp isn't
newer than the existing file::

   + path/in/jar/file_name.xul     (source/tree/location/my_file_name.xul)

Preprocessed files always replace existing files, to ensure that changes in
``#expand`` or ``#include`` directives are picked up, so ``+`` and ``*`` are
equivalent.

There is a special source-directory format for localized files (note the
percent sign in the source file location): this format reads ``localized.dtd``
from the ``en-US`` directory if building an English version, and reads the
file from the alternate localization source tree
``/l10n/<locale>/path/localized.dtd`` if building a localized version::

   locale/path/localized.dtd     (%localized/path/localized.dtd)

The source tree location can also use wildcards, in which case the path in
jar is expected to be a base directory. Paths before the wildcard are not
made part of the destination path::

     path/in/jar/                (source/tree/location/*.xul)

The above will install all xul files under ``source/tree/location`` as
``path/in/jar/*.xul``.

Register Chrome
===============

`Chrome Registration <https://developer.mozilla.org/en-US/docs/Chrome_Registration>`_
instructions are marked with a percent sign (``%``) at the beginning of the
line, and must be part of the definition of a JAR file. Any additional percents
signs are replaced with an appropriate relative URL of the JAR file being
packaged::

   % content global %path/in/jar/
   % overlay chrome://blah/content/blah.xul chrome://foo/content/overlay.xul

There are two possible locations for a manifest file. If the chrome is being
built into a standalone application, the ``jar.mn`` processor creates a
``<jarfilename>.manifest`` next to the JAR file itself. This is the default
behavior.

If the build specifies ``USE_EXTENSION_MANIFEST = 1``, the ``jar.mn`` processor
creates a single ``chrome.manifest`` file suitable for registering chrome as
an extension.
