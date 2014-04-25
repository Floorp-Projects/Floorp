/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsEditRules_h__
#define nsEditRules_h__

#define NS_IEDITRULES_IID \
{ 0x3836386d, 0x806a, 0x488d, \
  { 0x8b, 0xab, 0xaf, 0x42, 0xbb, 0x4c, 0x90, 0x66 } }

#include "nsEditor.h"

class nsPlaintextEditor;
class nsISelection;

/***************************************************************************
 * base for an object to encapsulate any additional info needed to be passed
 * to rules system by the editor
 */
class nsRulesInfo
{
  public:
  
  nsRulesInfo(EditAction aAction) : action(aAction) {}
  virtual ~nsRulesInfo() {}
  
  EditAction action;
};

/***************************************************************************
 * Interface of editing rules.
 *  
 */
class nsIEditRules : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_IEDITRULES_IID)
  
//Interfaces for addref and release and queryinterface
//NOTE: Use   NS_DECL_ISUPPORTS_INHERITED in any class inherited from nsIEditRules

  NS_IMETHOD Init(nsPlaintextEditor *aEditor)=0;
  NS_IMETHOD SetInitialValue(const nsAString& aValue) = 0;
  NS_IMETHOD DetachEditor()=0;
  NS_IMETHOD BeforeEdit(EditAction action,
                        nsIEditor::EDirection aDirection) = 0;
  NS_IMETHOD AfterEdit(EditAction action,
                       nsIEditor::EDirection aDirection) = 0;
  NS_IMETHOD WillDoAction(mozilla::dom::Selection* aSelection, nsRulesInfo* aInfo,
                          bool* aCancel, bool* aHandled) = 0;
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult)=0;
  NS_IMETHOD DocumentIsEmpty(bool *aDocumentIsEmpty)=0;
  NS_IMETHOD DocumentModified()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEditRules, NS_IEDITRULES_IID)

#endif //nsEditRules_h__

