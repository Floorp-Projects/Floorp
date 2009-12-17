// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base_drop_target.h"

#include <shlobj.h>

#include "base/logging.h"

///////////////////////////////////////////////////////////////////////////////

IDropTargetHelper* BaseDropTarget::cached_drop_target_helper_ = NULL;
int32 BaseDropTarget::drag_identity_ = 0;

BaseDropTarget::BaseDropTarget(HWND hwnd)
    : hwnd_(hwnd),
      suspend_(false),
      ref_count_(0) {
  DCHECK(hwnd);
  HRESULT result = RegisterDragDrop(hwnd, this);
  DCHECK(SUCCEEDED(result));
}

BaseDropTarget::~BaseDropTarget() {
}

// static
IDropTargetHelper* BaseDropTarget::DropHelper() {
  if (!cached_drop_target_helper_) {
    CoCreateInstance(CLSID_DragDropHelper, 0, CLSCTX_INPROC_SERVER,
                     IID_IDropTargetHelper,
                     reinterpret_cast<void**>(&cached_drop_target_helper_));
  }
  return cached_drop_target_helper_;
}

///////////////////////////////////////////////////////////////////////////////
// BaseDropTarget, IDropTarget implementation:

HRESULT BaseDropTarget::DragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINTL cursor_position,
                                  DWORD* effect) {
  // Tell the helper that we entered so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->DragEnter(GetHWND(), data_object,
                           reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  // You can't drag and drop within the same HWND.
  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  // Update the drag identity, skipping 0.
  if (++drag_identity_ == 0)
    ++drag_identity_;

  current_data_object_ = data_object;
  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDragEnter(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT BaseDropTarget::DragOver(DWORD key_state,
                                 POINTL cursor_position,
                                 DWORD* effect) {
  // Tell the helper that we moved over it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragOver(reinterpret_cast<POINT*>(&cursor_position), *effect);

  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDragOver(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

HRESULT BaseDropTarget::DragLeave() {
  // Tell the helper that we moved out of it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper)
    drop_helper->DragLeave();

  OnDragLeave(current_data_object_);

  current_data_object_ = NULL;
  return S_OK;
}

HRESULT BaseDropTarget::Drop(IDataObject* data_object,
                             DWORD key_state,
                             POINTL cursor_position,
                             DWORD* effect) {
  // Tell the helper that we dropped onto it so it can update the drag image.
  IDropTargetHelper* drop_helper = DropHelper();
  if (drop_helper) {
    drop_helper->Drop(current_data_object_,
                      reinterpret_cast<POINT*>(&cursor_position), *effect);
  }

  if (suspend_) {
    *effect = DROPEFFECT_NONE;
    return S_OK;
  }

  POINT screen_pt = { cursor_position.x, cursor_position.y };
  *effect = OnDrop(current_data_object_, key_state, screen_pt, *effect);
  return S_OK;
}

///////////////////////////////////////////////////////////////////////////////
// BaseDropTarget, IUnknown implementation:

HRESULT BaseDropTarget::QueryInterface(const IID& iid, void** object) {
  *object = NULL;
  if (IsEqualIID(iid, IID_IUnknown) || IsEqualIID(iid, IID_IDropTarget)) {
    *object = this;
  } else {
    return E_NOINTERFACE;
  }
  AddRef();
  return S_OK;
}

ULONG BaseDropTarget::AddRef() {
  return ++ref_count_;
}

ULONG BaseDropTarget::Release() {
  if (--ref_count_ == 0) {
    delete this;
    return 0U;
  }
  return ref_count_;
}

DWORD BaseDropTarget::OnDragEnter(IDataObject* data_object,
                                  DWORD key_state,
                                  POINT cursor_position,
                                  DWORD effect) {
  return DROPEFFECT_NONE;
}

DWORD BaseDropTarget::OnDragOver(IDataObject* data_object,
                                 DWORD key_state,
                                 POINT cursor_position,
                                 DWORD effect) {
  return DROPEFFECT_NONE;
}

void BaseDropTarget::OnDragLeave(IDataObject* data_object) {
}

DWORD BaseDropTarget::OnDrop(IDataObject* data_object,
                             DWORD key_state,
                             POINT cursor_position,
                             DWORD effect) {
  return DROPEFFECT_NONE;
}
