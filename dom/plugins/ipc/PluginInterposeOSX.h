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

#ifndef DOM_PLUGINS_IPC_PLUGININTERPOSEOSX_H
#define DOM_PLUGINS_IPC_PLUGININTERPOSEOSX_H

#include "base/basictypes.h"
#include "nsPoint.h"
#include "npapi.h"

// Make this includable from non-Objective-C code.
#ifndef __OBJC__
class NSCursor;
#else
#import <Cocoa/Cocoa.h>
#endif

// The header file QuickdrawAPI.h is missing on OS X 10.7 and up (though the
// QuickDraw APIs defined in it are still present) -- so we need to supply the
// relevant parts of its contents here.  It's likely that Apple will eventually
// remove the APIs themselves (probably in OS X 10.8), so we need to make them
// weak imports, and test for their presence before using them.
#if !defined(__QUICKDRAWAPI__)

typedef short Bits16[16];
struct Cursor {
  Bits16  data;
  Bits16  mask;
  Point   hotSpot;
};
typedef struct Cursor Cursor;

#endif /* __QUICKDRAWAPI__ */

namespace mac_plugin_interposing {

// Class used to serialize NSCursor objects over IPC between processes.
class NSCursorInfo {
public:
  enum Type {
    TypeCustom,
    TypeArrow,
    TypeClosedHand,
    TypeContextualMenu,   // Only supported on OS X 10.6 and up
    TypeCrosshair,
    TypeDisappearingItem,
    TypeDragCopy,         // Only supported on OS X 10.6 and up
    TypeDragLink,         // Only supported on OS X 10.6 and up
    TypeIBeam,
    TypeNotAllowed,       // Only supported on OS X 10.6 and up
    TypeOpenHand,
    TypePointingHand,
    TypeResizeDown,
    TypeResizeLeft,
    TypeResizeLeftRight,
    TypeResizeRight,
    TypeResizeUp,
    TypeResizeUpDown,
    TypeTransparent       // Special type
  };

  NSCursorInfo();
  NSCursorInfo(NSCursor* aCursor);
  NSCursorInfo(const Cursor* aCursor);
  ~NSCursorInfo();

  NSCursor* GetNSCursor() const;
  Type GetType() const;
  const char* GetTypeName() const;
  nsPoint GetHotSpot() const;
  uint8_t* GetCustomImageData() const;
  uint32_t GetCustomImageDataLength() const;

  void SetType(Type aType);
  void SetHotSpot(nsPoint aHotSpot);
  void SetCustomImageData(uint8_t* aData, uint32_t aDataLength);

  static bool GetNativeCursorsSupported();

private:
  NSCursor* GetTransparentCursor() const;

  Type mType;
  // The hot spot's coordinate system is the cursor's coordinate system, and
  // has an upper-left origin (in both Cocoa and pre-Cocoa systems).
  nsPoint mHotSpot;
  uint8_t* mCustomImageData;
  uint32_t mCustomImageDataLength;
  static int32_t mNativeCursorsSupported;
};

namespace parent {

void OnPluginShowWindow(uint32_t window_id, CGRect window_bounds, bool modal);
void OnPluginHideWindow(uint32_t window_id, pid_t aPluginPid);
void OnSetCursor(const NSCursorInfo& cursorInfo);
void OnShowCursor(bool show);
void OnPushCursor(const NSCursorInfo& cursorInfo);
void OnPopCursor();

}

namespace child {

void SetUpCocoaInterposing();

}

}

#endif /* DOM_PLUGINS_IPC_PLUGININTERPOSEOSX_H */
