/* -*- Mode: C++; tab-width: 3; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is the Mozilla browser.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications, Inc.  Portions created by Netscape are
 * Copyright (C) 1999, Mozilla.  All Rights Reserved.
 * 
 * Contributor(s):
 *   David W. Hyatt <hyatt@netscape.com> (Original Author)
 */

#ifndef nsOutlinerSelection_h__
#define nsOutlinerSelection_h__

#include "nsIOutlinerSelection.h"

class nsIOutlinerBoxObject;

class nsOutlinerSelection : public nsIOutlinerSelection
{
public:
  nsOutlinerSelection(nsIOutlinerBoxObject* aOutliner);
  virtual ~nsOutlinerSelection();
   
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOUTLINERSELECTION

protected:
  // Members
  nsIOutlinerBoxObject* mOutliner; // [Weak]. The outliner will hold on to us through the view and let go when it dies.
};

extern nsresult
NS_NewOutlinerSelection(nsIOutlinerBoxObject* aOutliner,
                        nsIOutlinerSelection** aResult);

#endif
