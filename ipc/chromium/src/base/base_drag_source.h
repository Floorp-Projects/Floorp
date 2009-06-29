// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_DRAG_SOURCE_H_
#define BASE_BASE_DRAG_SOURCE_H_

#include <objidl.h>

#include "base/basictypes.h"

///////////////////////////////////////////////////////////////////////////////
//
// BaseDragSource
//
//  A base IDropSource implementation. Handles notifications sent by an active
//  drag-drop operation as the user mouses over other drop targets on their
//  system. This object tells Windows whether or not the drag should continue,
//  and supplies the appropriate cursors.
//
class BaseDragSource : public IDropSource {
 public:
  BaseDragSource();
  virtual ~BaseDragSource() { }

  // IDropSource implementation:
  HRESULT __stdcall QueryContinueDrag(BOOL escape_pressed, DWORD key_state);
  HRESULT __stdcall GiveFeedback(DWORD effect);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 protected:
  virtual void OnDragSourceCancel() { }
  virtual void OnDragSourceDrop() { }
  virtual void OnDragSourceMove() { }

 private:
  LONG ref_count_;

  DISALLOW_EVIL_CONSTRUCTORS(BaseDragSource);
};

#endif  // BASE_BASE_DRAG_SOURCE_H_
