/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is XMLterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 */

// mozSimpleContainer.h: declaration of mozSimpleContainer class
// implementing mozISimpleContainer,
// which provides a DocShell container for use in simple programs
// using the layout engine

#include "nscore.h"
#include "nsCOMPtr.h"

#include "mozISimpleContainer.h"

class mozSimpleContainer : public mozISimpleContainer {

public:

  mozSimpleContainer();
  virtual ~mozSimpleContainer();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDocumentLoaderObserver interface
  NS_DECL_NSIDOCUMENTLOADEROBSERVER \

  // other
  NS_IMETHOD Init(nsNativeWidget aNativeWidget,
                  PRInt32 width, PRInt32 height,
                  nsIPref* aPref);

  NS_IMETHOD Resize(PRInt32 aWidth, PRInt32 aHeight);

  NS_IMETHOD LoadURL(const char* aURL);

  NS_IMETHOD GetDocShell(nsIDocShell*& aDocShell);

protected:

  /** owning reference to doc shell created by us */
  nsCOMPtr<nsIDocShell> mDocShell;

};
