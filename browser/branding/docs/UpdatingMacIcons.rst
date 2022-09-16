.. _updatingmacicons:

====================
Updating macOS Icons
====================

macOS icons are stored as ``icns`` files that contain the same logo in multiple different sizes and DPIs. Apple's `Human Interface Guidelines <https://developer.apple.com/design/human-interface-guidelines/foundations/app-icons>`_ should be consulted for the specifics.

These icons can be updated by starting with a PNG of the highest resolution and DPI version, and using these steps to create the others, and the ``icns`` itself:

::

    mkdir firefox.iconset
    sips -z 16 16     firefox.png --out firefox.iconset/icon_16x16.png
    sips -z 32 32     firefox.png --out firefox.iconset/icon_16x16@2x.png
    sips -z 32 32     firefox.png --out firefox.iconset/icon_32x32.png
    sips -z 64 64     firefox.png --out firefox.iconset/icon_32x32@2x.png
    sips -z 128 128   firefox.png --out firefox.iconset/icon_128x128.png
    sips -z 256 256   firefox.png --out firefox.iconset/icon_128x128@2x.png
    sips -z 256 256   firefox.png --out firefox.iconset/icon_256x256.png
    sips -z 512 512   firefox.png --out firefox.iconset/icon_256x256@2x.png
    sips -z 512 512   firefox.png --out firefox.iconset/icon_512x512.png
    cp firefox.png firefox.iconset/icon_512x512@2x.png
    iconutil -c icns firefox.iconset

...which will create a ``firefox.icns`` file. You can verify that it includes all of the necessary resolutions and DPIs by inspecting it with ``Preview.app``.
