/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef nsTSDNotifier_h__
#define nsTSDNotifier_h__

#include "nsCOMPtr.h"
#include "nsIEditActionListener.h"

class nsTextServicesDocument;

class nsTSDNotifier : public nsIEditActionListener
{
private:

  nsTextServicesDocument *mDoc;

public:

  /** The default constructor.
   */
  nsTSDNotifier(nsTextServicesDocument *aDoc);

  /** The default destructor.
   */
  virtual ~nsTSDNotifier();

  /* Macro for AddRef(), Release(), and QueryInterface() */
  NS_DECL_ISUPPORTS

  /* nsIEditActionListener method implementations. */
  NS_IMETHOD WillInsertNode(nsIDOMNode *aNode,
                            nsIDOMNode *aParent,
                            PRInt32      aPosition);
  NS_IMETHOD DidInsertNode(nsIDOMNode *aNode,
                           nsIDOMNode *aParent,
                           PRInt32     aPosition,
                           nsresult    aResult);

  NS_IMETHOD WillDeleteNode(nsIDOMNode *aChild);
  NS_IMETHOD DidDeleteNode(nsIDOMNode *aChild, nsresult aResult);

  NS_IMETHOD WillSplitNode(nsIDOMNode * aExistingRightNode,
                           PRInt32      aOffset);
  NS_IMETHOD DidSplitNode(nsIDOMNode *aExistingRightNode,
                          PRInt32     aOffset,
                          nsIDOMNode *aNewLeftNode,
                          nsresult    aResult);

  NS_IMETHOD WillJoinNodes(nsIDOMNode  *aLeftNode,
                           nsIDOMNode  *aRightNode,
                           nsIDOMNode  *aParent);
  NS_IMETHOD DidJoinNodes(nsIDOMNode  *aLeftNode,
                          nsIDOMNode  *aRightNode,
                          nsIDOMNode  *aParent,
                          nsresult     aResult);
  // these listen methods are unused:                        
  NS_IMETHOD WillCreateNode(const nsAReadableString& aTag, nsIDOMNode *aParent, PRInt32 aPosition);
  NS_IMETHOD DidCreateNode(const nsAReadableString& aTag, nsIDOMNode *aNode, nsIDOMNode *aParent, PRInt32 aPosition, nsresult aResult);
  NS_IMETHOD WillInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAReadableString &aString);
  NS_IMETHOD DidInsertText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, const nsAReadableString &aString, nsresult aResult);
  NS_IMETHOD WillDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength);
  NS_IMETHOD DidDeleteText(nsIDOMCharacterData *aTextNode, PRInt32 aOffset, PRInt32 aLength, nsresult aResult);
  NS_IMETHOD WillDeleteSelection(nsISelection *aSelection);
  NS_IMETHOD DidDeleteSelection(nsISelection *aSelection);
};

#endif // nsTSDNotifier_h__
