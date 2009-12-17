// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_BASE_DROP_TARGET_H_
#define BASE_BASE_DROP_TARGET_H_

#include <objidl.h>

#include "base/ref_counted.h"

struct IDropTargetHelper;

// A DropTarget implementation that takes care of the nitty gritty
// of dnd. While this class is concrete, subclasses will most likely
// want to override various OnXXX methods.
//
// Because BaseDropTarget is ref counted you shouldn't delete it directly,
// rather wrap it in a scoped_refptr. Be sure and invoke RevokeDragDrop(m_hWnd)
// before the HWND is deleted too.
//
// This class is meant to be used in a STA and is not multithread-safe.
class BaseDropTarget : public IDropTarget {
 public:
  // Create a new BaseDropTarget associating it with the given HWND.
  explicit BaseDropTarget(HWND hwnd);
  virtual ~BaseDropTarget();

  // When suspend is set to |true|, the drop target does not receive drops from
  // drags initiated within the owning HWND.
  // TODO(beng): (http://b/1085385) figure out how we will handle legitimate
  //             drag-drop operations within the same HWND, such as dragging
  //             selected text to an edit field.
  void set_suspend(bool suspend) { suspend_ = suspend; }

  // IDropTarget implementation:
  HRESULT __stdcall DragEnter(IDataObject* data_object,
                              DWORD key_state,
                              POINTL cursor_position,
                              DWORD* effect);
  HRESULT __stdcall DragOver(DWORD key_state,
                             POINTL cursor_position,
                             DWORD* effect);
  HRESULT __stdcall DragLeave();
  HRESULT __stdcall Drop(IDataObject* data_object,
                         DWORD key_state,
                         POINTL cursor_position,
                         DWORD* effect);

  // IUnknown implementation:
  HRESULT __stdcall QueryInterface(const IID& iid, void** object);
  ULONG __stdcall AddRef();
  ULONG __stdcall Release();

 protected:
  // Returns the hosting HWND.
  HWND GetHWND() { return hwnd_; }

  // Invoked when the cursor first moves over the hwnd during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragEnter(IDataObject* data_object,
                            DWORD key_state,
                            POINT cursor_position,
                            DWORD effect);

  // Invoked when the cursor moves over the window during a dnd session.
  // This should return a bitmask of the supported drop operations:
  // DROPEFFECT_NONE, DROPEFFECT_COPY, DROPEFFECT_LINK and/or
  // DROPEFFECT_MOVE.
  virtual DWORD OnDragOver(IDataObject* data_object,
                           DWORD key_state,
                           POINT cursor_position,
                           DWORD effect);

  // Invoked when the cursor moves outside the bounds of the hwnd during a
  // dnd session.
  virtual void OnDragLeave(IDataObject* data_object);

  // Invoked when the drop ends on the window. This should return the operation
  // that was taken.
  virtual DWORD OnDrop(IDataObject* data_object,
                       DWORD key_state,
                       POINT cursor_position,
                       DWORD effect);

  // Return the drag identity.
  static int32 GetDragIdentity() { return drag_identity_; }

 private:
  // Returns the cached drop helper, creating one if necessary. The returned
  // object is not addrefed. May return NULL if the object couldn't be created.
  static IDropTargetHelper* DropHelper();

  // The data object currently being dragged over this drop target.
  scoped_refptr<IDataObject> current_data_object_;

  // A helper object that is used to provide drag image support while the mouse
  // is dragging over the content area.
  //
  // DO NOT ACCESS DIRECTLY! Use DropHelper() instead, which will lazily create
  // this if it doesn't exist yet. This object can take tens of milliseconds to
  // create, and we don't want to block any window opening for this, especially
  // since often, DnD will never be used. Instead, we force this penalty to the
  // first time it is actually used.
  static IDropTargetHelper* cached_drop_target_helper_;

  // The drag identity (id). An up-counter that increases when the cursor first
  // moves over the HWND in a DnD session (OnDragEnter). 0 is reserved to mean
  // the "no/unknown" identity, and is used for initialization. The identity is
  // sent to the renderer in drag enter notifications. Note: the identity value
  // is passed over the renderer NPAPI interface to gears, so use int32 instead
  // of int here.
  static int32 drag_identity_;

  // The HWND of the source. This HWND is used to determine coordinates for
  // mouse events that are sent to the renderer notifying various drag states.
  HWND hwnd_;

  // Whether or not we are currently processing drag notifications for drags
  // initiated in this window.
  bool suspend_;

  LONG ref_count_;

  DISALLOW_EVIL_CONSTRUCTORS(BaseDropTarget);
};

#endif  // BASE_BASE_DROP_TARGET_H_
