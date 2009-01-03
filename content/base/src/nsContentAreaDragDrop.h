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
#include "nsIDOMEventTarget.h"
#include "nsIDOMEventListener.h"
#include "nsITransferable.h"

class nsIDOMNode;
class nsIDOMWindow;
class nsIDOMDocument;
class nsIDOMDragEvent;
class nsISelection;
class nsITransferable;
class nsIImage;
class nsIPresShell;
class nsPresContext;
class nsIContent;
class nsIURI;
class nsIFile;
class nsISimpleEnumerator;
class nsDOMDataTransfer;

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
class nsContentAreaDragDrop : public nsIDragDropHandler,
                              public nsIDOMEventListener
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIDRAGDROPHANDLER
  
  nsContentAreaDragDrop();
  virtual ~nsContentAreaDragDrop();

  NS_IMETHOD HandleEvent(nsIDOMEvent *event);

  /**
   * Determine what data in the content area, if any, is being dragged.
   *
   * aWindow - the window containing the target node
   * aTarget - the mousedown event target that started the drag
   * aSelectionTargetNode - the node where the drag event should be fired
   * aIsAltKeyPressed - true if the Alt key is pressed. In some cases, this
   *                    will prevent the drag from occuring. For example,
   *                    holding down Alt over a link should select the text,
   *                    not drag the link.
   * aDataTransfer - the dataTransfer for the drag event.
   * aCanDrag - [out] set to true if the drag may proceed, false to stop the
   *            drag entirely
   * aDragSelection - [out] set to true to indicate that a selection is being
   *                  dragged, rather than a specific node
   * aDragNode - [out] the link, image or area being dragged, or null if the
   *             drag occured on another element.
   */
  static nsresult GetDragData(nsIDOMWindow* aWindow,
                              nsIContent* aTarget,
                              nsIContent* aSelectionTargetNode,
                              PRBool aIsAltKeyPressed,
                              nsDOMDataTransfer* aDataTransfer,
                              PRBool* aCanDrag,
                              PRBool* aDragSelection,
                              nsIContent** aDragNode);

private:

  // Add/remove the relevant listeners
  nsresult AddDragListener();
  nsresult RemoveDragListener();

  nsresult DragOver(nsIDOMDragEvent* aDragEvent);
  nsresult Drop(nsIDOMDragEvent* aDragEvent);

  // utility routines
  static void NormalizeSelection(nsIDOMNode* inBaseNode,
                                 nsISelection* inSelection);
  static void GetEventDocument(nsIDOMEvent* inEvent,
                               nsIDOMDocument** outDocument);

  static void ExtractURLFromData(const nsACString & inFlavor,
                                 nsISupports* inDataWrapper, PRUint32 inDataLen,
                                 nsAString & outURL);

  nsCOMPtr<nsIDOMEventTarget> mEventTarget;

  // weak ref, this is probably my owning webshell
  // FIXME: we set this and never null it out.  That's bad!  See bug 332187.
  nsIWebNavigation* mNavigator;

};

// this is used to save images to disk lazily when the image data is asked for
// during the drop instead of when it is added to the drag data transfer. This
// ensures that the image data is only created when an image drop is allowed.
class nsContentAreaDragDropDataProvider : public nsIFlavorDataProvider
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIFLAVORDATAPROVIDER

  virtual ~nsContentAreaDragDropDataProvider() {}

  nsresult SaveURIToFile(nsAString& inSourceURIString,
                         nsIFile* inDestFile);
};


#endif /* nsContentAreaDragDrop_h__ */

