============================
Stub Installer Configuration
============================

The stub installer doesn't offer many options to the user, most of its configuration is automatic and implicit. But there are two things the user can control, and here we'll discuss those and the automated settings.

Architecture
------------
The stub installer automatically selects whether to install a 32-bit or a 64-bit build of Firefox. 64-bit will be selected only if these conditions all pass:

1. The operating system is a 64-bit build.
2. The system has enough physical memory installed. Currently an amount strictly greater than 2 GB is required (that is, exactly 2 GB is not considered enough).
3. No third-party software that is incompatible with the 64-bit build is present on the system.

If any of the above conditions is not satisfied, a 32-bit build is installed.

Scope
-----
We support creating the installation in either a machine or a per-user scope. This affects whether application files, shortcuts, and registry entries are created in locations that are accessible to any user on the machine or locations that are specific to a particular user. Even in the full installer there is no UI for configuring which scope is selected; instead both installers automatically select a scope based on whether they have the privileges needed to perform a machine scope installation. This means a user with those privileges can effectively control the scope by the selection they make in the Windows UAC prompt; rejecting it will mean that the installer doesn't see that user as an administrator and will perform a per-user installation.

Install Location
----------------
The user doesn't have any control over the install location used by the stub. To determine the location that will be used, first the stub looks for any existing installations of the same channel of Firefox. If it finds one of those and the architecture of that installation is the same as the one that's been selected for the new one, then the path for the new install will be set to the same as that install, effectively overwriting it. This allows the stub to be used as a repair method for broken installations. If there isn't any existing installation fitting those criteria, then a hard-coded default is selected based on the architecture, scope, and channel.

Profile Cleanup
---------------
If the stub installer detects that this could be a helpful thing to do, it will start off the installation by offering to clean up the user's Firefox profile. This is the same cleanup that's triggered by the "refresh" button in about:support, and it's performed by the same Firefox code (the stub really just asks the browser to do the cleanup using a command-line parameter, it doesn't try to perform the operation itself). The user can control whether this cleanup is done by toggling a checkbox that's shown before the installation begins.

There are two variations of the profile cleanup prompt that can be shown to the user, one that appears if the new installation would overwrite an existing one (the "paveover" prompt) and another if it would be a new installation (the "reinstall" prompt). The only difference is the text that the user sees, the two function identically.

This is the procedure that determines when one of the two cleanup prompts is shown:

1. Look for the current default Firefox profile. If there are no profiles or none is set as the default, don't show either prompt.
2. Look for an existing installation by searching the registry for any copies of Firefox that are registered for potential file type associations. If none exist, show the reinstall prompt.
3. Check if the existing installation is for the same channel that's being installed now. If not, don't show either prompt.
4. Check the version of Firefox that the default profile we found in step 1 was last used with. This information comes from the profile's compatibility.ini file. If that version is more than 2 versions behind the current version, show the paveover prompt. Otherwise, don't show either prompt. Information about the current version is taken from `<https://product-details.mozilla.org/1.0/firefox_versions.json>`_.
