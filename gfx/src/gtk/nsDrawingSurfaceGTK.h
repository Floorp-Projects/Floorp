#ifndef nsDrawingSurfaceGTK_h__
#define nsDrawingSurfaceGTK_h__

#include <gdk/gdk.h>

struct nsDrawingSurfaceGTK {
  GdkDrawable *drawable;
  GdkGC *gc;
};

typedef nsDrawingSurfaceGTK nsDrawingSurfaceGTK;

#endif /* nsDrawingSurfaceGTK_h__ */

