/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://wwwt.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef nsTSDNotifier_h__
#define nsTSDNotifier_h__

#include "nsCOMPtr.h"

class nsTextServicesDocument;
class nsIEditActioinListener;

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
  NS_IMETHOD InsertNode(nsIDOMNode * aNode,
                        nsIDOMNode * aParent,
                        PRInt32      aPosition);
  NS_IMETHOD DeleteNode(nsIDOMNode * aChild);
  NS_IMETHOD SplitNode(nsIDOMNode * aExistingRightNode,
                       PRInt32      aOffset,
                       nsIDOMNode * aNewLeftNode);
  NS_IMETHOD JoinNodes(nsIDOMNode  *aLeftNode,
                       nsIDOMNode  *aRightNode,
                       nsIDOMNode  *aParent);
};

#endif // nsTSDNotifier_h__
