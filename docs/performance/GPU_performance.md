# GPU Performance

Doing performance work with GPUs is harder than with CPUs because of the
asynchronous and massively parallel architecture.

## Tools

PIX - Can do timing of Direct3D calls. Works reasonably well with
Firefox. See also [Debugging With
PIX](/en/Debugging_With_PIX "en/Debugging With PIX").

NVIDIA PerfHUD - Last I checked required a special build to be used.

NVIDIA Parallel Nsight - Haven\'t tried.

AMD GPU ShaderAnalyzer - Will compile a shader and show the machine code
and give static pipeline estimations. Not that useful for Firefox
because all of our shaders are pretty simple.

AMD GPU PerfStudio - I had trouble getting this to work, and can\'t
remember whether I actually did or not.

[Intel Graphics Performance Analyzers](http://software.intel.com/en-us/articles/intel-gpa/ "http://software.intel.com/en-us/articles/intel-gpa/")
- Haven\'t tried.

[APITrace](https://github.com/apitrace/apitrace "https://github.com/apitrace/apitrace")
- Open source, works OK.

[PVRTrace](http://www.imgtec.com/powervr/insider/pvrtrace.asp "http://www.imgtec.com/powervr/insider/pvrtrace.asp")
- Doesn\'t seem to emit traces on android/Nexus S. Looks like it\'s
designed for X11-based linux-ARM devices, OMAP3 is mentioned a lot in
the docs \...

## Guides

[Accurately Profiling Direct3D API Calls (Direct3D
9)](http://msdn.microsoft.com/en-us/library/bb172234%28v=vs.85%29.aspx "http://msdn.microsoft.com/en-us/library/bb172234(v=vs.85).aspx")
Suggests avoiding normal profilers like xperf and instead measuring the
time to flush the command buffer.

[OS X - Best Practices for Working with Texture
Data](http://developer.apple.com/library/mac/#documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html "http://developer.apple.com/library/mac/#documentation/GraphicsImaging/Conceptual/OpenGL-MacProgGuide/opengl_texturedata/opengl_texturedata.html")
- Sort of old, but still useful.
