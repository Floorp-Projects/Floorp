=========
Installer
=========

The main role of the Firefox installer is to get a user running Firefox as quickly and reliably as possible, while taking all necessary steps to make Firefox an integrated part of the system.

It turns out that the only platform where we need an installer in order to accomplish all of that is Windows. So we only develop and ship and installer for Windows, and on other platforms we distribute a package in a non-executable format typical of applications distributed on that platform.

Currently, the installers are built on the `NSIS <http://nsis.sourceforge.net/Main_Page>`_ installer framework. This is a Windows-only framework which compiles scripts written in its custom language (along with some native plugins) into an executable installer.

We build four different kinds of installers, the :doc:`Stub Installer <StubInstaller>`, the :doc:`Full Installer <FullInstaller>`, an :doc:`MSI package <MSI>` which wraps the full installer, and an :doc:`MSIX package <MSIX>`. The stub is the default installer intended for most individual users, the full installer and MSI are aimed at power users and administrators who need more control, and the MSIX is aimed at enterprise users who need to control deployment.

There's also another installer-related program, which is called :doc:`helper.exe <Helper>`. It's also written in NSIS and has a few different jobs that involve maintaining things that the installer sets up, including the uninstaller and a post-update routine.

.. toctree::
   InstallerBuild
   StubInstaller
   FullInstaller
   MSI
   MSIX
   Helper
