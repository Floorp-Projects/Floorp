===========
MSI Package
===========

We provide a `Windows Installer <https://wikipedia.org/wiki/Windows_Installer>`_ MSI package for the convenience of administrators deploying Firefox in their organizations. The primary documentation for how to obtain and use these packages is hosted on `the Mozilla support web site <https://support.mozilla.org/kb/deploy-firefox-msi-installers>`_.

Our MSI packages are not "true" Windows Installer packages; they don't actually contain any installable components and don't register any product. They simply wrap the :doc:`full installer <FullInstaller>`, which performs the installation just as if it had been run normally. The main reason for this is the difficulty of adapting Firefox's existing update mechanisms to fit the way that Windows Installer would expect things to be done.

The MSI package is built for every release along with the other package formats using the command ``./mach repackage msi``. That command invokes the `WiX tools <https://wixtoolset.org/>`_ to build the package from `a normal WiX XML file <https://searchfox.org/mozilla-central/source/browser/installer/windows/msi/installer.wxs>`_, after filling in a few variables to customize the package for that particular Firefox build. The majority of the WiX file is concerned with passing parameters through from MSI properties to command-line arguments for the full installer to use; the rest is boilerplate to satisfy the minimum requirements for something that WiX will build and the Windows Installer system will load and run without errors.
