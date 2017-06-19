/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsITextControlFrame_h___
#define nsITextControlFrame_h___
 
#include "nsIFormControlFrame.h"

class nsISelectionController;
class nsFrameSelection;

namespace mozilla {
class TextEditor;
} // namespace mozilla

class nsITextControlFrame : public nsIFormControlFrame
{
public:
  NS_DECL_QUERYFRAME_TARGET(nsITextControlFrame)

  enum SelectionDirection {
    eNone,
    eForward,
    eBackward
  };

  NS_IMETHOD_(already_AddRefed<mozilla::TextEditor>) GetTextEditor() = 0;

  NS_IMETHOD    SetSelectionRange(uint32_t aSelectionStart,
                                  uint32_t aSelectionEnd,
                                  SelectionDirection aDirection = eNone) = 0;

  NS_IMETHOD    GetOwnedSelectionController(nsISelectionController** aSelCon) = 0;
  virtual nsFrameSelection* GetOwnedFrameSelection() = 0;

  /**
   * Ensure editor is initialized with the proper flags and the default value.
   * @throws NS_ERROR_NOT_INITIALIZED if mEditor has not been created
   * @throws various and sundry other things
   */
  virtual nsresult EnsureEditorInitialized() = 0;

  virtual nsresult ScrollSelectionIntoView() = 0;
};

#endif
