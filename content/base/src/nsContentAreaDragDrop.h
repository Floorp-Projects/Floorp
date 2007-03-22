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
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Pinkerton <pinkerton@netscape.com>
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


#ifndef nsContentAreaDragDrop_h__
#define nsContentAreaDragDrop_h__


#include "nsCOMPtr.h"

#include "nsIDragDropHandler.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsITransferable.h"

class nsIDOMNode;
class nsISelection;
class nsITransferable;
class nsIImage;
class nsIPresShell;
class nsPresContext;
class nsIContent;
class nsIDocument;
class nsIURI;
class nsIFile;
class nsISimpleEnumerator;

// {1f34bc80-1bc7-11d6-a384-d705dd0746fc}
#define NS_CONTENTAREADRAGDROP_CID             \
{ 0x1f34bc80, 0x1bc7, 0x11d6, \
  { 0xa3, 0x84, 0xd7, 0x05, 0xdd, 0x07, 0x46, 0xfc } }

#define NS_CONTENTAREADRAGDROP_CONTRACTID "@mozilla.org:/content/content-area-dragdrop;1"


//
// class nsContentAreaDragDrop
//
// The class that listens to the chrome events handles anything
// related to drag and drop. Registers itself with the DOM with
// AddChromeListeners() and removes itself with
// RemoveChromeListeners().
//
class nsContentAreaDragDrop : public nsIDOMDragListener,
                              public nsIDragDropHandler,
                              public nsIFlavorDataProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAGDROPHANDLER
  NS_DECL_NSIFLAVORDATAPROVIDER
  
  nsContentAreaDragDrop();
  virtual ~nsContentAreaDragDrop();

  // nsIDOMDragListener
  NS_IMETHOD DragEnter(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragExit(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragDrop(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragGesture(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD HandleEvent(nsIDOMEvent *event);

private:

  // Add/remove the relevant listeners
  nsresult AddDragListener();
  nsresult RemoveDragListener();

  // utility routines
  static void NormalizeSelection(nsIDOMNode* inBaseNode,
                                 nsISelection* inSelection);
  static void GetEventDocument(nsIDOMEvent* inEvent,
                               nsIDOMDocument** outDocument);

  static nsresult SaveURIToFile(nsAString& inSourceURIString,
                                nsIFile* inDestFile);

  void ExtractURLFromData(const nsACString & inFlavor,
                          nsISupports* inDataWrapper, PRUint32 inDataLen,
                          nsAString & outURL);
  nsresult GetHookEnumeratorFromEvent(nsIDOMEvent* inEvent,
                                      nsISimpleEnumerator** outEnumerator);

  PRPackedBool mListenerInstalled;

  nsCOMPtr<nsIDOMEventReceiver> mEventReceiver;

  // weak ref, this is probably my owning webshell
  // FIXME: we set this and never null it out.  That's bad!  See bug 332187.
  nsIWebNavigation* mNavigator;

};



#endif /* nsContentAreaDragDrop_h__ */

