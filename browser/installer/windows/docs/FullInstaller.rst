==============
Full Installer
==============

The full installer is actually responsible for installing the browser; it's what the stub launches in order to do the "real" installing work, but it's also available separately. It uses a traditional "wizard" interface design, as is (somewhat) natively supported by NSIS. It can also be :doc:`configured <FullConfig>` to launch in a silent mode, suitable for scripting or managed deployments.

The full installer's main script is `installer.nsi <https://searchfox.org/mozilla-central/source/browser/installer/windows/nsis/installer.nsi>`_, but most of the heavy lifting is done by the shared functions in `common.nsh <https://searchfox.org/mozilla-central/source/toolkit/mozapps/installer/windows/nsis/common.nsh>`_.

.. toctree::
   FullConfig
