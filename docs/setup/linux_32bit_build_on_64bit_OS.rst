Building Firefox 32-bit On Linux 64-bit
=======================================

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

Instructions for Ubuntu 19.10
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These steps were verified to work as of June 2020

#. Install mercurial or git and :ref:`fetch a copy <Mercurial Bundles>` of ``mozilla-central``.
#. Run ``./mach bootstrap`` to install some dependencies. Note that this
   will install some amd64 packages that are not needed to build 32-bit
   Firefox.
#. Run ``rustup target install i686-unknown-linux-gnu`` to install the
   32-bit Rust target.
#. Install 32-bit dependencies with the following command (this shouldn't try to
   remove packages. If this is the case, those instructions won't work as-is):

  .. code:: syntaxbox

     sudo apt install gcc-multilib g++-multilib libdbus-glib-1-dev:i386 \
       libgtk2.0-dev:i386 libgtk-3-dev:i386 libpango1.0-dev:i386 libxt-dev:i386 \
       libx11-xcb-dev:i386 libpulse-dev:i386

5. Create a file called ``mozconfig`` in the top-level directory of you
   ``mozilla-central`` checkout, containing at least the following:

  .. code:: syntaxbox

     ac_add_options --target=i686

6. Run ``./mach build``.
