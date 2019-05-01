/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsIHTMLDocument_h
#define nsIHTMLDocument_h

#include "nsISupports.h"
#include "nsCompatibility.h"

class nsIContent;
class nsContentList;

#define NS_IHTMLDOCUMENT_IID                         \
  {                                                  \
    0xcf814492, 0x303c, 0x4718, {                    \
      0x9a, 0x3e, 0x39, 0xbc, 0xd5, 0x2c, 0x10, 0xdb \
    }                                                \
  }

/**
 * HTML document extensions to Document.
 */
class nsIHTMLDocument : public nsISupports {
 public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IHTMLDOCUMENT_IID)

  /**
   * Set compatibility mode for this document
   */
  virtual void SetCompatibilityMode(nsCompatibility aMode) = 0;

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
  virtual int32_t GetNumFormsSynchronous() = 0;

  virtual bool IsWriting() = 0;

  /**
   * Should be called when an element's editable changes as a result of
   * changing its contentEditable attribute/property.
   *
   * @param aElement the element for which the contentEditable
   *                 attribute/property was changed
   * @param aChange +1 if the contentEditable attribute/property was changed to
   *                true, -1 if it was changed to false
   */
  virtual nsresult ChangeContentEditableCount(nsIContent* aElement,
                                              int32_t aChange) = 0;

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
  bool IsEditingOn() {
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
   * Called when this nsIHTMLDocument's editor is destroyed.
   */
  virtual void TearingDownEditor() = 0;

  virtual void SetIsXHTML(bool aXHTML) = 0;

  virtual void SetDocWriteDisabled(bool aDisabled) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIHTMLDocument, NS_IHTMLDOCUMENT_IID)

#endif /* nsIHTMLDocument_h */
