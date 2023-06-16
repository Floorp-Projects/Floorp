/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "DocAccessibleChild.h"

#include "LocalAccessible-inl.h"

namespace mozilla {
namespace a11y {

DocAccessibleChild::DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager)
    : DocAccessibleChildBase(aDoc) {
  MOZ_COUNT_CTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
  SetManager(aManager);
}

DocAccessibleChild::~DocAccessibleChild() {
  MOZ_COUNT_DTOR_INHERITED(DocAccessibleChild, DocAccessibleChildBase);
}

LayoutDeviceIntRect DocAccessibleChild::GetCaretRectFor(const uint64_t& aID) {
  LocalAccessible* target;

  if (aID) {
    target = reinterpret_cast<LocalAccessible*>(aID);
  } else {
    target = mDoc;
  }

  MOZ_ASSERT(target);

  HyperTextAccessible* text = target->AsHyperText();
  if (!text) {
    return LayoutDeviceIntRect();
  }

  nsIWidget* widget = nullptr;
  return text->GetCaretRect(&widget);
}

bool DocAccessibleChild::SendFocusEvent(const uint64_t& aID) {
  return PDocAccessibleChild::SendFocusEvent(aID, GetCaretRectFor(aID));
}

bool DocAccessibleChild::SendCaretMoveEvent(const uint64_t& aID,
                                            const int32_t& aOffset,
                                            const bool& aIsSelectionCollapsed,
                                            const bool& aIsAtEndOfLine,
                                            const int32_t& aGranularity) {
  return PDocAccessibleChild::SendCaretMoveEvent(aID, GetCaretRectFor(aID),
                                                 aOffset, aIsSelectionCollapsed,
                                                 aIsAtEndOfLine, aGranularity);
}

ipc::IPCResult DocAccessibleChild::RecvRestoreFocus() {
  if (FocusManager* focusMgr = FocusMgr()) {
    focusMgr->ForceFocusEvent();
  }
  return IPC_OK();
}

}  // namespace a11y
}  // namespace mozilla
