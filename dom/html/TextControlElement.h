/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_TextControlElement_h
#define mozilla_TextControlElement_h

#include "mozilla/dom/FromParser.h"
#include "mozilla/dom/NodeInfo.h"
#include "nsGenericHTMLElement.h"

class nsIContent;
class nsISelectionController;
class nsFrameSelection;
class nsTextControlFrame;

namespace mozilla {

class ErrorResult;
class TextControlState;
class TextEditor;

/**
 * This abstract class is used for the text control frame to get the editor and
 * selection controller objects, and some helper properties.
 */
class TextControlElement : public nsGenericHTMLFormElementWithState {
 public:
  TextControlElement(already_AddRefed<dom::NodeInfo>&& aNodeInfo,
                     dom::FromParser aFromParser, uint8_t aType)
      : nsGenericHTMLFormElementWithState(std::move(aNodeInfo), aFromParser,
                                          aType){};

  NS_DECL_ISUPPORTS_INHERITED
  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED(TextControlElement,
                                           nsGenericHTMLFormElementWithState)

  bool IsTextControlElement() const final { return true; }

  NS_IMPL_FROMNODE_HELPER(TextControlElement, IsTextControlElement())

  /**
   * Tell the control that value has been deliberately changed (or not).
   */
  virtual nsresult SetValueChanged(bool changed) = 0;

  /**
   * Find out whether this is a single line text control.  (text or password)
   * @return whether this is a single line text control
   */
  virtual bool IsSingleLineTextControl() const = 0;

  /**
   * Find out whether this control is a textarea.
   * @return whether this is a textarea text control
   */
  virtual bool IsTextArea() const = 0;

  /**
   * Find out whether this is a password control (input type=password)
   * @return whether this is a password ontrol
   */
  virtual bool IsPasswordTextControl() const = 0;

  /**
   * Get the cols attribute (if textarea) or a default
   * @return the number of columns to use
   */
  virtual int32_t GetCols() = 0;

  /**
   * Get the column index to wrap at, or -1 if we shouldn't wrap
   */
  virtual int32_t GetWrapCols() = 0;

  /**
   * Get the rows attribute (if textarea) or a default
   * @return the number of rows to use
   */
  virtual int32_t GetRows() = 0;

  /**
   * Get the default value of the text control
   */
  virtual void GetDefaultValueFromContent(nsAString& aValue) = 0;

  /**
   * Return true if the value of the control has been changed.
   */
  virtual bool ValueChanged() const = 0;

  /**
   * Returns the used maxlength attribute value.
   */
  virtual int32_t UsedMaxLength() const = 0;

  /**
   * Get the current value of the text editor.
   *
   * @param aValue the buffer to retrieve the value in
   * @param aIgnoreWrap whether to ignore the text wrapping behavior specified
   * for the element.
   */
  virtual void GetTextEditorValue(nsAString& aValue,
                                  bool aIgnoreWrap) const = 0;

  /**
   * Get the editor object associated with the text editor.
   * The return value is null if the control does not support an editor
   * (for example, if it is a checkbox.)
   * Note that GetTextEditor() creates editor if it hasn't been created yet.
   * If you need editor only when the editor is there, you should use
   * GetTextEditorWithoutCreation().
   */
  MOZ_CAN_RUN_SCRIPT virtual TextEditor* GetTextEditor() = 0;
  virtual TextEditor* GetTextEditorWithoutCreation() = 0;

  /**
   * Get the selection controller object associated with the text editor.
   * The return value is null if the control does not support an editor
   * (for example, if it is a checkbox.)
   */
  virtual nsISelectionController* GetSelectionController() = 0;

  virtual nsFrameSelection* GetConstFrameSelection() = 0;

  virtual TextControlState* GetTextControlState() const = 0;

  /**
   * Binds a frame to the text control.  This is performed when a frame
   * is created for the content node.
   * Be aware, this must be called with script blocker.
   */
  virtual nsresult BindToFrame(nsTextControlFrame* aFrame) = 0;

  /**
   * Unbinds a frame from the text control.  This is performed when a frame
   * belonging to a content node is destroyed.
   */
  MOZ_CAN_RUN_SCRIPT virtual void UnbindFromFrame(
      nsTextControlFrame* aFrame) = 0;

  /**
   * Creates an editor for the text control.  This should happen when
   * a frame has been created for the text control element, but the created
   * editor may outlive the frame itself.
   */
  MOZ_CAN_RUN_SCRIPT virtual nsresult CreateEditor() = 0;

  /**
   * Update preview value for the text control.
   */
  virtual void SetPreviewValue(const nsAString& aValue) = 0;

  /**
   * Get the current preview value for text control.
   */
  virtual void GetPreviewValue(nsAString& aValue) = 0;

  /**
   * Enable preview for text control.
   */
  virtual void EnablePreview() = 0;

  /**
   * Find out whether this control enables preview for form autofoll.
   */
  virtual bool IsPreviewEnabled() = 0;

  /**
   * Initialize the keyboard event listeners.
   */
  virtual void InitializeKeyboardEventListeners() = 0;

  enum class ValueChangeKind {
    Internal,
    Script,
    UserInteraction,
  };

  /**
   * Callback called whenever the value is changed.
   */
  virtual void OnValueChanged(ValueChangeKind) = 0;

  /**
   * Helpers for value manipulation from SetRangeText.
   */
  virtual void GetValueFromSetRangeText(nsAString& aValue) = 0;
  MOZ_CAN_RUN_SCRIPT virtual nsresult SetValueFromSetRangeText(
      const nsAString& aValue) = 0;

  static const int32_t DEFAULT_COLS = 20;
  static const int32_t DEFAULT_ROWS = 1;
  static const int32_t DEFAULT_ROWS_TEXTAREA = 2;
  static const int32_t DEFAULT_UNDO_CAP = 1000;

  // wrap can be one of these three values.
  typedef enum {
    eHTMLTextWrap_Off = 1,   // "off"
    eHTMLTextWrap_Hard = 2,  // "hard"
    eHTMLTextWrap_Soft = 3   // the default
  } nsHTMLTextWrap;

  static bool GetWrapPropertyEnum(nsIContent* aContent,
                                  nsHTMLTextWrap& aWrapProp);

  /**
   * Does the editor have a selection cache?
   *
   * Note that this function has the side effect of making the editor for input
   * elements be initialized eagerly.
   */
  virtual bool HasCachedSelection() = 0;

  static already_AddRefed<TextControlElement>
  GetTextControlElementFromEditingHost(nsIContent* aHost);

 protected:
  virtual ~TextControlElement() = default;
};

}  // namespace mozilla

#endif  // mozilla_TextControlElement_h
