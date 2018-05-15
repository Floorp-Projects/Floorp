===============
Installer Build
===============

How to build the installers
---------------------------

The easiest way to build an installer in your local tree is to run ``mach package``. The finished installers will be in ``$OBJDIR/dist/install/sea/``. You have to have a build of the application already done before that will work, but `artifact builds <https://developer.mozilla.org/en-US/docs/Mozilla/Developer_guide/Build_Instructions/Artifact_builds>`_ are supported, so that can save you a lot of time if you're only doing installer work.

You'll also need to be on a Windows machine; the installer build depends on tools that aren't available for other platforms.

Stub installer
~~~~~~~~~~~~~~

The stub installer probably won't be built by default in a local tree; normally unless the build has been set to use one of the official update channels, only the full installer is built. If you need to work on the stub installer, you can override the default and get one built along with the full installer by adding ``export MOZ_STUB_INSTALLER=1`` to your mozconfig.

Uninstaller
~~~~~~~~~~~

The uninstaller is built as part of the main application build, not the installer target, so ``mach build`` is what will get you an uninstaller. You'll find it at ``$OBJDIR/dist/bin/uninstaller/helper.exe``.

Branding
~~~~~~~~

By default local builds use "unofficial" branding, which somewhat resembles a previous version of the Nightly branding, but is designed not to resemble any official channel too closely.

But sometimes you'll need to test installers that are built using one or more of the official channel branding configurations, perhaps to try out different strings, make sure different sets of art look good, or to test behavior around installing multiple channels at the same time.

You can build installers (and the entire application) with official branding by adding ``ac_add_options --with-branding=browser/branding/{nightly|aurora|official}`` to your mozconfig (the default branding is ``browser/branding/unofficial``).

Build process
-------------

Both the full and stub installers are built through a similar process, which is summarized here along with references to the relevant bits of code.

Most of this procedure is done in `makensis.mk <http://searchfox.org/mozilla-central/source/toolkit/mozapps/installer/windows/nsis/makensis.mk>`_ and in the `mach repackage <https://searchfox.org/mozilla-central/rev/2b9779c59390ecc47be7a70d99753653d8eb5afc/python/mozbuild/mozbuild/mach_commands.py#2166>`_ command.

0. A prerequisite is for the application to be in a packaged state, so ``mach package`` first creates a release-style package and puts it in ``$OBJDIR/dist/firefox``.
1. All required files are copied into the instgen directory. This includes .nsi and .nsh script files, plugin DLL files, image and icon files, and the 7-zip SFX module and its configuration files.
2. The NSIS scripts are compiled, resulting in setup.exe and setup-stub.exe (if building the stub is enabled).
3. The 7-zip SFX module is run through UPX.
4. The application files and the full installer setup.exe are compressed together into one 7-zip file.
5. The stub installer is compressed into its own 7-zip file.
6. The (UPX-packed) 7-zip SFX module, the correct configuration data, and the 7-zip file containing the application files and setup.exe are concatenated together. This results in the final full installer.
7. The (still UPX-packed) 7-zip SFX module, the correct configuration data, and the 7-zip file containing the stub installer are concatenated together. This results in the final stub installer.


If this is an official build running on Mozilla automation infrastructure, then after this the installers will be signed, like other build products. Release engineering owns that process, it's not within the scope of this documentation.

