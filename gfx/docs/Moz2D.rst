Moz2D
========================

The `gfx/2d` contains our abstraction of a typical 2D API (similar
to the HTML Canvas API). It has different backends used for different
purposes. Direct2D is used for implementing hardware accelerated
canvas on Windows. Skia is used for any software drawing needs and
Cairo is used for printing.

Previously, Moz2D aimed to be buildable independently from the rest of
Gecko but we've slipped from this because C++/Gecko don't have a good
mechanism for modularization/dependencies. That being said, we still try
to keep the coupling with the rest of Gecko low for hygiene, simplicity
and perhaps a more modular future.

See also `Moz2D documentation on wiki <https://wiki.mozilla.org/Platform/GFX/Moz2D>`.
