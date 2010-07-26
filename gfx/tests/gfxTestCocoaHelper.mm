
#import <Cocoa/Cocoa.h>

#include "gfxTestCocoaHelper.h"

static NSAutoreleasePool *sPool = NULL;

void
CocoaPoolInit() {
  if (sPool)
    [sPool release];
  sPool = [[NSAutoreleasePool alloc] init];
}

void
CocoaPoolRelease() {
  [sPool release];
  sPool = NULL;
}
