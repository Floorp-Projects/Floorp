.. _jar_manifests:

=============
JAR Manifests
=============

JAR Manifests are plaintext files in the tree that are used to package chrome
files into ``.jar`` files and create :ref:`Chrome Registration <Chrome Registration>`
manifests. JAR Manifests are commonly named ``jar.mn``. They are declared in ``moz.build`` files using the ``JAR_MANIFESTS`` variable, which makes up a collection of ``jar.mn`` files.
All files declared in JAR Manifests are processed and installed into ``omni.ja`` files in ``browser/`` and ``toolkit/`` when building Firefox.

``jar.mn`` files are automatically processed by the build system when building a
source directory that contains one. The ``jar.mn`` is run through the
:ref:`preprocessor` before being passed to the manifest processor. In order to
have ``@variables@`` expanded (such as ``@AB_CD@``) throughout the file, add
the line ``#filter substitution`` at the top of your ``jar.mn`` file.

The format of a jar.mn is fairly simple; it consists of a heading specifying
which JAR file is being packaged, followed by indented lines listing files and
chrome registration instructions.

For a simple ``jar.mn`` file, see `toolkit/profile/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/toolkit/profile/jar.mn>`_. For a much
more complex ``jar.mn`` file, see `toolkit/locales/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/toolkit/locales/jar.mn>`_. More examples with specific formats and uses are available below.

Shipping Chrome Files
======================
General Format
^^^^^^^^^^^^^^
To ship chrome files in a JAR, an indented line indicates a file to be packaged::

   <jarfile>.jar:
     path/in/jar/file_name.xul     (source/tree/location/file_name.xul)

Note that file path mappings are listed by destination (left) followed by source (right).

Same Directory Omission
^^^^^^^^^^^^^^^^^^^^^^^
If the JAR manifest and packaged files live in the same directory, the source path and parentheses can be omitted.
A sample of a ``jar.mn`` file with omitted source paths and parentheses is `this revision of browser/components/colorways/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/browser/components/colorways/jar.mn>`_::

   browser.jar:
     content/browser/colorwaycloset.html
     content/browser/colorwaycloset.css
     content/browser/colorwaycloset.js

Writing the following is equivalent, given that the aforementioned files exist in the same directory as the ``jar.mn``. Notice the ``.jar`` file is named ``browser.jar``::

   browser.jar:
     content/browser/colorwaycloset.html (colorwaycloset.html)
     content/browser/colorwaycloset.css  (colorwaycloset.css)
     content/browser/colorwaycloset.js   (colorwaycloset.js)

This manifest is responsible for packaging files needed by Colorway Closet, including
JS scripts, localization files, images (ex. PNGs, AVIFs), and CSS styling. Look at `browser/components/colorways/colorwaycloset.html <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/browser/components/colorways/colorwaycloset.html#18>`_
to see how a file may be referenced using its chrome URL.

Absolute Paths
^^^^^^^^^^^^^^
The source tree location may also be an absolute path (taken from the top of the source tree).
One such example can be found in `toolkit/components/pictureinpicture/jar.mn <https://searchfox.org/mozilla-central/rev/2005e8d87ee045f19dac58e5bff32eff7d01bc9b/toolkit/components/pictureinpicture/jar.mn>`_::

   toolkit.jar:
     * content/global/pictureinpicture/player.xhtml   (content/player.xhtml)
     content/global/pictureinpicture/player.js      (content/player.js)

Asterisk Marker (Preprocessing)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
An asterisk marker (``*``) at the beginning of the line indicates that the file should be processed by the :ref:`preprocessor` before being packaged.
The file `toolkit/profile/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/toolkit/profile/jar.mn>`_ indicates that the file `toolkit/profile/content/profileDowngrade.xhtml <https://searchfox.org/mozilla-central/rev/2005e8d87ee045f19dac58e5bff32eff7d01bc9b/toolkit/profile/content/profileDowngrade.xhtml#34,36>`_ should be
run through the preprocessor, since it contains ``#ifdef`` and ``#endif`` statements that need to be interpreted::

   * content/mozapps/profile/profileDowngrade.xhtml  (content/profileDowngrade.xhtml)

Base Path, Variables, Wildcards and Localized Files
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
The ``.jar`` file location may be preceded with a base path between square brackets.
The file `toolkit/locales/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/toolkit/locales/jar.mn>`_ uses a base path so that the ``.jar`` file is under a ``localization`` directory,
which is a `special directory parsed by mozbuild <https://searchfox.org/mozilla-central/rev/2005e8d87ee045f19dac58e5bff32eff7d01bc9b/python/mozbuild/mozpack/packager/l10n.py#260-265>`_.

It is also named according to the value passed by the variable ``@AB_CD@``, normally a locale. Note the use of the preprocessor directive ``#filter substitution`` at the top of the file for replacing the variable with the value::

   #filter substitution

   ...

   [localization] @AB_CD@.jar:
     crashreporter                                    (%crashreporter/**/*.ftl)
     toolkit                                          (%toolkit/**/*.ftl)

The percentage sign in front of the source paths designates the locale to target as a source. By default, this is ``en-US``. With this specific example, `/toolkit/locales/en-US <https://searchfox.org/mozilla-central/source/toolkit/locales/en-US>`_ would be targeted.
Otherwise, the file from an alternate localization source tree ``/l10n/<locale>/toolkit/`` is read if building a localized version.
The wildcards in ``**/*.ftl`` tell the processor to install all Fluent files within the ``crashreporter`` and ``toolkit`` directories, as well as their subdirectories.

Registering Chrome
==================

:ref:`Chrome Registration <Chrome Registration>` instructions are marked with a percent sign (``%``) at the beginning of the
line, and must be part of the definition of a JAR file. Any additional percents
signs are replaced with an appropriate relative URL of the JAR file being
packaged.

There are two possible locations for a manifest file. If the chrome is being
built into a standalone application, the ``jar.mn`` processor creates a
``<jarfilename>.manifest`` next to the JAR file itself. This is the default
behavior.

If the ``moz.build`` specifies ``USE_EXTENSION_MANIFEST = 1``, the ``jar.mn`` processor
creates a single ``chrome.manifest`` file suitable for registering chrome as
an extension.

Example
^^^^^^^

The file `browser/themes/addons/jar.mn <https://searchfox.org/mozilla-central/rev/5b2d2863bd315f232a3f769f76e0eb16cdca7cb0/browser/themes/addons/jar.mn>`_ registers a ``resource`` chrome package under the name ``builtin-themes``. Its source files are in ``%content/builtin-themes/``::

   browser.jar:
     %  resource builtin-themes %content/builtin-themes/

     content/builtin-themes/alpenglow                 (alpenglow/*.svg)
     content/builtin-themes/alpenglow/manifest.json   (alpenglow/manifest.json)

Notice how other files declare an installation destination using the ``builtin-themes`` resource that is defined. As such, a SVG file ``preview.svg`` for a theme ``Alpenglow`` may be loaded using the resource URL ``resource://builtin-themes/alpenglow/preview.svg``
so that a preview of the theme is available on ``about:addons``. See :ref:`Chrome Registration <Chrome Registration>` for more details on ``resource`` and other manifest instructions.
