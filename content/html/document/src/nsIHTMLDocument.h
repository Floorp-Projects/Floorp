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
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#ifndef nsIHTMLDocument_h
#define nsIHTMLDocument_h

#include "nsISupports.h"
#include "nsCompatibility.h"

class nsIDOMHTMLFormElement;
class nsIContent;
class nsIScriptElement;
class nsIEditor;
class nsContentList;
class nsWrapperCache;

namespace mozilla {
namespace dom {
class Element;
} // namespace dom
} // namespace mozilla

#define NS_IHTMLDOCUMENT_IID \
{ 0xe43a4bfd, 0xff5a, 0x40b0, \
  { 0x8c, 0x31, 0x24, 0xac, 0xe8, 0x15, 0xda, 0xf2 } }


/**
 * HTML document extensions to nsIDocument.
 */
class nsIHTMLDocument : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTMLDOCUMENT_IID)

  virtual mozilla::dom::Element* GetImageMap(const nsAString& aMapName) = 0;

  /**
   * Set compatibility mode for this document
   */
  virtual void SetCompatibilityMode(nsCompatibility aMode) = 0;

  virtual nsresult ResolveName(const nsAString& aName,
                               nsIDOMHTMLFormElement *aForm,
                               nsISupports **aResult,
                               nsWrapperCache **aCache) = 0;

  /**
   * Called from the script loader to notify this document that a new
   * script is being loaded.
   */
  virtual void ScriptLoading(nsIScriptElement *aScript) = 0;

  /**
   * Called from the script loader to notify this document that a script
   * just finished executing.
   */
  virtual void ScriptExecuted(nsIScriptElement *aScript) = 0;

  /**
   * Called when form->BindToTree() is called so that document knows
   * immediately when a form is added
   */
  virtual void AddedForm() = 0;
  /**
   * Called when form->SetDocument() is called so that document knows
   * immediately when a form is removed
   */
  virtual void RemovedForm() = 0;
  /**
   * Called to get a better count of forms than document.forms can provide
   * without calling FlushPendingNotifications (bug 138892).
   */
  // XXXbz is this still needed now that we can flush just content,
  // not the rest?
  virtual PRInt32 GetNumFormsSynchronous() = 0;
  
  virtual PRBool IsWriting() = 0;

  virtual PRBool GetIsFrameset() = 0;
  virtual void SetIsFrameset(PRBool aFrameset) = 0;

  /**
   * Get the list of form elements in the document.
   */
  virtual nsContentList* GetForms() = 0;

  /**
   * Get the list of form controls in the document (all elements in
   * the document that are of type nsIContent::eHTML_FORM_CONTROL).
   */
  virtual nsContentList* GetFormControls() = 0;

  /**
   * Should be called when an element's editable changes as a result of
   * changing its contentEditable attribute/property.
   *
   * @param aElement the element for which the contentEditable
   *                 attribute/property was changed
   * @param aChange +1 if the contentEditable attribute/property was changed to
   *                true, -1 if it was changed to false
   */
  virtual nsresult ChangeContentEditableCount(nsIContent *aElement,
                                              PRInt32 aChange) = 0;

  enum EditingState {
    eTearingDown = -2,
    eSettingUp = -1,
    eOff = 0,
    eDesignMode,
    eContentEditable
  };

  /**
   * Returns whether the document is editable.
   */
  PRBool IsEditingOn()
  {
    return GetEditingState() == eDesignMode ||
           GetEditingState() == eContentEditable;
  }

  /**
   * Returns the editing state of the document (not editable, contentEditable or
   * designMode).
   */
  virtual EditingState GetEditingState() = 0;

  /**
   * Set the editing state of the document. Don't use this if you want
   * to enable/disable editing, call EditingStateChanged() or
   * SetDesignMode().
   */
  virtual nsresult SetEditingState(EditingState aState) = 0;

  /**
   * Disables getting and setting cookies
   */
  virtual void DisableCookieAccess() = 0;

  /**
   * Get the first <body> child of the root <html>, but don't do
   * anything <frameset>-related (like nsIDOMHTMLDocument::GetBody).
   */
  virtual nsIContent* GetBodyContentExternal() = 0;

  /**
   * Called when this nsIHTMLDocument's editor is destroyed.
   */
  virtual void TearingDownEditor(nsIEditor *aEditor) = 0;

  virtual void SetIsXHTML(PRBool aXHTML) = 0;

  virtual void SetDocWriteDisabled(PRBool aDisabled) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLDocument, NS_IHTMLDOCUMENT_IID)

#endif /* nsIHTMLDocument_h */
