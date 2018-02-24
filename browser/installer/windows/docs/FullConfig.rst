============================
Full Installer Configuration
============================

Command-line Options
--------------------

The full installer provides a number of options that can be used either from the GUI or from silent mode. It accepts these two command-line options:

``/S``
  Silent installation. This option doesn't open the GUI, instead running the installation in the background using all the default settings. It's useful as part of a script for configuring a new system, for example.

  For backwards compatibility, this option can also be spelled ``-ms``.

``/INI=[absolute path to .ini file]``
  Read configuration from an .ini file. Also triggers silent mode, but instead of using all the default settings, allows you to control them by providing a settings file.


INI File Settings
-----------------

All of these settings should be placed into one section, called ``[Install]``, in the standard INI syntax. All of them are optional; they can be included or left out in any combination. Order does not matter.

``InstallDirectoryPath``
  Absolute path specifying the complete install location. This directory does not need to exist already (but it can).

  If ``InstallDirectoryName`` is set, then this setting will be ignored.

``InstallDirectoryName``
  Name of the installation directory to create within Program Files. For example, if ``InstallDirectoryName`` is set to ``Firefox Release``, then the installation path will be something like ``C:\Program Files\Firefox Release``. The Program Files path used will be the correct one for the architecture of the application being installed and the locale/configuration of the machine; this setting is mainly useful to keep you from having to worry about those differences.

  If this is set, then ``InstallDirectoryPath`` will be ignored.

``TaskbarShortcut``
  Set to ``false`` to disable pinning a shortcut to the taskbar. ``true`` by default. This feature only works on Windows 7 and 8; it isn't possible to create taskbar pins from the installer on later versions.

``DesktopShortcut``
  Set to ``false`` to disable creating a shortcut on the desktop. ``true`` by default.

``StartMenuShortcuts``
  Set to ``false`` to disable creating a Start menu shortcut. ``true`` by default. The name is historical; only one shortcut is ever created in the Start menu.

``MaintenanceService``
  Set to ``false`` to disable installing the Mozilla Maintenance Service. This will effectively prevent users from installing Firefox updates if they do not have write permissions to the installation directory. ``true`` by default.

``RemoveDistributionDir``
  Set to ``false`` to disable removing the ``distribution`` directory from an existing installation that's being paved over. By default this is ``true`` and the directory is removed.

``PreventRebootRequired``
  Set to ``true`` to keep the installer from taking actions that would require rebooting the machine to complete, normally because files are in use. This should not be needed under normal circumstances because no such actions should be required unless you're paving over a copy of Firefox that was running while the installer was trying to run, and setting this option in that case may result in an incomplete installation. ``false`` by default.

``OptionalExtensions``
  Set to ``false`` to disable installing any bundled extensions that are present. Normally none of these exist, except in special distributions of Firefox such as the one produced by Mozilla China or by other partner organizations. ``true`` by default.


Example INI File
~~~~~~~~~~~~~~~~
::

  [Install]
  ; Semicolons can be used to add comments
  InstallDirectoryName=Firefox Release
  DesktopShortcut=false
  MaintenanceService=false
  OptionalExtensions=false
