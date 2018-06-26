/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "base/basictypes.h"
#include "nsCocoaUtils.h"
#include "PluginModuleChild.h"
#include "nsDebug.h"
#include "PluginInterposeOSX.h"
#include <set>
#import <AppKit/AppKit.h>
#import <objc/runtime.h>
#import <Carbon/Carbon.h>

using namespace mozilla::plugins;

namespace mac_plugin_interposing {

int32_t NSCursorInfo::mNativeCursorsSupported = -1;

// This constructor may be called from the browser process or the plugin
// process.
NSCursorInfo::NSCursorInfo()
  : mType(TypeArrow)
  , mHotSpot(nsPoint(0, 0))
  , mCustomImageData(NULL)
  , mCustomImageDataLength(0)
{
}

NSCursorInfo::NSCursorInfo(NSCursor* aCursor)
  : mType(TypeArrow)
  , mHotSpot(nsPoint(0, 0))
  , mCustomImageData(NULL)
  , mCustomImageDataLength(0)
{
  // This constructor is only ever called from the plugin process, so the
  // following is safe.
  if (!GetNativeCursorsSupported()) {
    return;
  }

  NSPoint hotSpotCocoa = [aCursor hotSpot];
  mHotSpot = nsPoint(hotSpotCocoa.x, hotSpotCocoa.y);

  Class nsCursorClass = [NSCursor class];
  if ([aCursor isEqual:[NSCursor arrowCursor]]) {
    mType = TypeArrow;
  } else if ([aCursor isEqual:[NSCursor closedHandCursor]]) {
    mType = TypeClosedHand;
  } else if ([aCursor isEqual:[NSCursor crosshairCursor]]) {
    mType = TypeCrosshair;
  } else if ([aCursor isEqual:[NSCursor disappearingItemCursor]]) {
    mType = TypeDisappearingItem;
  } else if ([aCursor isEqual:[NSCursor IBeamCursor]]) {
    mType = TypeIBeam;
  } else if ([aCursor isEqual:[NSCursor openHandCursor]]) {
    mType = TypeOpenHand;
  } else if ([aCursor isEqual:[NSCursor pointingHandCursor]]) {
    mType = TypePointingHand;
  } else if ([aCursor isEqual:[NSCursor resizeDownCursor]]) {
    mType = TypeResizeDown;
  } else if ([aCursor isEqual:[NSCursor resizeLeftCursor]]) {
    mType = TypeResizeLeft;
  } else if ([aCursor isEqual:[NSCursor resizeLeftRightCursor]]) {
    mType = TypeResizeLeftRight;
  } else if ([aCursor isEqual:[NSCursor resizeRightCursor]]) {
    mType = TypeResizeRight;
  } else if ([aCursor isEqual:[NSCursor resizeUpCursor]]) {
    mType = TypeResizeUp;
  } else if ([aCursor isEqual:[NSCursor resizeUpDownCursor]]) {
    mType = TypeResizeUpDown;
  // The following cursor types are only supported on OS X 10.6 and up.
  } else if ([nsCursorClass respondsToSelector:@selector(contextualMenuCursor)] &&
             [aCursor isEqual:[nsCursorClass performSelector:@selector(contextualMenuCursor)]]) {
    mType = TypeContextualMenu;
  } else if ([nsCursorClass respondsToSelector:@selector(dragCopyCursor)] &&
             [aCursor isEqual:[nsCursorClass performSelector:@selector(dragCopyCursor)]]) {
    mType = TypeDragCopy;
  } else if ([nsCursorClass respondsToSelector:@selector(dragLinkCursor)] &&
             [aCursor isEqual:[nsCursorClass performSelector:@selector(dragLinkCursor)]]) {
    mType = TypeDragLink;
  } else if ([nsCursorClass respondsToSelector:@selector(operationNotAllowedCursor)] &&
             [aCursor isEqual:[nsCursorClass performSelector:@selector(operationNotAllowedCursor)]]) {
    mType = TypeNotAllowed;
  } else {
    NSImage* image = [aCursor image];
    NSArray* reps = image ? [image representations] : nil;
    NSUInteger repsCount = reps ? [reps count] : 0;
    if (!repsCount) {
      // If we have a custom cursor with no image representations, assume we
      // need a transparent cursor.
      mType = TypeTransparent;
    } else {
      CGImageRef cgImage = nil;
      // XXX We don't know how to deal with a cursor that doesn't have a
      //     bitmap image representation.  For now we fall back to an arrow
      //     cursor.
      for (NSUInteger i = 0; i < repsCount; ++i) {
        id rep = [reps objectAtIndex:i];
        if ([rep isKindOfClass:[NSBitmapImageRep class]]) {
          cgImage = [(NSBitmapImageRep*)rep CGImage];
          break;
        }
      }
      if (cgImage) {
        CFMutableDataRef data = ::CFDataCreateMutable(kCFAllocatorDefault, 0);
        if (data) {
          CGImageDestinationRef dest = ::CGImageDestinationCreateWithData(data,
                                                                          kUTTypePNG,
                                                                          1,
                                                                          NULL);
          if (dest) {
            ::CGImageDestinationAddImage(dest, cgImage, NULL);
            if (::CGImageDestinationFinalize(dest)) {
              uint32_t dataLength = (uint32_t) ::CFDataGetLength(data);
              mCustomImageData = (uint8_t*) moz_xmalloc(dataLength);
              ::CFDataGetBytes(data, ::CFRangeMake(0, dataLength), mCustomImageData);
              mCustomImageDataLength = dataLength;
              mType = TypeCustom;
            }
            ::CFRelease(dest);
          }
          ::CFRelease(data);
        }
      }
      if (!mCustomImageData) {
        mType = TypeArrow;
      }
    }
  }
}

NSCursorInfo::NSCursorInfo(const Cursor* aCursor)
  : mType(TypeArrow)
  , mHotSpot(nsPoint(0, 0))
  , mCustomImageData(NULL)
  , mCustomImageDataLength(0)
{
  // This constructor is only ever called from the plugin process, so the
  // following is safe.
  if (!GetNativeCursorsSupported()) {
    return;
  }

  mHotSpot = nsPoint(aCursor->hotSpot.h, aCursor->hotSpot.v);

  int width = 16, height = 16;
  int bytesPerPixel = 4;
  int rowBytes = width * bytesPerPixel;
  int bitmapSize = height * rowBytes;

  bool isTransparent = true;

  uint8_t* bitmap = (uint8_t*) moz_xmalloc(bitmapSize);
  // The way we create 'bitmap' is largely "borrowed" from Chrome's
  // WebCursor::InitFromCursor().
  for (int y = 0; y < height; ++y) {
    unsigned short data = aCursor->data[y];
    unsigned short mask = aCursor->mask[y];
    // Change 'data' and 'mask' from big-endian to little-endian, but output
    // big-endian data below.
    data = ((data << 8) & 0xFF00) | ((data >> 8) & 0x00FF);
    mask = ((mask << 8) & 0xFF00) | ((mask >> 8) & 0x00FF);
    // It'd be nice to use a gray-scale bitmap.  But
    // CGBitmapContextCreateImage() (used below) won't work with one that also
    // has alpha values.
    for (int x = 0; x < width; ++x) {
      int offset = (y * rowBytes) + (x * bytesPerPixel);
      // Color value
      if (data & 0x8000) {
        bitmap[offset]     = 0x0;
        bitmap[offset + 1] = 0x0;
        bitmap[offset + 2] = 0x0;
      } else {
        bitmap[offset]     = 0xFF;
        bitmap[offset + 1] = 0xFF;
        bitmap[offset + 2] = 0xFF;
      }
      // Mask value
      if (mask & 0x8000) {
        bitmap[offset + 3] = 0xFF;
        isTransparent = false;
      } else {
        bitmap[offset + 3] = 0x0;
      }
      data <<= 1;
      mask <<= 1;
    }
  }

  if (isTransparent) {
    // If aCursor is transparent, we don't need to serialize custom cursor
    // data over IPC.
    mType = TypeTransparent;
  } else {
    CGColorSpaceRef color = ::CGColorSpaceCreateDeviceRGB();
    if (color) {
      CGContextRef context =
        ::CGBitmapContextCreate(bitmap,
                                width,
                                height,
                                8,
                                rowBytes,
                                color,
                                kCGImageAlphaPremultipliedLast |
                                  kCGBitmapByteOrder32Big);
      if (context) {
        CGImageRef image = ::CGBitmapContextCreateImage(context);
        if (image) {
          ::CFMutableDataRef data = ::CFDataCreateMutable(kCFAllocatorDefault, 0);
          if (data) {
            CGImageDestinationRef dest =
              ::CGImageDestinationCreateWithData(data,
                                                 kUTTypePNG,
                                                 1,
                                                 NULL);
            if (dest) {
              ::CGImageDestinationAddImage(dest, image, NULL);
              if (::CGImageDestinationFinalize(dest)) {
                uint32_t dataLength = (uint32_t) ::CFDataGetLength(data);
                mCustomImageData = (uint8_t*) moz_xmalloc(dataLength);
                ::CFDataGetBytes(data,
                                 ::CFRangeMake(0, dataLength),
                                 mCustomImageData);
                mCustomImageDataLength = dataLength;
                mType = TypeCustom;
              }
              ::CFRelease(dest);
            }
            ::CFRelease(data);
          }
          ::CGImageRelease(image);
        }
        ::CGContextRelease(context);
      }
      ::CGColorSpaceRelease(color);
    }
  }

  free(bitmap);
}

NSCursorInfo::~NSCursorInfo()
{
  if (mCustomImageData) {
    free(mCustomImageData);
  }
}

NSCursor* NSCursorInfo::GetNSCursor() const
{
  NSCursor* retval = nil;

  Class nsCursorClass = [NSCursor class];
  switch(mType) {
    case TypeArrow:
      retval = [NSCursor arrowCursor];
      break;
    case TypeClosedHand:
      retval = [NSCursor closedHandCursor];
      break;
    case TypeCrosshair:
      retval = [NSCursor crosshairCursor];
      break;
    case TypeDisappearingItem:
      retval = [NSCursor disappearingItemCursor];
      break;
    case TypeIBeam:
      retval = [NSCursor IBeamCursor];
      break;
    case TypeOpenHand:
      retval = [NSCursor openHandCursor];
      break;
    case TypePointingHand:
      retval = [NSCursor pointingHandCursor];
      break;
    case TypeResizeDown:
      retval = [NSCursor resizeDownCursor];
      break;
    case TypeResizeLeft:
      retval = [NSCursor resizeLeftCursor];
      break;
    case TypeResizeLeftRight:
      retval = [NSCursor resizeLeftRightCursor];
      break;
    case TypeResizeRight:
      retval = [NSCursor resizeRightCursor];
      break;
    case TypeResizeUp:
      retval = [NSCursor resizeUpCursor];
      break;
    case TypeResizeUpDown:
      retval = [NSCursor resizeUpDownCursor];
      break;
    // The following four cursor types are only supported on OS X 10.6 and up.
    case TypeContextualMenu: {
      if ([nsCursorClass respondsToSelector:@selector(contextualMenuCursor)]) {
        retval = [nsCursorClass performSelector:@selector(contextualMenuCursor)];
      }
      break;
    }
    case TypeDragCopy: {
      if ([nsCursorClass respondsToSelector:@selector(dragCopyCursor)]) {
        retval = [nsCursorClass performSelector:@selector(dragCopyCursor)];
      }
      break;
    }
    case TypeDragLink: {
      if ([nsCursorClass respondsToSelector:@selector(dragLinkCursor)]) {
        retval = [nsCursorClass performSelector:@selector(dragLinkCursor)];
      }
      break;
    }
    case TypeNotAllowed: {
      if ([nsCursorClass respondsToSelector:@selector(operationNotAllowedCursor)]) {
        retval = [nsCursorClass performSelector:@selector(operationNotAllowedCursor)];
      }
      break;
    }
    case TypeTransparent:
      retval = GetTransparentCursor();
      break;
    default:
      break;
  }

  if (!retval && mCustomImageData && mCustomImageDataLength) {
    CGDataProviderRef provider = ::CGDataProviderCreateWithData(NULL,
                                                                (const void*)mCustomImageData,
                                                                mCustomImageDataLength,
                                                                NULL);
    if (provider) {
      CGImageRef cgImage = ::CGImageCreateWithPNGDataProvider(provider,
                                                              NULL,
                                                              false,
                                                              kCGRenderingIntentDefault);
      if (cgImage) {
        NSBitmapImageRep* rep = [[NSBitmapImageRep alloc] initWithCGImage:cgImage];
        if (rep) {
          NSImage* image = [[NSImage alloc] init];
          if (image) {
            [image addRepresentation:rep];
            retval = [[[NSCursor alloc] initWithImage:image 
                                              hotSpot:NSMakePoint(mHotSpot.x, mHotSpot.y)]
                      autorelease];
            [image release];
          }
          [rep release];
        }
        ::CGImageRelease(cgImage);
      }
      ::CFRelease(provider);
    }
  }

  // Fall back to an arrow cursor if need be.
  if (!retval) {
    retval = [NSCursor arrowCursor];
  }

  return retval;
}

// Get a transparent cursor with the appropriate hot spot.  We need one if
// (for example) we have a custom cursor with no image data.
NSCursor* NSCursorInfo::GetTransparentCursor() const
{
  NSCursor* retval = nil;

  int width = 16, height = 16;
  int bytesPerPixel = 2;
  int rowBytes = width * bytesPerPixel;
  int dataSize = height * rowBytes;

  uint8_t* data = (uint8_t*) moz_xmalloc(dataSize);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int offset = (y * rowBytes) + (x * bytesPerPixel);
      data[offset] = 0x7E;  // Arbitrary gray-scale value
      data[offset + 1] = 0; // Alpha value to make us transparent
    }
  }

  NSBitmapImageRep* imageRep =
    [[[NSBitmapImageRep alloc] initWithBitmapDataPlanes:nil
                                             pixelsWide:width
                                             pixelsHigh:height
                                          bitsPerSample:8
                                        samplesPerPixel:2
                                               hasAlpha:YES
                                               isPlanar:NO
                                         colorSpaceName:NSCalibratedWhiteColorSpace
                                            bytesPerRow:rowBytes
                                           bitsPerPixel:16]
     autorelease];
  if (imageRep) {
    uint8_t* repDataPtr = [imageRep bitmapData];
    if (repDataPtr) {
      memcpy(repDataPtr, data, dataSize);
      NSImage *image =
        [[[NSImage alloc] initWithSize:NSMakeSize(width, height)]
         autorelease];
      if (image) {
        [image addRepresentation:imageRep];
        retval =
          [[[NSCursor alloc] initWithImage:image
                                   hotSpot:NSMakePoint(mHotSpot.x, mHotSpot.y)]
           autorelease];
      }
    }
  }

  free(data);

  // Fall back to an arrow cursor if (for some reason) the above code failed.
  if (!retval) {
    retval = [NSCursor arrowCursor];
  }

  return retval;
}

NSCursorInfo::Type NSCursorInfo::GetType() const
{
  return mType;
}

const char* NSCursorInfo::GetTypeName() const
{
  switch(mType) {
    case TypeCustom:
      return "TypeCustom";
    case TypeArrow:
      return "TypeArrow";
    case TypeClosedHand:
      return "TypeClosedHand";
    case TypeContextualMenu:
      return "TypeContextualMenu";
    case TypeCrosshair:
      return "TypeCrosshair";
    case TypeDisappearingItem:
      return "TypeDisappearingItem";
    case TypeDragCopy:
      return "TypeDragCopy";
    case TypeDragLink:
      return "TypeDragLink";
    case TypeIBeam:
      return "TypeIBeam";
    case TypeNotAllowed:
      return "TypeNotAllowed";
    case TypeOpenHand:
      return "TypeOpenHand";
    case TypePointingHand:
      return "TypePointingHand";
    case TypeResizeDown:
      return "TypeResizeDown";
    case TypeResizeLeft:
      return "TypeResizeLeft";
    case TypeResizeLeftRight:
      return "TypeResizeLeftRight";
    case TypeResizeRight:
      return "TypeResizeRight";
    case TypeResizeUp:
      return "TypeResizeUp";
    case TypeResizeUpDown:
      return "TypeResizeUpDown";
    case TypeTransparent:
      return "TypeTransparent";
    default:
      break;
  }
  return "TypeUnknown";
}

nsPoint NSCursorInfo::GetHotSpot() const
{
  return mHotSpot;
}

uint8_t* NSCursorInfo::GetCustomImageData() const
{
  return mCustomImageData;
}

uint32_t NSCursorInfo::GetCustomImageDataLength() const
{
  return mCustomImageDataLength;
}

void NSCursorInfo::SetType(Type aType)
{
  mType = aType;
}

void NSCursorInfo::SetHotSpot(nsPoint aHotSpot)
{
  mHotSpot = aHotSpot;
}

void NSCursorInfo::SetCustomImageData(uint8_t* aData, uint32_t aDataLength)
{
  if (mCustomImageData) {
    free(mCustomImageData);
  }
  if (aDataLength) {
    mCustomImageData = (uint8_t*) moz_xmalloc(aDataLength);
    memcpy(mCustomImageData, aData, aDataLength);
  } else {
    mCustomImageData = NULL;
  }
  mCustomImageDataLength = aDataLength;
}

// This should never be called from the browser process -- only from the
// plugin process.
bool NSCursorInfo::GetNativeCursorsSupported()
{
  if (mNativeCursorsSupported == -1) {
    ENSURE_PLUGIN_THREAD(false);
    PluginModuleChild *pmc = PluginModuleChild::GetChrome();
    if (pmc) {
      bool result = pmc->GetNativeCursorsSupported();
      if (result) {
        mNativeCursorsSupported = 1;
      } else {
        mNativeCursorsSupported = 0;
      }
    }
  }
  return (mNativeCursorsSupported == 1);
}

} // namespace mac_plugin_interposing

namespace mac_plugin_interposing {
namespace parent {

// Tracks plugin windows currently visible.
std::set<uint32_t> plugin_visible_windows_set_;
// Tracks full screen windows currently visible.
std::set<uint32_t> plugin_fullscreen_windows_set_;
// Tracks modal windows currently visible.
std::set<uint32_t> plugin_modal_windows_set_;

void OnPluginShowWindow(uint32_t window_id,
                        CGRect window_bounds,
                        bool modal) {
  plugin_visible_windows_set_.insert(window_id);

  if (modal)
    plugin_modal_windows_set_.insert(window_id);

  CGRect main_display_bounds = ::CGDisplayBounds(CGMainDisplayID());

  if (CGRectEqualToRect(window_bounds, main_display_bounds) &&
      (plugin_fullscreen_windows_set_.find(window_id) ==
       plugin_fullscreen_windows_set_.end())) {
    plugin_fullscreen_windows_set_.insert(window_id);

    nsCocoaUtils::HideOSChromeOnScreen(true);
  }
}

static void ActivateProcess(pid_t pid) {
  ProcessSerialNumber process;
  OSStatus status = ::GetProcessForPID(pid, &process);

  if (status == noErr) {
    SetFrontProcess(&process);
  } else {
    NS_WARNING("Unable to get process for pid.");
  }
}

// Must be called on the UI thread.
// If plugin_pid is -1, the browser will be the active process on return,
// otherwise that process will be given focus back before this function returns.
static void ReleasePluginFullScreen(pid_t plugin_pid) {
  // Releasing full screen only works if we are the frontmost process; grab
  // focus, but give it back to the plugin process if requested.
  ActivateProcess(base::GetCurrentProcId());

  nsCocoaUtils::HideOSChromeOnScreen(false);

  if (plugin_pid != -1) {
    ActivateProcess(plugin_pid);
  }
}

void OnPluginHideWindow(uint32_t window_id, pid_t aPluginPid) {
  bool had_windows = !plugin_visible_windows_set_.empty();
  plugin_visible_windows_set_.erase(window_id);
  bool browser_needs_activation = had_windows &&
      plugin_visible_windows_set_.empty();

  plugin_modal_windows_set_.erase(window_id);
  if (plugin_fullscreen_windows_set_.find(window_id) !=
      plugin_fullscreen_windows_set_.end()) {
    plugin_fullscreen_windows_set_.erase(window_id);
    pid_t plugin_pid = browser_needs_activation ? -1 : aPluginPid;
    browser_needs_activation = false;
    ReleasePluginFullScreen(plugin_pid);
  }

  if (browser_needs_activation) {
    ActivateProcess(getpid());
  }
}

void OnSetCursor(const NSCursorInfo& cursorInfo)
{
  NSCursor* aCursor = cursorInfo.GetNSCursor();
  if (aCursor) {
    [aCursor set];
  }
}

void OnShowCursor(bool show)
{
  if (show) {
    [NSCursor unhide];
  } else {
    [NSCursor hide];
  }
}

void OnPushCursor(const NSCursorInfo& cursorInfo)
{
  NSCursor* aCursor = cursorInfo.GetNSCursor();
  if (aCursor) {
    [aCursor push];
  }
}

void OnPopCursor()
{
  [NSCursor pop];
}

} // namespace parent
} // namespace mac_plugin_interposing

namespace mac_plugin_interposing {
namespace child {

// TODO(stuartmorgan): Make this an IPC to order the plugin process above the
// browser process only if the browser is current frontmost.
void FocusPluginProcess() {
  ProcessSerialNumber this_process, front_process;
  if ((GetCurrentProcess(&this_process) != noErr) ||
      (GetFrontProcess(&front_process) != noErr)) {
    return;
  }

  Boolean matched = false;
  if ((SameProcess(&this_process, &front_process, &matched) == noErr) &&
      !matched) {
    SetFrontProcess(&this_process);
  }
}

void NotifyBrowserOfPluginShowWindow(uint32_t window_id, CGRect bounds,
                                     bool modal) {
  ENSURE_PLUGIN_THREAD_VOID();

  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc)
    pmc->PluginShowWindow(window_id, modal, bounds);
}

void NotifyBrowserOfPluginHideWindow(uint32_t window_id, CGRect bounds) {
  ENSURE_PLUGIN_THREAD_VOID();

  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc)
    pmc->PluginHideWindow(window_id);
}

void NotifyBrowserOfSetCursor(NSCursorInfo& aCursorInfo)
{
  ENSURE_PLUGIN_THREAD_VOID();
  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc) {
    pmc->SetCursor(aCursorInfo);
  }
}

void NotifyBrowserOfShowCursor(bool show)
{
  ENSURE_PLUGIN_THREAD_VOID();
  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc) {
    pmc->ShowCursor(show);
  }
}

void NotifyBrowserOfPushCursor(NSCursorInfo& aCursorInfo)
{
  ENSURE_PLUGIN_THREAD_VOID();
  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc) {
    pmc->PushCursor(aCursorInfo);
  }
}

void NotifyBrowserOfPopCursor()
{
  ENSURE_PLUGIN_THREAD_VOID();
  PluginModuleChild *pmc = PluginModuleChild::GetChrome();
  if (pmc) {
    pmc->PopCursor();
  }
}

struct WindowInfo {
  uint32_t window_id;
  CGRect bounds;
  explicit WindowInfo(NSWindow* aWindow) {
    NSInteger window_num = [aWindow windowNumber];
    window_id = window_num > 0 ? window_num : 0;
    bounds = NSRectToCGRect([aWindow frame]);
  }
};

static void OnPluginWindowClosed(const WindowInfo& window_info) {
  if (window_info.window_id == 0)
    return;
  mac_plugin_interposing::child::NotifyBrowserOfPluginHideWindow(window_info.window_id,
                                                                 window_info.bounds);
}

static void OnPluginWindowShown(const WindowInfo& window_info, BOOL is_modal) {
  // The window id is 0 if it has never been shown (including while it is the
  // process of being shown for the first time); when that happens, we'll catch
  // it in _setWindowNumber instead.
  static BOOL s_pending_display_is_modal = NO;
  if (window_info.window_id == 0) {
    if (is_modal)
      s_pending_display_is_modal = YES;
    return;
  }
  if (s_pending_display_is_modal) {
    is_modal = YES;
    s_pending_display_is_modal = NO;
  }
  mac_plugin_interposing::child::NotifyBrowserOfPluginShowWindow(
    window_info.window_id, window_info.bounds, is_modal);
}

static BOOL OnSetCursor(NSCursorInfo &aInfo)
{
  if (NSCursorInfo::GetNativeCursorsSupported()) {
    NotifyBrowserOfSetCursor(aInfo);
    return YES;
  }
  return NO;
}

static BOOL OnHideCursor()
{
  if (NSCursorInfo::GetNativeCursorsSupported()) {
    NotifyBrowserOfShowCursor(NO);
    return YES;
  }
  return NO;
}

static BOOL OnUnhideCursor()
{
  if (NSCursorInfo::GetNativeCursorsSupported()) {
    NotifyBrowserOfShowCursor(YES);
    return YES;
  }
  return NO;
}

static BOOL OnPushCursor(NSCursorInfo &aInfo)
{
  if (NSCursorInfo::GetNativeCursorsSupported()) {
    NotifyBrowserOfPushCursor(aInfo);
    return YES;
  }
  return NO;
}

static BOOL OnPopCursor()
{
  if (NSCursorInfo::GetNativeCursorsSupported()) {
    NotifyBrowserOfPopCursor();
    return YES;
  }
  return NO;
}

} // namespace child
} // namespace mac_plugin_interposing

using namespace mac_plugin_interposing::child;

@interface NSWindow (PluginInterposing)
- (void)pluginInterpose_orderOut:(id)sender;
- (void)pluginInterpose_orderFront:(id)sender;
- (void)pluginInterpose_makeKeyAndOrderFront:(id)sender;
- (void)pluginInterpose_setWindowNumber:(NSInteger)num;
@end

@implementation NSWindow (PluginInterposing)

- (void)pluginInterpose_orderOut:(id)sender {
  WindowInfo window_info(self);
  [self pluginInterpose_orderOut:sender];
  OnPluginWindowClosed(window_info);
}

- (void)pluginInterpose_orderFront:(id)sender {
  mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_orderFront:sender];
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)pluginInterpose_makeKeyAndOrderFront:(id)sender {
  mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_makeKeyAndOrderFront:sender];
  OnPluginWindowShown(WindowInfo(self), NO);
}

- (void)pluginInterpose_setWindowNumber:(NSInteger)num {
  if (num > 0)
    mac_plugin_interposing::child::FocusPluginProcess();
  [self pluginInterpose_setWindowNumber:num];
  if (num > 0)
    OnPluginWindowShown(WindowInfo(self), NO);
}

@end

@interface NSApplication (PluginInterposing)
- (NSInteger)pluginInterpose_runModalForWindow:(NSWindow*)window;
@end

@implementation NSApplication (PluginInterposing)

- (NSInteger)pluginInterpose_runModalForWindow:(NSWindow*)window {
  mac_plugin_interposing::child::FocusPluginProcess();
  // This is out-of-order relative to the other calls, but runModalForWindow:
  // won't return until the window closes, and the order only matters for
  // full-screen windows.
  OnPluginWindowShown(WindowInfo(window), YES);
  return [self pluginInterpose_runModalForWindow:window];
}

@end

// Hook commands to manipulate the current cursor, so that they can be passed
// from the child process to the parent process.  These commands have no
// effect unless they're performed in the parent process.
@interface NSCursor (PluginInterposing)
- (void)pluginInterpose_set;
- (void)pluginInterpose_push;
- (void)pluginInterpose_pop;
+ (NSCursor*)pluginInterpose_currentCursor;
+ (void)pluginInterpose_hide;
+ (void)pluginInterpose_unhide;
+ (void)pluginInterpose_pop;
@end

// Cache the results of [NSCursor set], [NSCursor push] and [NSCursor pop].
// The last element is always the current cursor.
static NSMutableArray* gCursorStack = nil;

static BOOL initCursorStack()
{
  if (!gCursorStack) {
    gCursorStack = [[NSMutableArray arrayWithCapacity:5] retain];
  }
  return (gCursorStack != NULL);
}

static NSCursor* currentCursorFromCache()
{
  if (!initCursorStack())
    return nil;
  return (NSCursor*) [gCursorStack lastObject];
}

static void setCursorInCache(NSCursor* aCursor)
{
  if (!initCursorStack() || !aCursor)
    return;
  NSUInteger count = [gCursorStack count];
  if (count) {
    [gCursorStack replaceObjectAtIndex:count - 1 withObject:aCursor];
  } else {
    [gCursorStack addObject:aCursor];
  }
}

static void pushCursorInCache(NSCursor* aCursor)
{
  if (!initCursorStack() || !aCursor)
    return;
  [gCursorStack addObject:aCursor];
}

static void popCursorInCache()
{
  if (!initCursorStack())
    return;
  // Apple's doc on the +[NSCursor pop] method says:  "If the current cursor
  // is the only cursor on the stack, this method does nothing."
  if ([gCursorStack count] > 1) {
    [gCursorStack removeLastObject];
  }
}

@implementation NSCursor (PluginInterposing)

- (void)pluginInterpose_set
{
  NSCursorInfo info(self);
  OnSetCursor(info);
  setCursorInCache(self);
  [self pluginInterpose_set];
}

- (void)pluginInterpose_push
{
  NSCursorInfo info(self);
  OnPushCursor(info);
  pushCursorInCache(self);
  [self pluginInterpose_push];
}

- (void)pluginInterpose_pop
{
  OnPopCursor();
  popCursorInCache();
  [self pluginInterpose_pop];
}

// The currentCursor method always returns nil when running in a background
// process.  But this may confuse plugins (notably Flash, see bug 621117).  So
// if we get a nil return from the "call to super", we return a cursor that's
// been cached by previous calls to set or push.  According to Apple's docs,
// currentCursor "only returns the cursor set by your application using
// NSCursor methods".  So we don't need to worry about changes to the cursor
// made by other methods like SetThemeCursor().
+ (NSCursor*)pluginInterpose_currentCursor
{
  NSCursor* retval = [self pluginInterpose_currentCursor];
  if (!retval) {
    retval = currentCursorFromCache();
  }
  return retval;
}

+ (void)pluginInterpose_hide
{
  OnHideCursor();
  [self pluginInterpose_hide];
}

+ (void)pluginInterpose_unhide
{
  OnUnhideCursor();
  [self pluginInterpose_unhide];
}

+ (void)pluginInterpose_pop
{
  OnPopCursor();
  popCursorInCache();
  [self pluginInterpose_pop];
}

@end

static void ExchangeMethods(Class target_class,
                            BOOL class_method,
                            SEL original,
                            SEL replacement) {
  Method m1;
  Method m2;
  if (class_method) {
    m1 = class_getClassMethod(target_class, original);
    m2 = class_getClassMethod(target_class, replacement);
  } else {
    m1 = class_getInstanceMethod(target_class, original);
    m2 = class_getInstanceMethod(target_class, replacement);
  }

  if (m1 == m2)
    return;

  if (m1 && m2)
    method_exchangeImplementations(m1, m2);
  else
    MOZ_ASSERT_UNREACHABLE("Cocoa swizzling failed");
}

namespace mac_plugin_interposing {
namespace child {

void SetUpCocoaInterposing() {
  Class nswindow_class = [NSWindow class];
  ExchangeMethods(nswindow_class, NO, @selector(orderOut:),
                  @selector(pluginInterpose_orderOut:));
  ExchangeMethods(nswindow_class, NO, @selector(orderFront:),
                  @selector(pluginInterpose_orderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(makeKeyAndOrderFront:),
                  @selector(pluginInterpose_makeKeyAndOrderFront:));
  ExchangeMethods(nswindow_class, NO, @selector(_setWindowNumber:),
                  @selector(pluginInterpose_setWindowNumber:));

  ExchangeMethods([NSApplication class], NO, @selector(runModalForWindow:),
                  @selector(pluginInterpose_runModalForWindow:));

  Class nscursor_class = [NSCursor class];
  ExchangeMethods(nscursor_class, NO, @selector(set),
                  @selector(pluginInterpose_set));
  ExchangeMethods(nscursor_class, NO, @selector(push),
                  @selector(pluginInterpose_push));
  ExchangeMethods(nscursor_class, NO, @selector(pop),
                  @selector(pluginInterpose_pop));
  ExchangeMethods(nscursor_class, YES, @selector(currentCursor),
                  @selector(pluginInterpose_currentCursor));
  ExchangeMethods(nscursor_class, YES, @selector(hide),
                  @selector(pluginInterpose_hide));
  ExchangeMethods(nscursor_class, YES, @selector(unhide),
                  @selector(pluginInterpose_unhide));
  ExchangeMethods(nscursor_class, YES, @selector(pop),
                  @selector(pluginInterpose_pop));
}

}  // namespace child
}  // namespace mac_plugin_interposing

// Called from plugin_child_interpose.mm, which hooks calls to
// SetCursor() (the QuickDraw call) from the plugin child process.
extern "C" NS_VISIBILITY_DEFAULT BOOL
mac_plugin_interposing_child_OnSetCursor(const Cursor* cursor)
{
  NSCursorInfo info(cursor);
  return OnSetCursor(info);
}

// Called from plugin_child_interpose.mm, which hooks calls to
// SetThemeCursor() (the Appearance Manager call) from the plugin child
// process.
extern "C" NS_VISIBILITY_DEFAULT BOOL
mac_plugin_interposing_child_OnSetThemeCursor(ThemeCursor cursor)
{
  NSCursorInfo info;
  switch (cursor) {
    case kThemeArrowCursor:
      info.SetType(NSCursorInfo::TypeArrow);
      break;
    case kThemeCopyArrowCursor:
      info.SetType(NSCursorInfo::TypeDragCopy);
      break;
    case kThemeAliasArrowCursor:
      info.SetType(NSCursorInfo::TypeDragLink);
      break;
    case kThemeContextualMenuArrowCursor:
      info.SetType(NSCursorInfo::TypeContextualMenu);
      break;
    case kThemeIBeamCursor:
      info.SetType(NSCursorInfo::TypeIBeam);
      break;
    case kThemeCrossCursor:
    case kThemePlusCursor:
      info.SetType(NSCursorInfo::TypeCrosshair);
      break;
    case kThemeWatchCursor:
    case kThemeSpinningCursor:
      info.SetType(NSCursorInfo::TypeArrow);
      break;
    case kThemeClosedHandCursor:
      info.SetType(NSCursorInfo::TypeClosedHand);
      break;
    case kThemeOpenHandCursor:
      info.SetType(NSCursorInfo::TypeOpenHand);
      break;
    case kThemePointingHandCursor:
    case kThemeCountingUpHandCursor:
    case kThemeCountingDownHandCursor:
    case kThemeCountingUpAndDownHandCursor:
      info.SetType(NSCursorInfo::TypePointingHand);
      break;
    case kThemeResizeLeftCursor:
      info.SetType(NSCursorInfo::TypeResizeLeft);
      break;
    case kThemeResizeRightCursor:
      info.SetType(NSCursorInfo::TypeResizeRight);
      break;
    case kThemeResizeLeftRightCursor:
      info.SetType(NSCursorInfo::TypeResizeLeftRight);
      break;
    case kThemeNotAllowedCursor:
      info.SetType(NSCursorInfo::TypeNotAllowed);
      break;
    case kThemeResizeUpCursor:
      info.SetType(NSCursorInfo::TypeResizeUp);
      break;
    case kThemeResizeDownCursor:
      info.SetType(NSCursorInfo::TypeResizeDown);
      break;
    case kThemeResizeUpDownCursor:
      info.SetType(NSCursorInfo::TypeResizeUpDown);
      break;
    case kThemePoofCursor:
      info.SetType(NSCursorInfo::TypeDisappearingItem);
      break;
    default:
      info.SetType(NSCursorInfo::TypeArrow);
      break;
  }
  return OnSetCursor(info);
}

extern "C" NS_VISIBILITY_DEFAULT BOOL
mac_plugin_interposing_child_OnHideCursor()
{
  return OnHideCursor();
}

extern "C" NS_VISIBILITY_DEFAULT BOOL
mac_plugin_interposing_child_OnShowCursor()
{
  return OnUnhideCursor();
}
