================================================================================

= = = = = = = = = = = = = =   Firefox Read Me    = = = = = = = = = = = = = = = =

================================================================================

Firefox is subject to the terms detailed in the license agreement
accompanying it.

This Read Me file contains information about system requirements and
installation instructions for the OS/2 build of Firefox.

For more info on Firefox, see http://www.mozilla.org/products/firefox/.
For more info on the OS/2 port see http://www.mozilla.org/ports/os2. To submit
bugs or other feedback check out Bugzilla at https://bugzilla.mozilla.org for
links to known bugs, bug-writing guidelines, and more. You can also get help
with Bugzilla by pointing your IRC client to #mozillazine at irc.mozilla.org,
OS/2 specific problems are discussed in #warpzilla and in the newsgroup
mozilla.dev.ports.os2 on news.mozilla.org.


================================================================================

                            Getting Firefox

================================================================================

Official Milestone builds of Firefox are published on the release page at

  http://www.mozilla.org/products/firefox/releases/

OS/2 releases are not created by Mozilla.org staff and may appear on the page
http://www.mozilla.org/ports/os2 before the releases page. Be sure to read the
Firefox release notes linked on the releases page for information on known
problems and installation issues with Firefox.


================================================================================

                        System Requirements on OS/2

================================================================================

- This release requires the C runtime DLLs (libc-0.6.3) from
  ftp://ftp.netlabs.org/pub/gcc/libc-0.6.3-csd3.zip
  in order to run.  You can unpack them in the same directory as the
  Firefox executable or somewhere else in your LIBPATH.

- Minimum hardware requirements
  + 400 MHz Pentium class processor
  + 128 MiB RAM plus 128 MiB free swap space
  + 40 MiB free harddisk space for installation
    plus storage space for disk cache
  + Graphics card and driver capable of displaying more than 256 colors

- Recommended hardware for acceptable performance
  + 1.0 GHz processor
  + 512 MiB RAM plus 128 MiB free swap space
    NOTE: Firefox's performance and stability increases the more physical
    RAM is available.

- Software requirements
  + Installation on a file system supporting long file names
    (i.e. HPFS or JFS but not FAT)
  + OS/2 Warp 4 with Fixpack 15 or later (Warp 3 may work but is unsupported)
  + MPTS version 5.3
  + TCP/IP version 4.1
  + INETVER: SOCKETS.SYS=5.3007, AFOS2.SYS=5.3001, AFINET.SYS=5.3006
    NOTE: Do not attempt to use MPTS & TCP/IP versions below these INETVER
    levels. Although Firefox may seem to start and run normally with older
    stacks, some features Firefox needs are not implemented correctly in
    older MPTS versions, which may result in crashes and data loss.

  + Convenience Pack 2 or eComStation 1.0 or later meet these requirements
    out of the box.

- Useful optional software
  The following packages enable additional features in Firefox. Download
  locations are listed below with a detailed description of the features.
  + Remote Workplace Server (RWS), version 0.8
  + Doodle's Screen Saver (DSSaver), version 1.8 or later

================================================================================

                          Installation Instructions

================================================================================

For all platforms, unpack into a clean (new) directory.  Installing on top of
previously released builds may cause problems with Firefox.

Note: These instructions do not tell you how to build Firefox.
For info on building the Firefox source, see

  http://www.mozilla.org/build/


OS/2 Installation Instructions
------------------------------

   On OS/2, Firefox does not have an installation program. To install it,
   download the .zip file and follow these steps:

     1. Click the "Zip" link on the site you're downloading Firefox from
     to download the ZIP package to your machine. This file is typically called
     firefox-x.x.x.en-US.os2.zip where the "x.x.x" is replaced by the Firefox
     version.

     2. Navigate to where you downloaded the file and unpack it using your
     favorite unzip tool.

     3. Keep in mind that the unzip process creates a directory "firefox"
     below the location you point it to, e.g.
        unzip firefox-3.0.en-US.os2.zip -d c:\firefox-3.0
     will unpack Firefox into c:\firefox-3.0\firefox.

     4. Make sure that you are _not_ unpacking over an old installation. This is
     known to cause problems.

     5. To start Firefox, navigate to the directory you extracted
     Firefox to, make sure that the C library DLLs are copied to the
     installation directory or installed in the LIBPATH, and then double-click
     the Firefox.exe object.


Running multiple versions concurrently
--------------------------------------

Because various members of the Mozilla family (i.e. Mozilla, Firefox,
Thunderbird, IBM Web Browser) may use different, incompatible versions of the
same DLL, some extra steps may be required to run them concurrently.

One workaround is the LIBPATHSTRICT variable. To run Firefox one can create
a CMD script like the following example (where an installation of Firefox
exists in the directory d:\internet\firefox is assumed):

   set LIBPATHSTRICT=T
   rem The next line may be needed when a different Mozilla program is listed in LIBPATH
   rem set BEGINLIBPATH=d:\internet\firefox
   rem The next line is only needed to run two different versions of Firefox
   rem set MOZ_NO_REMOTE=1
   d:
   cd d:\internet\firefox
   firefox.exe %1 %2 %3 %4 %5 %6 %7 %8 %9

Similarly, one can create a program object to start Firefox using the
following settings:

   Path and file name: *
   Parameters:         /c set LIBPATHSTRICT=T & .\firefox.exe "%*"
   Working directory:  d:\internet\firefox

(One might need to add MOZ_NO_REMOTE and/or BEGINLIBPATH as in the CMD script
above depending on the system configuration.)

Finally, the simplest method is to use the Run! utility by Rich Walsh that can
be found in the Hobbes Software Archive:

   http://hobbes.nmsu.edu/cgi-bin/h-search?key=Run!

Read its documentation for more information.


Separating profiles from installation directory
-----------------------------------------------

To separate the locations of the user profile(s) (containing the bookmarks and
all customizations) from the installation directory to keep your preferences in
the case of an update even when using ZIP packages, set the variable
MOZILLA_HOME to a directory of your choice. You can do this either in Config.sys
or in a script or using a program object as listed above. If you add

   set MOZILLA_HOME=f:\Data

the Firefox user profile will be created in "f:\Data\Mozilla\Firefox".

If you are migrating from Mozilla, Firefox's import routine will only find
the existing Mozilla profile data if MOZILLA_HOME is correctly set to point to
it.


Other important environment variables
-------------------------------------

There are a few enviroment variables that can be used to control special
behavior of Firefox on OS/2:

- set NSPR_OS2_NO_HIRES_TIMER=1
  This causes Firefox not to use OS/2's high resolution timer. Set this if
  other applications using the high resolution timer (multimedia apps) act
  strangely.

- set MOZ_NO_REMOTE=1
  Use this to run two instances of Firefox simultaneously (like e.g. debug
  and optimized version).

- set MOZ_NO_RWS=1
  Use this to disable Remote Workplace Server support (see below).

Find more information on this topic and other tips on
   http://www.os2bbs.com/os2news/Warpzilla.html


Support for WPS objects in the browser
--------------------------------------

Firefox can make use of Rich Walsh's Remote Workplace Server (RWS) library to
access Workplace Shell objects from the browser. This allows helper applications
for downloaded files to be selected based on their default WPS association. In
addition, the WPS icons of files will be displayed in the download dialog and in
directory views.

To enable this functionality, Firefox has to find the RWS DLLs. They have to be
located in a directory on the LIBPATH, in the Firefox directory, or already be
registered as a WPS class. For eComStation 2 this is already the case. If RWS
is not yet available on your system, download it from
   http://hobbes.nmsu.edu/cgi-bin/h-search?key=rws08dll

If RWS is found on the system, it is used by Firefox automatically. In case you
need to disable RWS support, create an environment variable MOZ_NO_RWS and set
it to 1.


Idle timer for internal cleanups
--------------------------------

If Doodle's Screen Saver (DSSaver) v1.8 or later is installed, Firefox can
determine how long the user has been inactive. If the user has been inactive
for a while, cleanup operations are performed (e.g. on the browsing history)
and reloads of "Live Bookmarks" are suspended.

DSSaver can be downloaded from
   ftp://ftp.netlabs.org/pub/dssaver/dssaver_v18.zip


Setting the OS/2 default browser
--------------------------------

The default web browser can be set using WPS URL objects.  The properties
notebook of every URL object contains a "Browser" tab where the browser
executable can be selected. Any changes to these settings will be stored and
reflected in all URL objects once the user presses the "Set Default" button.

Two small applications make this change a bit easier and provide options to also
set default programs for mail and news as well as internet chat services:

- ConfigApps
     http://hobbes.nmsu.edu/cgi-bin/h-search?key=configapps&sort=date

- Internet Application Integration
     http://7cities.net/~mckinnis/os2/


Known Problems of the OS/2 version
----------------------------------

Cross-platform problems are usually listed in the release notes of each
milestone release.

- Firefox will beep when copying more than 64 kB of text to the clipboard.
  This is to alert users that many applications (most notably the system
  editor, EPM, and applications running in VIO windows) cannot paste more
  than this.

- Firefox cannot make use of OS/2 fonts like WarpSans and others which
  are not available in Type1 or TrueType format. It is therefore
  recommended to install the "Workplace Sans" font from
     http://users.socis.ca/~ataylo00/creative/fonts/
  or
     http://hobbes.nmsu.edu/cgi-bin/h-search?key=wpsu_ttf
  which Firefox will use as a replacement of WarpSans.

Other known problems can be found by following the link "Current Open Warpzilla
Bugs" on the OS/2 Mozilla page <http://www.mozilla.org/ports/os2/>.
