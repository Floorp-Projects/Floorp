Mozilla Graphics Overview {#graphicsoverview}
=================
## Work in progress.  Possibly incorrect or incomplete.

Overview
--------
The graphics systems is responsible for rendering (painting, drawing) the frame tree (rendering tree) elements as created by the layout system.  Each leaf in the tree has content, either bounded by a rectangle (or perhaps another shape, in the case of SVG.)

The simple approach for producing the result would thus involve traversing the frame tree, in a correct order, drawing each frame into the resulting buffer and displaying (printing non-withstanding) that buffer when the traversal is done. It is worth spending some time on the "correct order" note above.  If there are no overlapping frames, this is fairly simple - any order will do, as long as there is no background.  If there is background, we just have to worry about drawing that first. Since we do not control the content, chances are the page is more complicated.  There are overlapping frames, likely with transparency, so we need to make sure the elements are draw "back to front", in layers, so to speak.  Layers are an important concept, and we will revisit them shortly, as they are central to fixing a major issue with the above simple approach.

While the above simple approach will work, the performance will suffer.  Each time anything changes in any of the frames, the complete process needs to be repeated, everything needs to be redrawn.  Further, there is very little space to take advantage of the modern graphics (GPU) hardware, or multi-core computers.  If you recall from the previous sections, the frame tree is only accessible from the UI thread, so while we're doing all this work, the UI is basically blocked.

### (Retained) Layers

Layers framework was introduced to address the above performance issues, by having a part of the design address each item. At the high level:

1. We create a layer tree. The leaf elements of the tree contain all frames (possibly multiple frames per leaf).
2. We render each layer tree element and cache (retain) the result.
3. We composite (combine) all the leaf elements into the final result.

Let's examine each of these steps, in reverse order.

### Compositing
We use the term composite as it implies that the order is important.  If the elements being composited overlap, whether there is transparency involved or not, the order in which they are combined will effect the result.
Compositing is where we can use some of the power of the modern graphics hardware.  It is optimal for doing this job. In the scenarios where only the position of individual frames changes, without the content inside them changing, we see why caching each layer would be advantageous - we only need to repeat the final compositing step, completely skipping the layer tree creation and the rendering of each leaf, thus speeding up the process considerably.

Another benefit is equally apparent in the context of the stated deficiencies of the simple approach. We can use the available graphics hardware accelerated APIs to do the compositing step.  Direct3D, OpenGL can be used on different platforms and are well suited to accelerate this step.

Finally, we can now envision performing the compositing step on a separate thread, unblocking the UI thread for other work, and doing more work in parallel.  More on this below.

It is important to note that the number of operations in this step is proportional to the number of layer tree (leaf) elements, so there is additional work and complexity involved, when the layer tree is large.

#### Render and retain layer elements
As we saw, the compositing step benefits from caching the intermediate result.  This does result in the extra memory usage, so needs to be considered during the layer tree creation. Beyond the caching, we can accelerate the rendering of each element by (indirectly) using the available platform APIs (e.g., Direct2D, CoreGraphics, even some of the 3D APIs like OpenGL or Direct3D) as available.  This is actually done through a platform independent API (see Moz2D) below, but is important to realize it does get accelerated appropriately.

#### Creating the layer tree
We need to create a layer tree (from the frames tree), which will give us the correct result while striking the right balance between a layer per frame element and a single layer for the complete frames tree.  As was mentioned above, there is an overhead in traversing the whole tree and caching each of the elements, balanced by the performance improvements.  Some of the performance improvements are only noticed when something changes (e.g., one element is moving, we only need to redo the compositing step).

### Refresh Driver

### Layers

#### Rendering each layer

### Tiling vs. Buffer Rotation vs. Full paint

#### Compositing for the final result

### Graphics API

#### Moz2D
* The Moz2D graphics API, part of the Azure project, is a cross-platform interface onto the various graphics backends that Gecko uses for rendering such as Direct2D (1.0 and 1.1), Skia, Cairo, Quartz, and NV Path. Adding a new graphics platform to Gecko is accomplished by adding a backend to Moz2D.
\see [Moz2D documentation on wiki](https://wiki.mozilla.org/Platform/GFX/Moz2D)

#### Compositing

#### Image Decoding

#### Image Animation

### Funny words
There are a lot of code words that we use to refer to projects, libraries, areas of the code.  Here's an attempt to cover some of those:
* Azure - See Moz2D in the Graphics API section above.
* Backend - See Moz2D in the Graphics API section above.
* Cairo - http://www.cairographics.org/.  Cairo is a 2D graphics library with support for multiple output devices. Currently supported output targets include the X Window System (via both Xlib and XCB), Quartz, Win32, image buffers, PostScript, PDF, and SVG file output. 
* Moz2D - See Moz2D in the Graphics API section above.
* Thebes - Graphics API that preceded Moz2D.
* Reflow
* Display list

### [Historical Documents](http://www.youtube.com/watch?v=lLZQz26-kms)
A number of posts and blogs that will give you more details or more background, or reasoning that led to different solutions and approaches.

* 2010-01 [Layers: Cross Platform Acceleration] (http://www.basschouten.com/blog1.php/layers-cross-platform-acceleration) 
* 2010-04 [Layers] (http://robert.ocallahan.org/2010/04/layers_01.html)
* 2010-07 [Retained Layers](http://robert.ocallahan.org/2010/07/retained-layers_16.html)
* 2011-04 [Introduction](https://blog.mozilla.org/joe/2011/04/26/introducing-the-azure-project/ Moz2D)
* 2011-07 [Layers](http://chrislord.net/index.php/2011/07/25/shadow-layers-and-learning-by-failing/ Shadow)
* 2011-09 [Graphics API Design](http://robert.ocallahan.org/2011/09/graphics-api-design.html)
* 2012-04 [Moz2D Canvas on OSX](http://muizelaar.blogspot.ca/2012/04/azure-canvas-on-os-x.html)
* 2012-05 [Mask Layers](http://featherweightmusings.blogspot.co.uk/2012/05/mask-layers_26.html)
* 2013-07 [Graphics related](http://www.basschouten.com/blog1.php)

