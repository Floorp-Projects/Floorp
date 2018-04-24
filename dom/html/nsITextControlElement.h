/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsITextControlElement_h___
#define nsITextControlElement_h___

#include "nsISupports.h"
#include "nsCOMPtr.h"
#include "nsStringFwd.h"
class nsIContent;
class nsISelectionController;
class nsFrameSelection;
class nsTextControlFrame;

namespace mozilla {

class ErrorResult;
class TextEditor;

namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

// IID for the nsITextControl interface
#define NS_ITEXTCONTROLELEMENT_IID    \
{ 0x3df7db6d, 0xa548, 0x4e20, \
 { 0x97, 0xfd, 0x75, 0xa3, 0x31, 0xa2, 0xf3, 0xd4 } }

/**
 * This interface is used for the text control frame to get the editor and
 * selection controller objects, and some helper properties.
 */
class nsITextControlElement : public nsISupports {
public:

  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ITEXTCONTROLELEMENT_IID)

  /**
   * Tell the control that value has been deliberately changed (or not).
   */
  NS_IMETHOD SetValueChanged(bool changed) = 0;

  /**
   * Find out whether this is a single line text control.  (text or password)
   * @return whether this is a single line text control
   */
  NS_IMETHOD_(bool) IsSingleLineTextControl() const = 0;

  /**
   * Find out whether this control is a textarea.
   * @return whether this is a textarea text control
   */
  NS_IMETHOD_(bool) IsTextArea() const = 0;

  /**
   * Find out whether this is a password control (input type=password)
   * @return whether this is a password ontrol
   */
  NS_IMETHOD_(bool) IsPasswordTextControl() const = 0;

  /**
   * Get the cols attribute (if textarea) or a default
   * @return the number of columns to use
   */
  NS_IMETHOD_(int32_t) GetCols() = 0;

  /**
   * Get the column index to wrap at, or -1 if we shouldn't wrap
   */
  NS_IMETHOD_(int32_t) GetWrapCols() = 0;

  /**
   * Get the rows attribute (if textarea) or a default
   * @return the number of rows to use
   */
  NS_IMETHOD_(int32_t) GetRows() = 0;

  /**
   * Get the default value of the text control
   */
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue) = 0;

  /**
   * Return true if the value of the control has been changed.
   */
  NS_IMETHOD_(bool) ValueChanged() const = 0;

  /**
   * Get the current value of the text editor.
   *
   * @param aValue the buffer to retrieve the value in
   * @param aIgnoreWrap whether to ignore the text wrapping behavior specified
   * for the element.
   */
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, bool aIgnoreWrap) const = 0;

  /**
   * Get the editor object associated with the text editor.
   * The return value is null if the control does not support an editor
   * (for example, if it is a checkbox.)
   */
  NS_IMETHOD_(mozilla::TextEditor*) GetTextEditor() = 0;

  /**
   * Get the selection controller object associated with the text editor.
   * The return value is null if the control does not support an editor
   * (for example, if it is a checkbox.)
   */
  NS_IMETHOD_(nsISelectionController*) GetSelectionController() = 0;

  NS_IMETHOD_(nsFrameSelection*) GetConstFrameSelection() = 0;

  /**
   * Binds a frame to the text control.  This is performed when a frame
   * is created for the content node.
   */
  NS_IMETHOD BindToFrame(nsTextControlFrame* aFrame) = 0;

  /**
   * Unbinds a frame from the text control.  This is performed when a frame
   * belonging to a content node is destroyed.
   */
  NS_IMETHOD_(void) UnbindFromFrame(nsTextControlFrame* aFrame) = 0;

  /**
   * Creates an editor for the text control.  This should happen when
   * a frame has been created for the text control element, but the created
   * editor may outlive the frame itself.
   */
  NS_IMETHOD CreateEditor() = 0;

  /**
   * Update preview value for the text control.
   */
  NS_IMETHOD_(void) SetPreviewValue(const nsAString& aValue) = 0;

  /**
   * Get the current preview value for text control.
   */
  NS_IMETHOD_(void) GetPreviewValue(nsAString& aValue) = 0;

  /**
   * Enable preview for text control.
   */
  NS_IMETHOD_(void) EnablePreview() = 0;

  /**
   * Find out whether this control enables preview for form autofoll.
   */
  NS_IMETHOD_(bool) IsPreviewEnabled() = 0;

  /**
   * Initialize the keyboard event listeners.
   */
  NS_IMETHOD_(void) InitializeKeyboardEventListeners() = 0;

  /**
   * Update the visibility of both the placholder and preview text based on the element's state.
   */
  NS_IMETHOD_(void) UpdateOverlayTextVisibility(bool aNotify) = 0;

  /**
   * Returns the current expected placeholder visibility state.
   */
  NS_IMETHOD_(bool) GetPlaceholderVisibility() = 0;

  /**
   * Returns the current expected preview visibility state.
   */
  NS_IMETHOD_(bool) GetPreviewVisibility() = 0;

  /**
   * Callback called whenever the value is changed.
   */
  NS_IMETHOD_(void) OnValueChanged(bool aNotify, bool aWasInteractiveUserChange) = 0;

  /**
   * Helpers for value manipulation from SetRangeText.
   */
  virtual void GetValueFromSetRangeText(nsAString& aValue) = 0;
  virtual nsresult SetValueFromSetRangeText(const nsAString& aValue) = 0;

  static const int32_t DEFAULT_COLS = 20;
  static const int32_t DEFAULT_ROWS = 1;
  static const int32_t DEFAULT_ROWS_TEXTAREA = 2;
  static const int32_t DEFAULT_UNDO_CAP = 1000;

  // wrap can be one of these three values.
  typedef enum {
    eHTMLTextWrap_Off     = 1,    // "off"
    eHTMLTextWrap_Hard    = 2,    // "hard"
    eHTMLTextWrap_Soft    = 3     // the default
  } nsHTMLTextWrap;

  static bool
  GetWrapPropertyEnum(nsIContent* aContent, nsHTMLTextWrap& aWrapProp);

  /**
   * Does the editor have a selection cache?
   *
   * Note that this function has the side effect of making the editor for input
   * elements be initialized eagerly.
   */
  NS_IMETHOD_(bool) HasCachedSelection() = 0;

  static already_AddRefed<nsITextControlElement>
  GetTextControlElementFromEditingHost(nsIContent* aHost);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITextControlElement,
                              NS_ITEXTCONTROLELEMENT_IID)

#endif // nsITextControlElement_h___

