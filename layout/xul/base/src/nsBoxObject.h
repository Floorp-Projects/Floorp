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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIBoxObject.h"
#include "nsPIBoxObject.h"

class nsIBoxLayoutManager;
class nsIBoxPaintManager;
class nsIFrame;
struct nsRect;

class nsBoxObject : public nsIBoxObject, public nsPIBoxObject
{
  NS_DECL_ISUPPORTS
  NS_DECL_NSIBOXOBJECT
  NS_DECL_NSISECURITYCHECKEDCOMPONENT

public:
  nsBoxObject();
  virtual ~nsBoxObject();

  // nsPIBoxObject
  NS_IMETHOD Init(nsIContent* aContent, nsIPresShell* aPresShell);
  NS_IMETHOD SetDocument(nsIDocument* aDocument);

  virtual nsIFrame* GetFrame();
  nsresult GetOffsetRect(nsRect& aRect);

// MEMBER VARIABLES
protected: 
  nsCOMPtr<nsIBoxLayoutManager> mLayoutManager; // [OWNER]
  nsCOMPtr<nsIBoxPaintManager> mPaintManager; // [OWNER]
  nsIContent* mContent; // [WEAK]
  nsIPresShell* mPresShell; // [WEAK]
};
