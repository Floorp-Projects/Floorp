Building Firefox 32-bit On Linux 64-bit
=======================================

.. note::

   Unless you really want to target older hardware, you probably
   want to :ref:`Build Firefox 64-bit <Building Firefox On Linux>`
   since it is better-supported.

Before following these 32-bit-Firefox-specific instructions, follow
the :ref:`Building Firefox On Linux` instructions to ensure that
your machine can do a normal build.

Instructions for Ubuntu 19.10
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

These steps were verified to work as of June 2020.

#. Run ``rustup target install i686-unknown-linux-gnu`` to install the
   32-bit Rust target.
#. Install 32-bit dependencies with the following command (this shouldn't try to
   remove packages. If this is the case, those instructions won't work as-is):

  .. code::

     sudo apt install gcc-multilib g++-multilib \
       libgtk2.0-dev:i386 libgtk-3-dev:i386 libpango1.0-dev:i386 libxt-dev:i386 \
       libx11-xcb-dev:i386 libpulse-dev:i386 libdrm-dev:i386

#. Then, create a ``mozconfig`` file in your Firefox code directory
   (probably ``mozilla-unified``) that looks like the following example:

   .. code::

      ac_add_options --target=i686

#. Run ``./mach build``.
