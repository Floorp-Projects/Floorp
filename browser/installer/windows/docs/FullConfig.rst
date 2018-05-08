============================
Full Installer Configuration
============================

Command-line Options
--------------------

The full installer provides a number of options that can be used either from the GUI or from the command line. The following command line options are accepted.

The presence of **any** command-line option implicitly enables silent mode (see the ``/S`` option).

Each option must start with a ``/`` as shown, ``-`` or ``--`` are not supported. Short names for the options are not provided; all names must be spelled out.

For options that accept ``true`` or ``false``, ``=true`` can be left off to get the same effect. That is, ``/DesktopShortcut`` and ``/DesktopShortcut=true`` both enable the desktop shortcut.

``/S``
  Silent installation. This option doesn't open the GUI, instead running the installation in the background using all the default settings. It's useful as part of a script for configuring a new system, for example.

  For backwards compatibility, this option can also be spelled ``-ms``.

``/InstallDirectoryPath=[path]``
  Absolute path specifying the complete install location. This directory does not need to exist already (but it can).

  If ``InstallDirectoryName`` is set, then this setting will be ignored.

``/InstallDirectoryName=[name]``
  Name of the installation directory to create within Program Files. For example, if ``InstallDirectoryName`` is set to ``Firefox Release``, then the installation path will be something like ``C:\Program Files\Firefox Release``. The Program Files path used will be the correct one for the architecture of the application being installed and the locale/configuration of the machine; this setting is mainly useful to keep you from having to worry about those differences.

  If this is set, then ``InstallDirectoryPath`` will be ignored.

``/TaskbarShortcut={true,false}``
  Set to ``false`` to disable pinning a shortcut to the taskbar. ``true`` by default. This feature only works on Windows 7 and 8; it isn't possible to create taskbar pins from the installer on later Windows versions.

``/DesktopShortcut={true,false}``
  Set to ``false`` to disable creating a shortcut on the desktop. ``true`` by default.

``/StartMenuShortcut={true,false}``
  Set to ``false`` to disable creating a Start menu shortcut. ``true`` by default.

  For backwards compatibility, this option can also be spelled ``/StartMenuShortcuts`` (plural), however only one shortcut is ever created in the Start menu per installation.

``/MaintenanceService={true,false}``
  Set to ``false`` to disable installing the Mozilla Maintenance Service. This will effectively prevent users from installing Firefox updates if they do not have write permissions to the installation directory. ``true`` by default.

``/RemoveDistributionDir={true,false}``
  Set to ``false`` to disable removing the ``distribution`` directory from an existing installation that's being paved over. By default this is ``true`` and the directory is removed.

``/PreventRebootRequired={true,false}``
  Set to ``true`` to keep the installer from taking actions that would require rebooting the machine to complete, normally because files are in use. This should not be needed under normal circumstances because no such actions should be required unless you're paving over a copy of Firefox that was running while the installer was trying to run, and setting this option in that case may result in an incomplete installation. ``false`` by default.

``/OptionalExtensions={true,false}``
  Set to ``false`` to disable installing any bundled extensions that are present. Normally none of these exist, except in special distributions of Firefox such as the one produced by Mozilla China or by other partner organizations. ``true`` by default.

``/INI=[absolute path to .ini file]``
  Read configuration from an .ini file. All settings should be placed into one section, called ``[Install]``, and use the standard INI syntax. All settings are optional; they can be included or left out in any combination. Order does not matter.

  The available settings in the .ini file are the same as the command line options, except for ``/S`` and ``/INI`` (of course). They should be set with the same syntax described above for command line use.

  For any option provided both in an .ini file and on the command line, the value found on the command line will be used. This allows command line options to override .ini settings.

  Here's an example of a valid .ini file for use with this option::

    [Install]
    ; Semicolons can be used to add comments
    InstallDirectoryName=Firefox Release
    DesktopShortcut=false
    StartMenuShortcuts=true
    MaintenanceService=false
    OptionalExtensions=false

``/ExtractDir=[directory]``
  Extract the application files to the given directory and exit, without actually running the installer. Of course, this means all other options will be ignored.

  This option is not available for use in .ini files.
