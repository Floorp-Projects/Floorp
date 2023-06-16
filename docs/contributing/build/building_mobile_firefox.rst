Firefox for Mobile Devices
--------------------------

We have several different mobile products aimed at different tasks,
devices, and audiences:

-  Building **Firefox for Android** (codename: fenix). Our general-purpose
   mobile browser is split into several different artifact layers:

  - `The fenix Android application </mobile/android/fenix.html>`_
  - `The android-components Android library <https://github.com/mozilla-mobile/firefox-android/tree/main/android-components>`_
  - `The GeckoView platform </mobile/android/geckoview>`_

-  `Firefox for iOS <https://github.com/mozilla-mobile/firefox-ios>`_,
   our general-purpose browser for iOS with desktop sync built-in.
-  Building **Firefox Focus**, our privacy-focused browser for

  - `iOS <https://github.com/mozilla-mobile/focus-ios>`_
  - `Android <https://github.com/mozilla-mobile/firefox-android/tree/main/focus-android>`_. This browser
    also uses the android-components library and GeckoView platform, like Firefox for Android

For both Desktop and Mobile development, please bear the following in
mind:

-  While you can build Firefox on older hardware it can take quite a bit
   of time to compile on slower machines. Having at least 8GB of RAM is
   recommended, and more is always better. The build process is both CPU
   and I/O intensive, so building on a machine with an SSD is also
   strongly preferred.
-  Fast broadband internet is strongly recommended as well. Both the
   development environment and the source code repository are quite
   large.
-  Though you can build Firefox to run on 32-bit machines, the build
   process for almost all of our products requires a 64-bit OS.
