.. _updatingmacicons:

====================
Updating macOS Icons
====================

macOS icons are stored as ``icns`` files that contain the same logo in multiple different sizes and DPIs. Apple's `Human Interface Guidelines <https://developer.apple.com/design/human-interface-guidelines/foundations/app-icons>`_ should be consulted for the specifics.

Although it may seem like we can simply be handed the highest resolution/DPI version and downscale for the remainder -- this is not the case, some finer details in the icons (most notably shadows) need to be tweaked for each size. The UX team should hand off PNGs for every size and DPI needed.

Once those are in hand, the ``icns`` file can be created with something like the following:

::

    mkdir firefox.iconset
    mv icon_16x16.png firefox.iconset
    mv icon_32x32.png firefox.iconset
    mv icon_32x32@2x.png firefox.iconset
    mv icon_64x64@2x.png firefox.iconset
    mv icon_128x128.png firefox.iconset
    mv icon_256x256 firefox.iconset
    mv icon_256x256@2x.png firefox.iconset
    mv icon_512x512.png firefox.iconset
    mv icon_512x512@2x.png firefox.iconset
    mv icon_1024x1024@2x.png firefox.iconset
    iconutil -c icns firefox.iconset


(The ``NxN`` part is obviously the resolution, and the ``@2x`` string is used in the high DPI versions.)

This will create a ``firefox.icns`` file. You can verify that it includes all of the necessary resolutions and DPIs by inspecting it with ``Preview.app``. You will likely need to do this for all brandings (``official``, ``aurora``, ``nightly``, and ``unofficial`` at the time of writing).
