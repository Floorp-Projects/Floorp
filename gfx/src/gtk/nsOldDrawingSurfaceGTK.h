#ifndef nsOldDrawingSurfaceGTK_h__
#define nsOldDrawingSurfaceGTK_h__

#include <gdk/gdk.h>

struct nsDrawingSurfaceGTK {
  GdkDrawable *drawable;
  GdkGC *gc;
};

typedef nsDrawingSurfaceGTK nsDrawingSurfaceGTK;

#endif /* nsOldDrawingSurfaceGTK_h__ */

