/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <dlfcn.h>
#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include "PluginUtilsOSX.h"

// Remove definitions for try/catch interfering with ObjCException macros.
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"

#include "nsDebug.h"

#include "mozilla/Sprintf.h"

@interface CALayer (ContentsScale)
- (double)contentsScale;
- (void)setContentsScale:(double)scale;
@end

using namespace mozilla::plugins::PluginUtilsOSX;

@interface CGBridgeLayer : CALayer {
  DrawPluginFunc mDrawFunc;
  void* mPluginInstance;
  nsIntRect mUpdateRect;
}
- (void)setDrawFunc:(DrawPluginFunc)aFunc
     pluginInstance:(void*)aPluginInstance;
- (void)updateRect:(nsIntRect)aRect;

@end

// CGBitmapContextSetData() is an undocumented function present (with
// the same signature) since at least OS X 10.5.  As the name suggests,
// it's used to replace the "data" in a bitmap context that was
// originally specified in a call to CGBitmapContextCreate() or
// CGBitmapContextCreateWithData().
typedef void (*CGBitmapContextSetDataFunc) (CGContextRef c,
                                            size_t x,
                                            size_t y,
                                            size_t width,
                                            size_t height,
                                            void* data,
                                            size_t bitsPerComponent,
                                            size_t bitsPerPixel,
                                            size_t bytesPerRow);
CGBitmapContextSetDataFunc CGBitmapContextSetDataPtr = NULL;

@implementation CGBridgeLayer
- (void) updateRect:(nsIntRect)aRect
{
   mUpdateRect.UnionRect(mUpdateRect, aRect);
}

- (void) setDrawFunc:(DrawPluginFunc)aFunc
      pluginInstance:(void*)aPluginInstance
{
  mDrawFunc = aFunc;
  mPluginInstance = aPluginInstance;
}

- (void)drawInContext:(CGContextRef)aCGContext
{
  ::CGContextSaveGState(aCGContext); 
  ::CGContextTranslateCTM(aCGContext, 0, self.bounds.size.height);
  ::CGContextScaleCTM(aCGContext, (CGFloat) 1, (CGFloat) -1);

  mUpdateRect = nsIntRect::Truncate(0, 0, self.bounds.size.width, self.bounds.size.height);

  mDrawFunc(aCGContext, mPluginInstance, mUpdateRect);

  ::CGContextRestoreGState(aCGContext);

  mUpdateRect.SetEmpty();
}

@end

void* mozilla::plugins::PluginUtilsOSX::GetCGLayer(DrawPluginFunc aFunc, 
                                                   void* aPluginInstance, 
                                                   double aContentsScaleFactor) {
  CGBridgeLayer *bridgeLayer = [[CGBridgeLayer alloc] init];

  // We need to make bridgeLayer behave properly when its superlayer changes
  // size (in nsCARenderer::SetBounds()).
  bridgeLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
  bridgeLayer.needsDisplayOnBoundsChange = YES;
  NSNull *nullValue = [NSNull null];
  NSDictionary *actions = [NSDictionary dictionaryWithObjectsAndKeys:
                             nullValue, @"bounds",
                             nullValue, @"contents",
                             nullValue, @"contentsRect",
                             nullValue, @"position",
                             nil];
  [bridgeLayer setStyle:[NSDictionary dictionaryWithObject:actions forKey:@"actions"]];

  // For reasons that aren't clear (perhaps one or more OS bugs), we can only
  // use full HiDPI resolution here if the tree is built with the 10.7 SDK or
  // up.  If we build with the 10.6 SDK, changing the contentsScale property
  // of bridgeLayer (even to the same value) causes it to stop working (go
  // blank).  This doesn't happen with objects that are members of the CALayer
  // class (as opposed to one of its subclasses).
#if defined(MAC_OS_X_VERSION_10_7) && \
    MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_7
  if ([bridgeLayer respondsToSelector:@selector(setContentsScale:)]) {
    bridgeLayer.contentsScale = aContentsScaleFactor;
  }
#endif

  [bridgeLayer setDrawFunc:aFunc
            pluginInstance:aPluginInstance];
  return bridgeLayer;
}

void mozilla::plugins::PluginUtilsOSX::ReleaseCGLayer(void *cgLayer) {
  CGBridgeLayer *bridgeLayer = (CGBridgeLayer*)cgLayer;
  [bridgeLayer release];
}

void mozilla::plugins::PluginUtilsOSX::Repaint(void *caLayer, nsIntRect aRect) {
  CGBridgeLayer *bridgeLayer = (CGBridgeLayer*)caLayer;
  [CATransaction begin];
  [bridgeLayer updateRect:aRect];
  [bridgeLayer setNeedsDisplay];
  [bridgeLayer displayIfNeeded];
  [CATransaction commit];
}

@interface EventProcessor : NSObject {
  RemoteProcessEvents   aRemoteEvents;
  void                 *aPluginModule;
}
- (void)setRemoteEvents:(RemoteProcessEvents) remoteEvents pluginModule:(void*) pluginModule;
- (void)onTick;
@end

@implementation EventProcessor
- (void) onTick
{
    aRemoteEvents(aPluginModule);
}

- (void)setRemoteEvents:(RemoteProcessEvents) remoteEvents pluginModule:(void*) pluginModule
{
    aRemoteEvents = remoteEvents;
    aPluginModule = pluginModule;
}
@end

#define EVENT_PROCESS_DELAY 0.05 // 50 ms

NPError mozilla::plugins::PluginUtilsOSX::ShowCocoaContextMenu(void* aMenu, int aX, int aY, void* pluginModule, RemoteProcessEvents remoteEvent) 
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;

  // Set the native cursor to the OS default (an arrow) before displaying the
  // context menu.  Otherwise (if the plugin has changed the cursor) it may
  // stay as the plugin has set it -- which means it may be invisible.  We
  // need to do this because we display the context menu without making the
  // plugin process the foreground process.  If we did, the cursor would
  // change to an arrow cursor automatically -- as it does in Chrome.
  [[NSCursor arrowCursor] set];

  EventProcessor* eventProcessor = nullptr;
  NSTimer *eventTimer = nullptr;
  if (pluginModule) {
    // Create a timer to process browser events while waiting
    // on the menu. This prevents the browser from hanging
    // during the lifetime of the menu.
    eventProcessor = [[EventProcessor alloc] init];
    [eventProcessor setRemoteEvents:remoteEvent pluginModule:pluginModule];
    eventTimer = [NSTimer timerWithTimeInterval:EVENT_PROCESS_DELAY
                                   target:eventProcessor selector:@selector(onTick)
                                   userInfo:nil repeats:TRUE];
    // Use NSEventTrackingRunLoopMode otherwise the timer will
    // not fire during the right click menu.
    [[NSRunLoop currentRunLoop] addTimer:eventTimer
                                 forMode:NSEventTrackingRunLoopMode];
  }

  NSMenu* nsmenu = reinterpret_cast<NSMenu*>(aMenu);
  NSPoint screen_point = ::NSMakePoint(aX, aY);

  [nsmenu popUpMenuPositioningItem:nil atLocation:screen_point inView:nil];

  if (pluginModule) {
    [eventTimer invalidate];
    [eventProcessor release];
  }

  return NPERR_NO_ERROR;

  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(NPERR_GENERIC_ERROR);
}

void mozilla::plugins::PluginUtilsOSX::InvokeNativeEventLoop()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  ::CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);
  NS_OBJC_END_TRY_ABORT_BLOCK;
}


#define UNDOCUMENTED_SESSION_CONSTANT ((int)-2)
namespace mozilla {
namespace plugins {
namespace PluginUtilsOSX {
  static void *sApplicationASN = NULL;
  static void *sApplicationInfoItem = NULL;
} // namespace PluginUtilsOSX
} // namespace plugins
} // namespace mozilla

bool mozilla::plugins::PluginUtilsOSX::SetProcessName(const char* aProcessName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  nsAutoreleasePool localPool;

  if (!aProcessName || strcmp(aProcessName, "") == 0) {
    return false;
  }

  NSString *currentName = [[[NSBundle mainBundle] localizedInfoDictionary]
                              objectForKey:(NSString *)kCFBundleNameKey];

  char formattedName[1024];
  SprintfLiteral(formattedName, "%s %s", [currentName UTF8String], aProcessName);

  aProcessName = formattedName;

  // This function is based on Chrome/Webkit's and relies on potentially dangerous SPI.
  typedef CFTypeRef (*LSGetASNType)();
  typedef OSStatus (*LSSetInformationItemType)(int, CFTypeRef,
                                               CFStringRef, 
                                               CFStringRef,
                                               CFDictionaryRef*);

  CFBundleRef launchServices = ::CFBundleGetBundleWithIdentifier(
                                          CFSTR("com.apple.LaunchServices"));
  if (!launchServices) {
    NS_WARNING("Failed to set process name: Could not open LaunchServices bundle");
    return false;
  }

  if (!sApplicationASN) {
    sApplicationASN = ::CFBundleGetFunctionPointerForName(launchServices, 
                                            CFSTR("_LSGetCurrentApplicationASN"));
    if (!sApplicationASN) {
      NS_WARNING("Failed to set process name: Could not get function pointer "
                 "for LaunchServices");
      return false;
    }
  }

  LSGetASNType getASNFunc = reinterpret_cast<LSGetASNType>
                                          (sApplicationASN);

  if (!sApplicationInfoItem) {
    sApplicationInfoItem = ::CFBundleGetFunctionPointerForName(launchServices, 
                                            CFSTR("_LSSetApplicationInformationItem"));
  }

  LSSetInformationItemType setInformationItemFunc 
                                          = reinterpret_cast<LSSetInformationItemType>
                                          (sApplicationInfoItem);

  void * displayNameKeyAddr = ::CFBundleGetDataPointerForName(launchServices,
                                          CFSTR("_kLSDisplayNameKey"));

  CFStringRef displayNameKey = nil;
  if (displayNameKeyAddr) {
    displayNameKey = reinterpret_cast<CFStringRef>(*(CFStringRef*)displayNameKeyAddr);
  }

  // Rename will fail without this
  ProcessSerialNumber psn;
  if (::GetCurrentProcess(&psn) != noErr) {
    return false;
  }

  CFTypeRef currentAsn = getASNFunc();

  if (!getASNFunc || !setInformationItemFunc || 
      !displayNameKey || !currentAsn) {
    NS_WARNING("Failed to set process name: Accessing launchServices failed");
    return false;
  }

  CFStringRef processName = ::CFStringCreateWithCString(nil, 
                                                        aProcessName, 
                                                        kCFStringEncodingASCII);
  if (!processName) {
    NS_WARNING("Failed to set process name: Could not create CFStringRef");
    return false;
  }

  OSErr err = setInformationItemFunc(UNDOCUMENTED_SESSION_CONSTANT, currentAsn,
                                     displayNameKey, processName,
                                     nil); // Optional out param
  ::CFRelease(processName);
  if (err != noErr) {
    NS_WARNING("Failed to set process name: LSSetInformationItemType err");
    return false;
  }

  return true;
  NS_OBJC_END_TRY_ABORT_BLOCK_RETURN(false);
}

namespace mozilla {
namespace plugins {
namespace PluginUtilsOSX {

size_t nsDoubleBufferCARenderer::GetFrontSurfaceWidth() {
  if (!HasFrontSurface()) {
    return 0;
  }

  return mFrontSurface->GetWidth();
}

size_t nsDoubleBufferCARenderer::GetFrontSurfaceHeight() {
  if (!HasFrontSurface()) {
    return 0;
  }

  return mFrontSurface->GetHeight();
}

double nsDoubleBufferCARenderer::GetFrontSurfaceContentsScaleFactor() {
  if (!HasFrontSurface()) {
    return 1.0;
  }

  return mFrontSurface->GetContentsScaleFactor();
}

size_t nsDoubleBufferCARenderer::GetBackSurfaceWidth() {
  if (!HasBackSurface()) {
    return 0;
  }

  return mBackSurface->GetWidth();
}

size_t nsDoubleBufferCARenderer::GetBackSurfaceHeight() {
  if (!HasBackSurface()) {
    return 0;
  }

  return mBackSurface->GetHeight();
}

double nsDoubleBufferCARenderer::GetBackSurfaceContentsScaleFactor() {
  if (!HasBackSurface()) {
    return 1.0;
  }

  return mBackSurface->GetContentsScaleFactor();
}

IOSurfaceID nsDoubleBufferCARenderer::GetFrontSurfaceID() {
  if (!HasFrontSurface()) {
    return 0;
  }

  return mFrontSurface->GetIOSurfaceID();
}

bool nsDoubleBufferCARenderer::HasBackSurface() {
  return !!mBackSurface;
}

bool nsDoubleBufferCARenderer::HasFrontSurface() {
  return !!mFrontSurface;
}

bool nsDoubleBufferCARenderer::HasCALayer() {
  return !!mCALayer;
}

void nsDoubleBufferCARenderer::SetCALayer(void *aCALayer) {
  mCALayer = aCALayer;
}

bool nsDoubleBufferCARenderer::InitFrontSurface(size_t aWidth, size_t aHeight,
                                                double aContentsScaleFactor,
                                                AllowOfflineRendererEnum aAllowOfflineRenderer) {
  if (!mCALayer) {
    return false;
  }

  mContentsScaleFactor = aContentsScaleFactor;
  mFrontSurface = MacIOSurface::CreateIOSurface(aWidth, aHeight, mContentsScaleFactor);
  if (!mFrontSurface) {
    mCARenderer = nullptr;
    return false;
  }

  if (!mCARenderer) {
    mCARenderer = new nsCARenderer();
    if (!mCARenderer) {
      mFrontSurface = nullptr;
      return false;
    }

    mCARenderer->AttachIOSurface(mFrontSurface);

    nsresult result = mCARenderer->SetupRenderer(mCALayer,
                        mFrontSurface->GetWidth(),
                        mFrontSurface->GetHeight(),
                        mContentsScaleFactor,
                        aAllowOfflineRenderer);

    if (result != NS_OK) {
      mCARenderer = nullptr;
      mFrontSurface = nullptr;
      return false;
    }
  } else {
    mCARenderer->AttachIOSurface(mFrontSurface);
  }

  return true;
}

void nsDoubleBufferCARenderer::Render() {
  if (!HasFrontSurface() || !mCARenderer) {
    return;
  }

  mCARenderer->Render(GetFrontSurfaceWidth(), GetFrontSurfaceHeight(),
                      mContentsScaleFactor, nullptr);
}

void nsDoubleBufferCARenderer::SwapSurfaces() {
  RefPtr<MacIOSurface> prevFrontSurface = mFrontSurface;
  mFrontSurface = mBackSurface;
  mBackSurface = prevFrontSurface;

  if (mFrontSurface) {
    mCARenderer->AttachIOSurface(mFrontSurface);
  }
}

void nsDoubleBufferCARenderer::ClearFrontSurface() {
  mFrontSurface = nullptr;
  if (!mFrontSurface && !mBackSurface) {
    mCARenderer = nullptr;
  }
}

void nsDoubleBufferCARenderer::ClearBackSurface() {
  mBackSurface = nullptr;
  if (!mFrontSurface && !mBackSurface) {
    mCARenderer = nullptr;
  }
}

} // namespace PluginUtilsOSX
} // namespace plugins
} // namespace mozilla

