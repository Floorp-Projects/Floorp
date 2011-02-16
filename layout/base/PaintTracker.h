#ifndef mozilla_PaintTracker_h
#define mozilla_PaintTracker_h

#include "nscore.h"
#include "nsDebug.h"

namespace mozilla {

class NS_STACK_CLASS PaintTracker
{
public:
  PaintTracker() {
    ++gPaintTracker;
  }
  ~PaintTracker() {
    NS_ASSERTION(gPaintTracker > 0, "Mismatched constructor/destructor");
    --gPaintTracker;
  }

  static bool IsPainting() {
    return !!gPaintTracker;
  }

private:
  static int gPaintTracker;
};

} // namespace mozilla

#endif // mozilla_PaintTracker_h
