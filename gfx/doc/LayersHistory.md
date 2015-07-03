This is an overview of the major events in the history of our Layers infrastructure.

- iPhone released in July 2007 (Built on a toolkit called LayerKit)

- Core Animation (October 2007) LayerKit was publicly renamed to OS X 10.5

- Webkit CSS 3d transforms (July 2009)

- Original layers API (March 2010) Introduced the idea of a layer manager that
  would composite. One of the first use cases for this was hardware accelerated
  YUV conversion for video.

- Retained layers (July 7 2010 - Bug 564991)
This was an important concept that introduced the idea of persisting the layer
content across paints in gecko controlled buffers instead of just by the OS. This introduced
the concept of buffer rotation to deal with scrolling instead of using the
native scrolling APIs like ScrollWindowEx

- Layers IPC (July 2010 - Bug 570294)
This introduced shadow layers and edit lists and was originally done for e10s v1

- 3d transforms (September 2011 - Bug 505115)

- OMTC (December 2012 - Bug 711168)
This was prototyped on OS X but shipped first for Fennec

- Tiling v1 (April 2012 - Bug 739679)
Originally done for Fennec.
This was done to avoid situations where we had to do a bunch of work for
scrolling a small amount. i.e. buffer rotation.  It allowed us to have a
variety of interesting features like progressive painting and lower resolution
painting.

- C++ Async pan zoom controller (July 2012 - Bug 750974)
The existing APZ code was in Java for Fennec so this was reimplemented.

- Compositor API (April 2013 - Bug 825928)
Layers refactoring created a compositor API that abstracted away the differences between the
D3D vs OpenGL. The main piece of API is DrawQuad.

- Tiling v2 (Mar 7 2014 - Bug 963073)
Tiling for B2G. This work is mainly porting tiled layers to new textures,
implementing double-buffered tiles and implementing a texture client pool, to
be used by tiled content clients.

 A large motivation for the pool was the very slow performance of allocating tiles because
of the sync messages to the compositor.

 The slow performance of allocating was directly addressed by bug 959089 which allowed us
to allocate gralloc buffers without sync messages to the compositor thread.
