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

#ifndef nsEditRules_h__
#define nsEditRules_h__

// FB45AC36-E8F1-44ae-8FB7-466E1BE119B0
#define NS_IEDITRULES_IID \
{ 0x2cc50d11, 0x9909, 0x433f, \
  { 0xb6, 0xfb, 0x4c, 0xf2, 0x56, 0xe5, 0xe5, 0x71 } }

class nsPlaintextEditor;
class nsISelection;

/***************************************************************************
 * base for an object to encapsulate any additional info needed to be passed
 * to rules system by the editor
 */
class nsRulesInfo
{
  public:
  
  nsRulesInfo(int aAction) : action(aAction) {}
  virtual ~nsRulesInfo() {}
  
  int action;
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
  NS_IMETHOD DetachEditor()=0;
  NS_IMETHOD BeforeEdit(PRInt32 action, nsIEditor::EDirection aDirection)=0;
  NS_IMETHOD AfterEdit(PRInt32 action, nsIEditor::EDirection aDirection)=0;
  NS_IMETHOD WillDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, bool *aCancel, bool *aHandled)=0;
  NS_IMETHOD DidDoAction(nsISelection *aSelection, nsRulesInfo *aInfo, nsresult aResult)=0;
  NS_IMETHOD DocumentIsEmpty(bool *aDocumentIsEmpty)=0;
  NS_IMETHOD DocumentModified()=0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIEditRules, NS_IEDITRULES_IID)

#endif //nsEditRules_h__

