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
#ifndef nsIHTMLDocument_h___
#define nsIHTMLDocument_h___

#include "nsISupports.h"
#include "nsCompatibility.h"

class nsIImageMap;
class nsString;
class nsIDOMNodeList;
class nsIDOMHTMLCollection;
class nsIDOMHTMLFormElement;
class nsIDOMHTMLMapElement;
class nsHTMLStyleSheet;
class nsIStyleSheet;
class nsICSSLoader;
class nsIContent;
class nsIDOMHTMLBodyElement;

/* 30dc35c0-75b5-4e96-a828-54470ce86726 */
#define NS_IHTMLDOCUMENT_IID \
{0x30dc35c0, 0x75b5, 0x4e96, {0xa8, 0x28, 0x54, 0x47, 0x0c, 0xe8, 0x67, 0x26}}


/**
 * HTML document extensions to nsIDocument.
 */
class nsIHTMLDocument : public nsISupports
{
public:
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IHTMLDOCUMENT_IID)

  virtual nsresult AddImageMap(nsIDOMHTMLMapElement* aMap) = 0;

  virtual nsIDOMHTMLMapElement *GetImageMap(const nsAString& aMapName) = 0;

  virtual void RemoveImageMap(nsIDOMHTMLMapElement* aMap) = 0;

  /**
   * Access compatibility mode for this document
   */
  virtual nsCompatibility GetCompatibilityMode() = 0;
  virtual void SetCompatibilityMode(nsCompatibility aMode) = 0;

  /*
   * Returns true if document.domain was set for this document
   */
  virtual PRBool WasDomainSet() = 0;

  virtual nsresult ResolveName(const nsAString& aName,
                               nsIDOMHTMLFormElement *aForm,
                               nsISupports **aResult) = 0;

  /*
   * This method returns null if we run out of memory. Callers should
   * check for null.
   */
  virtual already_AddRefed<nsIDOMNodeList> GetFormControlElements() = 0;
  
  /**
   * Called when form->SetDocument() is called so that document knows
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
};

#endif /* nsIHTMLDocument_h___ */
