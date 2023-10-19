MSIX Package
============

Firefox MSIX packages are full participants in the "modern" Windows
app packaging system.  They are distributed, installed, updated,
repaired, and uninstalled entirely using that system.  This gives
administrators lots of deployment options, and also grants complete
control over when and how application updates are rolled out
(Firefox's built-in updater is always fully disabled in MSIX
packages).  This stands in contrast to Firefox MSI packages, which
mostly work against the Windows Installer framework rather than with
it, and therefore are missing a lot of important functionality; for
example, tools that install MSI packages generally cannot uninstall
Firefox [#]_.  This means the MSIX package may well be the better
option for deploying to Windows 10 and up.

In automation
-------------

The ``repackage-msix`` and ``repackage-shippable-l10n-msix`` tasks
repackage the ZIP packages produced by signed build tasks into MSIX
packages. The ``shippable-l10n`` variants depend on Linux64 builds and
localization tasks to produce signed langpacks, which are incorporated
into the MSIX package as distribution extensions. (This is the same
approach taken by ``snap`` and ``flatpak`` Linux packages.)

The ``repackage-signing-msix`` and
``repackage-signing-shippable-l10n-msix`` tasks sign the MSIX packages
produced by the repackage tasks.

Signing in automation
~~~~~~~~~~~~~~~~~~~~~

MSIX packages are signed by the same certificates that sign binaries for
other jobs. In practice, this means that try builds are signed with the
```Mozilla Fake CA``
certificate `MozFakeCA_2017-10-13.cer <https://raw.githubusercontent.com/mozilla-releng/OpenCloudConfig/3493a608bf700b68a54ff2fd506f33373bb87a04/userdata/Configuration/Mozilla%20Maintenance%20Service/MozFakeCA_2017-10-13.cer>`__.
In order to install try builds locally, you must trust this certificate.
**For your own security, it's best to do this in Windows Sandbox or a
Virtual Machine**. To do so run the following in an elevated
(administrator) Powershell:

::

    $ Import-Certificate -FilePath "MozFakeCA_2017-10-13.cer" -Cert Cert:\LocalMachine\Root\
    ...
    Thumbprint                                Subject
    ----------                                -------
    FA056CEBEFF3B1D0500A1FB37C2BD2F9CE4FB5D8  CN=Mozilla Fake CA

The ``shippable-l10n`` MSIX variants incorporate signed langpacks. These
in turn are signed with the same certificate. Firefox knows about this
certificate but does not trust it by default. To trust it, set the
hidden Gecko boolean preference

::

    xpinstall.signatures.dev-root=true

Sadly, it's not possible to set preferences via a ``distribution.ini``
early enough to impact loading the signed langpacks (see `Bug
1721764 <https://bugzilla.mozilla.org/show_bug.cgi?id=1721764>`__), and
once the signed langpacks fail to load once, they will not be reloaded
(see `Bug
1721763 <https://bugzilla.mozilla.org/show_bug.cgi?id=1721763>`__). This
make testing the first-run experience challenging. What can be done is
to install the MSIX package (perhaps using
``Add-AppxPackage -Path ...``) and determine the profile directory
(using ``about:support``). Uninstall the MSIX package (perhaps using
``Get-AppxPackage | Where -Property Name -like "Mozilla.*" | Remove-AppxPackage``).
Delete the contents of the profile directory entirely, but add a file
``user.js`` containing:

::

    user_pref("xpinstall.signatures.dev-root", true);
    user_pref("extensions.logging.enabled", true);

Reinstall the MSIX package and the signed langpacks should now be loaded
(slowly!) and available after first startup.

Local developer builds
----------------------

``mach repackage msix`` lets you repackage a Firefox package (or
directory) into an MSIX/App Package. The main complication is that an
App Package contains channel-specific paths and assets, and therefore
needs to be branding-aware, much as an Android package needs to be
branding-aware.

Usage
~~~~~

The tool is designed to repackage ZIP archives produced in automation.
Start looking for official builds at locations like:

==========    ==========================================================================================================================
Channel       URL
==========    ==========================================================================================================================
Release       https://archive.mozilla.org/pub/firefox/candidates/88.0.1-candidates/build1/win64/en-US/firefox-88.0.1.zip
Beta          https://archive.mozilla.org/pub/firefox/candidates/89.0b15-candidates/build1/win64/en-US/firefox-89.0b15.zip
Devedition    https://archive.mozilla.org/pub/devedition/candidates/89.0b15-candidates/build1/win64/en-US/firefox-89.0b15.zip
Nightly       https://archive.mozilla.org/pub/firefox/nightly/2021/05/2021-05-21-09-57-54-mozilla-central/firefox-90.0a1.en-US.win64.zip
==========    ==========================================================================================================================

Repackage using commands like:

::

    $ ./mach repackage msix \
      --input firefox-88.0.1.zip \
      --channel=official \
      --arch=x86_64 \
      --verbose

Or package a local developer build directly with ``mach repackage msix``:

::

    $ ./mach repackage msix

This command will do its best to guess your channel and other necessary
information. You can override these with options like ``--channel``
(see the ``--help`` text for all supported options).

Paths to tools can be set via environment variables. In order, searched
first to searched last:

1. the tool name, like ``MAKEAPPX`` or ``SIGNTOOL``
2. searching on ``PATH``
3. searching under ``WINDOWSSDKDIR``
4. searching under ``C:/Program Files (x86)/Windows Kits/10``

If you are cross compiling from Linux or macOS you will need a
compiled version of `Mozilla's fork of Microsoft's msix-packaging
<https://github.com/mozilla/msix-packaging/tree/johnmcpms/signing>`__
tools.

Linux users can obtain a prebuilt version with:

::

    $ ./mach artifact toolchain --from-build linux64-msix-packaging

After `bug 1743036 <https://bugzilla.mozilla.org/show_bug.cgi?id=1743036>`__
is fixed, macOS and Windows users will have a similar option.

Signing locally
~~~~~~~~~~~~~~~

The repackaged MSIX files produced are not signed by default. In
automation, Mozilla's signing service signs the repackaged MSIX files.
For local testing, you can sign them with a self-signed certificate by
adding ``--sign`` to ``mach repackage msix``, or with commands like:

::

    $ ./mach repackage sign-msix --input test.msix --verbose

Or sign them yourself following `Microsoft's self-signed certificate
instructions <https://docs.microsoft.com/en-us/windows/msix/package/create-certificate-package-signing#create-a-self-signed-certificate>`__.

Signing Certificates
^^^^^^^^^^^^^^^^^^^^

Mach will create the necessary signing keys and certificates for you
and reuse them for subsequent signings. Before your locally signed
builds can be installed you will need to install the correct
certificate to the Windows Root Store. This can be done with a command
like:

::

    $ powershell -c 'Import-Certificate -FilePath mycert.cer -Cert Cert:\LocalMachine\Root\'

The exact command to run will be shown if you run ``./mach repackage``
with ``--verbose``.

You _may_ choose to sign in a different manner, with a key and certificate
you create yourself, but Windows requires that the Subject of the certificate
match the Publisher found in the MSIX's AppxManifest.xml. If you choose
to go this route, ensure that you pass ``--publisher`` to
``./mach repackage msix`` to set that correctly.

For developers
~~~~~~~~~~~~~~

Updating the MSIX template
^^^^^^^^^^^^^^^^^^^^^^^^^^

MSIX is an "open format" in one sense: the MSIX container format is
specified at https://github.com/Microsoft/msix-packaging. It is
categorically *not* an open format in another sense: many of the
contained files are proprietary binary formats (``.reg`` -- registry
hive files) or undocumented (``.pri`` files -- resource index files).

Generally the MSIX packaging code tries to avoid requiring such files
that can't be built from sources. Where they are truly required, it
tries to use a single such file independent of branding and other
configuration, checked into the source tree.

resources.pri
'''''''''''''

Generate a new ``resources.pri`` file on a Windows machine using
``makepri.exe`` from the Windows SDK, like:

::

    $ makepri.exe new \
        -IndexName firefox \
        -ConfigXml browser/installer/windows/msix/priconfig.xml \
        -ProjectRoot browser/branding/official/msix \
        -OutputFile browser/installer/windows/msix/resources.pri \
        -Overwrite

The choice of channel (i.e.,
``browser/branding/{official,aurora,beta,nightly,unofficial}``) should
not matter.

.. [#] The MSI has to be limited in this way because of the difficulty
       of migrating existing installations into MSI and adding support
       for it to Firefox's update pipeline. MSIX does not have these
       constraints, because the partially virtualized file system that
       these kinds of apps run in makes install migration impossible
       and unnecessary.
