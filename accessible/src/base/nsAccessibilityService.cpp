/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Original Author: David W. Hyatt (hyatt@netscape.com)
 *
 * Contributor(s): 
 */

#include "nsIAccessibilityService.h"
#include "nsAccessibilityService.h"
#include "nsAccessible.h"
#include "nsCOMPtr.h"
#include "nsIDocument.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
#include "nsIContent.h"
#include "nsIFrame.h"

nsAccessibilityService::nsAccessibilityService()
{
	NS_INIT_REFCNT();
}

nsAccessibilityService::~nsAccessibilityService()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsAccessibilityService, nsIAccessibilityService);

////////////////////////////////////////////////////////////////////////////////
// nsIAccessibilityService methods:

NS_IMETHODIMP 
nsAccessibilityService::GetAccessibleFor(nsISupports* frame, nsISupports* context, nsIAccessible **_retval)
{
  void* ptr = nsnull;
  frame->QueryInterface(nsIFrame::GetIID(), &ptr);
  nsIFrame* f = (nsIFrame*)ptr;

  nsCOMPtr<nsIPresContext> c = do_QueryInterface(context);

  NS_ASSERTION(f,"Error non frame passed to accessible factory!!!");
  NS_ASSERTION(c,"Error non prescontext passed to accessible factory!!!");

  *_retval = new nsAccessible(f,c);
  return NS_OK;
}

//////////////////////////////////////////////////////////////////////

nsresult
NS_NewAccessibilityService(nsIAccessibilityService** aResult)
{
    NS_PRECONDITION(aResult != nsnull, "null ptr");
    if (! aResult)
        return NS_ERROR_NULL_POINTER;

    nsAccessibilityService* a = new nsAccessibilityService();
    if (a == nsnull)
        return NS_ERROR_OUT_OF_MEMORY;
    NS_ADDREF(a);
    *aResult = a;
    return NS_OK;
}

