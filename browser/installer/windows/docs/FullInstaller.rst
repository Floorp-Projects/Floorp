==============
Full Installer
==============

The full installer is actually responsible for installing the browser; it's what the stub launches in order to do the "real" installing work, but it's also available separately. It uses a traditional "wizard" interface design, as is (somewhat) natively supported by NSIS. It can also be :doc:`configured <FullConfig>` to launch in a silent mode, suitable for scripting or managed deployments.

The full installer's main script is `installer.nsi <https://searchfox.org/mozilla-central/source/browser/installer/windows/nsis/installer.nsi>`_, but most of the heavy lifting is done by the shared functions in `common.nsh <https://searchfox.org/mozilla-central/source/toolkit/mozapps/installer/windows/nsis/common.nsh>`_.

If it was not launched by the :doc:`StubInstaller`, an :ref:`Install Ping` is sent when the installer exits.

The installer writes ``installation_telemetry.json`` to the install location, this is read by Firefox in order to send a telemetry event, see the event definition in `Events.yaml <https://searchfox.org/mozilla-central/source/toolkit/components/telemetry/Events.yaml>`_ (category ``installation``, event name ``first_seen``) for a description of the properties. There is also an ``install_timestamp`` property, which is saved in the profile to determine whether there has been a new installation; this is not sent as part of the ping.

.. toctree::
   FullConfig
