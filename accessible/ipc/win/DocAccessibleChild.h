/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_a11y_DocAccessibleChild_h
#define mozilla_a11y_DocAccessibleChild_h

#include "mozilla/a11y/DocAccessibleChildBase.h"

namespace mozilla {
namespace a11y {

/*
 * These objects handle content side communication for an accessible document,
 * and their lifetime is the same as the document they represent.
 */
class DocAccessibleChild : public DocAccessibleChildBase {
 public:
  DocAccessibleChild(DocAccessible* aDoc, IProtocol* aManager);
  ~DocAccessibleChild();

  virtual ipc::IPCResult RecvRestoreFocus() override;

  bool SendCaretMoveEvent(const uint64_t& aID, const int32_t& aOffset,
                          const bool& aIsSelectionCollapsed,
                          const bool& aIsAtEndOfLine,
                          const int32_t& aGranularity);
  bool SendFocusEvent(const uint64_t& aID);

 private:
  LayoutDeviceIntRect GetCaretRectFor(const uint64_t& aID);
};

}  // namespace a11y
}  // namespace mozilla

#endif  // mozilla_a11y_DocAccessibleChild_h
