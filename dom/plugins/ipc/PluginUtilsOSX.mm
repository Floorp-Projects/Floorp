/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* ***** BEGIN LICENSE BLOCK *****
  * Version: MPL 1.1/GPL 2.0/LGPL 2.1
  *
  * The contents of this file are subject to the Mozilla Public License Version
  * 1.1 (the "License"); you may not use this file except in compliance with
  * the License. You may obtain a copy of the License at
  * http://www.mozilla.org/MPL/
  *
  * Software distributed under the License is distributed on an "AS IS" basis,
  * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
  * for the specific language governing rights and limitations under the
  * License.
  *
  * The Original Code is Mozilla.org code.
  *
  * The Initial Developer of the Original Code is
  *   Mozilla Corporation.
  * Portions created by the Initial Developer are Copyright (C) 2010
  * the Initial Developer. All Rights Reserved.
  *
  * Contributor(s):
  *   Benoit Girard <b56girard@gmail.com>
  *
  * Alternatively, the contents of this file may be used under the terms of
  * either of the GNU General Public License Version 2 or later (the "GPL"),
  * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
  * in which case the provisions of the GPL or the LGPL are applicable instead
  * of those above. If you wish to allow use of your version of this file only
  * under the terms of either the GPL or the LGPL, and not to allow others to
  * use your version of this file under the terms of the MPL, indicate your
  * decision by deleting the provisions above and replace them with the notice
  * and other provisions required by the GPL or the LGPL. If you do not delete
  * the provisions above, a recipient may use your version of this file under
  * the terms of any one of the MPL, the GPL or the LGPL.
  *
  * ***** END LICENSE BLOCK ***** */

#import <AppKit/AppKit.h>
#import <QuartzCore/QuartzCore.h>
#include "PluginUtilsOSX.h"

// Remove definitions for try/catch interfering with ObjCException macros.
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"

#include "nsDebug.h"

using namespace mozilla::plugins::PluginUtilsOSX;

@interface CGBridgeLayer : CALayer {
  DrawPluginFunc mDrawFunc;
  void* mPluginInstance;
  nsIntRect mUpdateRect;
}
- (void) setDrawFunc: (DrawPluginFunc)aFunc pluginInstance:(void*) aPluginInstance;
- (void) updateRect: (nsIntRect)aRect;

@end

@implementation CGBridgeLayer
- (void) updateRect: (nsIntRect)aRect
{
   mUpdateRect.UnionRect(mUpdateRect, aRect);
}

- (void) setDrawFunc: (DrawPluginFunc)aFunc pluginInstance:(void*) aPluginInstance
{
  mDrawFunc = aFunc;
  mPluginInstance = aPluginInstance;
}

- (void)drawInContext:(CGContextRef)aCGContext

{
  ::CGContextSaveGState(aCGContext); 
  ::CGContextTranslateCTM(aCGContext, 0, self.bounds.size.height);
  ::CGContextScaleCTM(aCGContext, (CGFloat) 1, (CGFloat) -1);

  mDrawFunc(aCGContext, mPluginInstance, mUpdateRect);

  ::CGContextRestoreGState(aCGContext); 

  mUpdateRect.SetEmpty();
}

@end

void* mozilla::plugins::PluginUtilsOSX::GetCGLayer(DrawPluginFunc aFunc, void* aPluginInstance) {
  CGBridgeLayer *bridgeLayer = [[CGBridgeLayer alloc] init ];
  [bridgeLayer setDrawFunc:aFunc pluginInstance:aPluginInstance];
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

  // Create a timer to process browser events while waiting
  // on the menu. This prevents the browser from hanging
  // during the lifetime of the menu.
  EventProcessor* eventProcessor = [[EventProcessor alloc] init];
  [eventProcessor setRemoteEvents:remoteEvent pluginModule:pluginModule];
  NSTimer *eventTimer = [NSTimer timerWithTimeInterval:EVENT_PROCESS_DELAY
                             target:eventProcessor selector:@selector(onTick) 
                             userInfo:nil repeats:TRUE];
  // Use NSEventTrackingRunLoopMode otherwise the timer will
  // not fire during the right click menu.
  [[NSRunLoop currentRunLoop] addTimer:eventTimer 
                              forMode:NSEventTrackingRunLoopMode];

  NSMenu* nsmenu = reinterpret_cast<NSMenu*>(aMenu);
  NSPoint screen_point = ::NSMakePoint(aX, aY);

  [nsmenu popUpMenuPositioningItem:nil atLocation:screen_point inView:nil];

  [eventTimer invalidate];
  [eventProcessor release];

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
}
}
}

bool mozilla::plugins::PluginUtilsOSX::SetProcessName(const char* aProcessName) {
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK;
  nsAutoreleasePool localPool;

  if (!aProcessName || strcmp(aProcessName, "") == 0) {
    return false;
  }

  NSString *currentName = [[[NSBundle mainBundle] localizedInfoDictionary] 
                              objectForKey:(NSString *)kCFBundleNameKey];

  char formattedName[1024];
  snprintf(formattedName, sizeof(formattedName), 
      "%s (%s)", [currentName UTF8String], aProcessName);

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
                                                AllowOfflineRendererEnum aAllowOfflineRenderer) {
  if (!mCALayer) {
    return false;
  }

  mFrontSurface = nsIOSurface::CreateIOSurface(aWidth, aHeight);
  if (!mFrontSurface) {
    return false;
  }

  mFrontRenderer = new nsCARenderer();
  if (!mFrontRenderer) {
    mFrontSurface = nsnull;
    return false;
  }

  nsRefPtr<nsIOSurface> ioSurface = nsIOSurface::LookupSurface(mFrontSurface->GetIOSurfaceID());
  if (!ioSurface) {
    mFrontRenderer = nsnull;
    mFrontSurface = nsnull;
    return false;
  }

  mFrontRenderer->AttachIOSurface(ioSurface);

  nsresult result = mFrontRenderer->SetupRenderer(mCALayer,
                        ioSurface->GetWidth(),
                        ioSurface->GetHeight(),
                        aAllowOfflineRenderer);

  return result == NS_OK;
}

void nsDoubleBufferCARenderer::Render() {
  if (!HasFrontSurface()) {
    return;
  }

  mFrontRenderer->Render(GetFrontSurfaceWidth(), GetFrontSurfaceHeight(), nsnull);
}

void nsDoubleBufferCARenderer::SwapSurfaces() {
  if (mFrontRenderer) {
    mFrontRenderer->DettachCALayer();
  }

  nsRefPtr<nsCARenderer> prevFrontRenderer = mFrontRenderer;
  nsRefPtr<nsIOSurface> prevFrontSurface = mFrontSurface;

  mFrontRenderer = mBackRenderer;
  mFrontSurface = mBackSurface;

  mBackRenderer = prevFrontRenderer;
  mBackSurface = prevFrontSurface;

  if (mFrontRenderer) {
    mFrontRenderer->AttachCALayer(mCALayer);
  }
}

void nsDoubleBufferCARenderer::ClearFrontSurface() {
  mFrontRenderer = nsnull;
  mFrontSurface = nsnull;
}

void nsDoubleBufferCARenderer::ClearBackSurface() {
  mBackRenderer = nsnull;
  mBackSurface = nsnull;
}

} //PluginUtilsOSX
} //plugins
} //mozilla

