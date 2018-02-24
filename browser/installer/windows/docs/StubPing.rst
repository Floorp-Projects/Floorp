=========
Stub Ping
=========

When the stub installer completes with almost any result [1]_, it generates a ping containing some data about the system and about how the installation went. This ping isn't part of Firefox unified telemetry, it's a bespoke system; we can't use the telemetry client code when it isn't installed yet.


Querying the stub ping
----------------------

The stub ping data is available (to those with a Mozilla LDAP login) on `Redash <https://sql.telemetry.mozilla.org>`_. Make sure to select the ``DSMO-RS`` data source. The ``download_stats`` table is the main data table. It contains the columns in the following list.

Some of the columns are marked [DEPRECATED] because they involve features that were removed when the stub installer was streamlined in Firefox 55. These columns were not removed to keep compatibility and so we could continue to use the old data, but they should no longer be used.

timestamp
  Time the ping was received

build_channel
  Channel the installer was built with the branding for ("release", "beta", "nightly", or "default")

update_channel
  Value of MOZ_UPDATE_CHANNEL for the installer build; should generally be the same as build_channel

version
  Version number of the installed product, or 0 if the installation failed. This is **not** the version of the installer itself.

build_id
  Build ID of the installed product, or 0 of the installation failed

locale
  Locale of the installer and of the installed product, in AB_CD format

amd64_bit_build
  True if a 64-bit build was selected for installation. This means the OS is 64-bit, the RAM requirement was met, and no third-party software that blocks 64-bit installations was found.

amd64bit_os
  True if the version of Windows on the machine was 64-bit

os_version
  Version number of Windows in ``major.minor.build`` format [2]_

service_pack
  Latest Windows service pack installed on the machine

server_os
  True if the installed OS is a server version of Windows

admin_user
  True if the installer was run by a user with administrator privileges (and the UAC prompt was accepted)

default_path
  [DEPRECATED] True if the default installation path was not changed. We no longer support changing this in the streamlined stub, so this should always be true once `bug 1351697 <https://bugzilla.mozilla.org/show_bug.cgi?id=1351697>`_ is fixed

set_default
  [DEPRECATED] True if the option to set the new installation as the default browser was left selected. We no longer attempt to change the default browser setting in the streamlined stub, so this should always be false.

new_default
  [DEPRECATED] True if the new installation was successfully made the default browser. We no longer attempt to change the default browser setting in the streamlined stub, so this should always be false.

old_default
  True if an existing installation of Firefox was already set as the default browser

had_old_install
  True if at least one existing installation of Firefox was found on the system prior to this installation

old_version
  Version of the previously existing Firefox installation, if any

old_build_id
  Build ID of the previously existing Firefox installation, if any

bytes_downloaded
  Size of the full installer data that was transferred before the download ended (whether it failed, was canceled, or completed normally)

download_size
  Expected size of the full installer download according to the HTTP response headers

download_retries
  Number of times the full installer download was retried or resumed. 10 retries is the maximum.

download_time
  Number of seconds spent downloading the full installer

download_latency
  Seconds between sending the full installer download request and receiving the first response data

download_ip
  IP address of the server the full installer was download from (can be either IPv4 or IPv6)

manual_download
  True if the user clicked on the button that opens the manual download page. The prompt to do that is shown after the installation fails or is canceled.

intro_time
  [DEPRECATED] Seconds the user spent on the intro screen. The streamlined stub no longer has this screen, so this should always be 0.

options_time
  [DEPRECATED] Seconds the user spent on the options screen. The streamlined stub no longer has this screen, so this should always be 0.

download_phase_time
  Seconds spent in the download phase; should be very close to download_time, since nothing else happens in this phase.

preinstall_time
  Seconds spent verifying the downloaded full installer and preparing to run it

install_time
  Seconds the full installer ran for

finish_time
  Seconds spent waiting for the installed application to launch

succeeded
  True if a new installation was successfully created. False if that didn't happen for any reason, including when the user closed the installer window.

disk_space_error
  [DEPRECATED] True if the installation failed because the drive we're trying to install to does not have enough space. The streamlined stub no longer sends a ping in this case, because the installation drive can no longer be selected.

no_write_access
  [DEPRECATED] True if the installation failed because the user doesn't have permission to write to the path we're trying to install to. The streamlined stub no longer sends a ping in this case, because the installation drive can no longer be selected.

download_cancelled
  True if the installation failed because the user closed the window during the download.

out_of_retries
  True if the installation failed because the download had to be retried too many times (currently 10)

file_error
  True if the installation failed because the downloaded file couldn't be read from

sig_not_trusted
  True if the installation failed because the signature on the downloaded file wasn't valid and/or wasn't signed by a trusted authority

sig_unexpected
  True if the installation failed because the signature on the downloaded file didn't have the expected subject and issuer names

install_timeout
  True if the installation failed because running the full installer timed out. Currently that means it ran for more than 150 seconds for a new installation, or 165 seconds for a paveover installation.

new_launched
  True if the installation succeeded and we were able to launch the newly installed application.

old_running
  True if the installation succeeded and we weren't able to launch the newly installed application because a copy of Firefox was already running.

attribution
  Any attribution data that was included with the installer

profile_cleanup_prompt
  0: neither profile cleanup prompt was shown

  1: the "reinstall" version of the profile cleanup prompt was shown (no existing installation was found, but the user did have an old Firefox profile)

  2: the "paveover" version of the profile cleanup prompt was shown (an installation of Firefox was already present, but it's an older version)

profile_cleanup_requested
  True if either profile cleanup prompt was shown and the user accepted the prompt


.. [1] No ping is sent if the installer exits early because initial system requirements checks fail.
.. [2] Previous versions of Windows have used a very small set of build numbers through their entire lifecycle. However, Windows 10 gets a new build number with every major update (about every 6 months), and many more builds have been released on its insider channels. So, to prevent a huge amount of noise, queries using this field should generally filter out the build number and only use the major and minor version numbers to differentiate Windows versions, unless the build number is specifically needed.
