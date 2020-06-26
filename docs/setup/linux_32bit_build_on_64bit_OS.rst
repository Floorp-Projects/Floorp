Building Firefox 32bit On Linux 64bit
=====================================

Instructions for Fedora 20 and 19
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

First ensure that your compiler toolchain and Gecko build dependencies
are installed.

.. code:: brush:

   sudo yum install \
     ccache cmake gcc gcc-c++ glibc-devel.i686 \
     libstdc++-devel libstdc++-devel.i686 \
     autoconf213 \
     gtk2-devel.i686 gtk+-devel.i686 gtk+extra-devel.i686 \
     glib-devel.i686 glib2-devel.i686 \
     dbus-devel.i686 dbus-glib-devel.i686 \
     alsa-lib-devel.i686 yasm-devel.i686 \
     gstreamer-devel.i686 gstreamer-plugins-devel.i686 \
     gstreamer-plugins-base-devel.i686 \
     libxml2-devel.i686 zlib-devel.i686 \
     freetype-devel.i686 \
     atk-devel.i686 pango-devel.i686 fontconfig-devel.i686 \
     cairo-devel.i686 gdk-pixbuf2-devel.i686 \
     libX11-devel.i686 libXt-devel.i686 libXext-devel.i686 \
     libXrender-devel.i686 libXau-devel.i686 libxcb-devel.i686 \
     pulseaudio-libs-devel.i686 harfbuzz-devel.i686 \
     mesa-libGL-devel.i686 libXxf86vm-devel.i686 \
     libXfixes-devel.i686 libdrm-devel-2.4.49-2.fc19.i686 \
     mesa-libEGL-devel libXdamage-devel.i686 libXcomposite-devel.i686

Then you need to use a .mozconfig that looks like the following example.

.. code:: brush:

   # Flags set for targeting x86.
   export CROSS_COMPILE=1
   export PKG_CONFIG_PATH=/usr/lib/pkgconfig

   CC="ccache gcc -m32"
   CXX="ccache g++ -m32"
   AR=ar
   ac_add_options --target=i686-pc-linux

   # Normal build flags.  These make a debug browser build.
   ac_add_options --enable-application=browser
   mk_add_options MOZ_MAKE_FLAGS="-s -j6"
   mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/../ff-dbg

   ac_add_options --enable-debug
   ac_add_options --disable-optimize

Instructions for Ubuntu 18.10
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

.. warning::

   WARNING: Although Ubuntu and other Debian-derived distributions
   support multiarch packages these days, there are still cases where
   packages are broken, or where the 32-bit and 64-bit versions of a
   package conflict. This makes it difficult to impossible to have a
   32-bit development environment co-exist with a 64-bit development
   environment. **So it is recommended to follow these instructions in a
   separate virtual machine!**

These steps were verified to work as of December 2018:

#. Set up a new virtual machine running Ubuntu 18.10.
#. Install python with ``sudo apt install python``.
#. Install mercurial or git and fetch a copy of mozilla-central.
#. Run ``./mach bootstrap`` to install some dependencies. Note that this
   will install some amd64 packages that are not needed to build 32-bit
   Firefox.
#. Run ``rustup target install i686-unknown-linux-gnu`` to install the
   32-bit Rust target.
#. Install 32-bit dependencies with the following command (this may
   uninstall some unneeded/conflicting packages that were installed by
   mach bootstrap):

.. code:: syntaxbox

   sudo apt install gcc-multilib g++-multilib libdbus-glib-1-dev:i386 \
     libgtk2.0-dev:i386 libgtk-3-dev:i386 libpango1.0-dev:i386 libxt-dev:i386

7. Create a ``.mozconfig`` file containing at least the following:

.. code:: syntaxbox

   ac_add_options --target=i686-pc-linux

   CFLAGS="$CFLAGS -march=pentium-m -msse -msse2 -mfpmath=sse"
   CXXFLAGS="$CXXFLAGS -march=pentium-m -msse -msse2 -mfpmath=sse"

   export PKG_CONFIG_PATH=/usr/lib/i386-linux-gnu/pkgconfig

8. Run ``./mach build``.

Older, generic instructions for Ubuntu
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Method 1: True Cross-Compiling
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

This method is actually cross-compiling: you take a 64-bit toolchain and
produce 32-bit binaries. This is ideally how you cross-compile.

#. ``sudo apt-get install ia32-libs gcc-multilib g++-multilib lib32*``
#. (I had to open Synaptic afterwards, search for package names
   beginning with lib32, and install the ones that apt-get missed)
#. Use a ``.mozconfig`` like below.
#. ``make -f client.mk build``

::

   export CROSS_COMPILE=1
   mk_add_options MOZ_OBJDIR=@TOPSRCDIR@/objdir-ff-dbg32
   mk_add_options MOZ_MAKE_FLAGS="-s -j4"
   ac_add_options --enable-application=browser

   CC="gcc -m32"
   CXX="g++ -m32"
   AR=ar
   ac_add_options --x-libraries=/usr/lib32
   ac_add_options --target=i686-pc-linux

   ac_add_options --disable-crashreporter # needed because I couldn't find a 32-bit curl-dev lib
   ac_add_options --disable-libnotify     # needed because I couldn't find a 32-bit libinotify-dev
   ac_add_options --disable-gnomevfs      # needed because I couldn't find a 32-bit libgnomevfs-dev
   ac_add_options --disable-gstreamer     # needed because I couldn't find a 32-bit gstreamer lib

If you are getting an error as follows:

::

   error: Can't find header fontconfig/fcfreetype.h

Add these configuration options to your ``.mozconfig``:

::

   ac_add_options --disable-freetypetest
   ac_add_options --disable-pango

Method 2: Create a 32-bit chroot Environment
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In this method, we effectively create a wholly-contained 32-bit
operating system within a 64-bit operating system using ``schroot``.
This isn't technically cross-compiling, but it yields the same result:
32-bit binaries.

This method is arguably more reliable than true cross-compiling because
the newly-created environment is completely isolated from the 64-bit
operating system and it won't be susceptible to common issues with
cross-compiling, such as unavailability of 32-bit libraries/packages
when running in 64-bit mode. Additionally, since your 32-bit environment
is completely isolated, to clean up from it, you just ``rm -rf`` the
chroot directory. Contrast this with removing dozens of 32-bit packages
from your primary operating system.

The downside to this method is size and complexity. Since you will be
effectively creating a whole operating system within your primary
operating system, there will be lots of redundant files. You'll probably
need at least 1GB for all the new files. Additionally, the steps for
initially creating the 32-bit environment are more involved. See the
bottom of this page for a script capable of automating the whole process
of cross-compilation.

To create a 32-bit chroot Ubuntu environment, follow the
`DebootstrapChroot <https://help.ubuntu.com/community/DebootstrapChroot>`__
instructions. Here is an example config file which works in Ubuntu
13.10:

::

   # /etc/schroot/chroot.d/saucy_i386

   [saucy_i386]
   description=Ubuntu 13.10 for i386
   directory=/srv/chroot/saucy_i386
   root-users=gps
   type=directory
   personality=linux32
   users=gps

Once you have changed the ``root-users`` and ``users`` entries to
include your username and verified that ``$ schroot -c saucy_i386``
works, ``$ exit`` back to your regular operating system and copy your
APT's sources list to the new environment:

::

   $ sudo cp /etc/apt/sources.list /srv/chroot/saucy_i386/etc/apt/sources.list

**Note: this assumes a generic sources list. If you have modified this
file yourself, you may wish to ensure the contents are accurate when you
perform the copy.**

The reason we copy the APT sources is because ``debootstrap``\ does not
appear to configure all the sources by default (it doesn't define the
"sources" sources, for example).

Once your sources list is copied over, enter your new environment and
configure things:

::

   # Update the APT sources and install sudo into the new environment and exit back out
   $ schroot -c saucy_i386 -u root
   (precise_i386) # apt-get update
   (precise_i386) # apt-get install sudo
   (precise_i386) # exit

   # re-enter the environment as a regular user
   $ schroot -c saucy_i386

   # Install Firefox build dependencies
   $ sudo apt-get build-dep firefox

Now, your new 32-bit operating system should be ready for building
Firefox!

| One last step is ensuring that ``configure``\ detects the proper
  system type. Since you are technically running on a 64-bit kernel,
  things could still be fooled.
|  Run the following program from your mozilla source tree:

::

   $ ./build/autoconf/config.guess

**If this prints anything with ``x86_64``, the system type is being
incorrectly detected and you must override it.** You can fix things by
adding the following to your ``mozconfig``:

::

   ac_add_options --host=i686-pc-linux-gnu
   ac_add_options --target=i686-pc-linux-gnu

When you run configure (``$ mach configure``), verify that the host,
target, and build system types are what you just defined in your
``mozconfig``:

::

   checking host system type... i686-pc-linux-gnu
   checking target system type... i686-pc-linux-gnu
   checking build system type... i686-pc-linux-gnu

If you intend to run the 32bits Firefox build in the chroot on the
64bits machine, you need to install a few packages in the host:

::

   sudo apt-get install gcc-multilib g++-multilib libxrender1:i386 libasound-dev:i386 libdbus-glib-1-2:i386 libgtk2.0-0:i386 libxt-dev:i386

Now, follow the `build
instructions </En/Developer_Guide/Build_Instructions>`__ like normal and
you should have 32-bit builds!

See Also
~~~~~~~~

-  `Cross-Compiling Mozilla </en/Cross-Compiling_Mozilla>`__
-  `Set of scripts to automate the chroot and build process on
   Ubuntu <https://github.com/padenot/fx-32-on-64.sh>`__
