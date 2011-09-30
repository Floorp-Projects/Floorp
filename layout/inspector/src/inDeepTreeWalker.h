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
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Joe Hewitt <hewitt@netscape.com> (original author)
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

#ifndef __inDeepTreeWalker_h___
#define __inDeepTreeWalker_h___

#include "inIDeepTreeWalker.h"

#include "nsCOMPtr.h"
#include "nsIDOMNode.h"
#include "nsTArray.h"

class inIDOMUtils;

////////////////////////////////////////////////////

struct DeepTreeStackItem
{
  nsCOMPtr<nsIDOMNode> node;
  nsCOMPtr<nsIDOMNodeList> kids;
  PRUint32 lastIndex; // Index one bigger than the index of whatever
                      // kid we're currently at in |kids|.
};

////////////////////////////////////////////////////

class inDeepTreeWalker : public inIDeepTreeWalker
{
public:
	NS_DECL_ISUPPORTS
	NS_DECL_NSIDOMTREEWALKER
	NS_DECL_INIDEEPTREEWALKER

  inDeepTreeWalker();
  virtual ~inDeepTreeWalker();

protected:
  void PushNode(nsIDOMNode* aNode);

  bool mShowAnonymousContent;
  bool mShowSubDocuments;
  nsCOMPtr<nsIDOMNode> mRoot;
  nsCOMPtr<nsIDOMNode> mCurrentNode;
  PRUint32 mWhatToShow;
  
  nsAutoTArray<DeepTreeStackItem, 8> mStack;
  nsCOMPtr<inIDOMUtils> mDOMUtils;
};

// {BFCB82C2-5611-4318-90D6-BAF4A7864252}
#define IN_DEEPTREEWALKER_CID \
{ 0xbfcb82c2, 0x5611, 0x4318, { 0x90, 0xd6, 0xba, 0xf4, 0xa7, 0x86, 0x42, 0x52 } }

#endif // __inDeepTreeWalker_h___
