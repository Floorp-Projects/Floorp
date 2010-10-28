/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */

/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * John B. Keiser
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsITextControlElement_h___
#define nsITextControlElement_h___

#include "nsISupports.h"
class nsIContent;
class nsAString;
class nsIEditor;
class nsISelectionController;
class nsFrameSelection;
class nsTextControlFrame;

// IID for the nsITextControl interface
#define NS_ITEXTCONTROLELEMENT_IID    \
{ 0x66545dde, 0x3f4a, 0x49fd,    \
  { 0x82, 0x73, 0x69, 0x7e, 0xab, 0x54, 0x06, 0x0a } }

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
  NS_IMETHOD SetValueChanged(PRBool changed) = 0;

  /**
   * Find out whether this is a single line text control.  (text or password)
   * @return whether this is a single line text control
   */
  NS_IMETHOD_(PRBool) IsSingleLineTextControl() const = 0;

  /**
   * Find out whether this control is a textarea.
   * @return whether this is a textarea text control
   */
  NS_IMETHOD_(PRBool) IsTextArea() const = 0;

  /**
   * Find out whether this control edits plain text.  (Currently always true.)
   * @return whether this is a plain text control
   */
  NS_IMETHOD_(PRBool) IsPlainTextControl() const = 0;

  /**
   * Find out whether this is a password control (input type=password)
   * @return whether this is a password ontrol
   */
  NS_IMETHOD_(PRBool) IsPasswordTextControl() const = 0;

  /**
   * Get the cols attribute (if textarea) or a default
   * @return the number of columns to use
   */
  NS_IMETHOD_(PRInt32) GetCols() = 0;

  /**
   * Get the column index to wrap at, or -1 if we shouldn't wrap
   */
  NS_IMETHOD_(PRInt32) GetWrapCols() = 0;

  /**
   * Get the rows attribute (if textarea) or a default
   * @return the number of rows to use
   */
  NS_IMETHOD_(PRInt32) GetRows() = 0;

  /**
   * Get the default value of the text control
   */
  NS_IMETHOD_(void) GetDefaultValueFromContent(nsAString& aValue) = 0;

  /**
   * Return true if the value of the control has been changed.
   */
  NS_IMETHOD_(PRBool) ValueChanged() const = 0;

  /**
   * Get the current value of the text editor.
   *
   * @param aValue the buffer to retrieve the value in
   * @param aIgnoreWrap whether to ignore the text wrapping behavior specified
   * for the element.
   */
  NS_IMETHOD_(void) GetTextEditorValue(nsAString& aValue, PRBool aIgnoreWrap) const = 0;

  /**
   * Set the current value of the text editor.
   *
   * @param aValue the new value for the text control.
   * @param aUserInput whether this value is coming from user input.
   */
  NS_IMETHOD_(void) SetTextEditorValue(const nsAString& aValue, PRBool aUserInput) = 0;

  /**
   * Get the editor object associated with the text editor.
   * The return value is null if the control does not support an editor
   * (for example, if it is a checkbox.)
   */
  NS_IMETHOD_(nsIEditor*) GetTextEditor() = 0;

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
   * Get the anonymous root node for the text control.
   */
  NS_IMETHOD_(nsIContent*) GetRootEditorNode() = 0;

  /**
   * Create the placeholder anonymous node for the text control and returns it.
   */
  NS_IMETHOD_(nsIContent*) CreatePlaceholderNode() = 0;

  /**
   * Get the placeholder anonymous node for the text control.
   */
  NS_IMETHOD_(nsIContent*) GetPlaceholderNode() = 0;

  /**
   * Initialize the keyboard event listeners.
   */
  NS_IMETHOD_(void) InitializeKeyboardEventListeners() = 0;

  /**
   * Notify the text control that the placeholder text needs to be updated.
   */
  NS_IMETHOD_(void) UpdatePlaceholderText(PRBool aNotify) = 0;

  /**
   * Show/hide the placeholder for the control.
   */
  NS_IMETHOD_(void) SetPlaceholderClass(PRBool aVisible, PRBool aNotify) = 0;

  /**
   * Callback called whenever the value is changed.
   */
  NS_IMETHOD_(void) OnValueChanged(PRBool aNotify) = 0;

  static const PRInt32 DEFAULT_COLS = 20;
  static const PRInt32 DEFAULT_ROWS = 1;
  static const PRInt32 DEFAULT_ROWS_TEXTAREA = 2;
  static const PRInt32 DEFAULT_UNDO_CAP = 1000;

  // wrap can be one of these three values.  
  typedef enum {
    eHTMLTextWrap_Off     = 1,    // "off"
    eHTMLTextWrap_Hard    = 2,    // "hard"
    eHTMLTextWrap_Soft    = 3     // the default
  } nsHTMLTextWrap;

  static PRBool
  GetWrapPropertyEnum(nsIContent* aContent, nsHTMLTextWrap& aWrapProp);
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsITextControlElement,
                              NS_ITEXTCONTROLELEMENT_IID)

#endif // nsITextControlElement_h___

