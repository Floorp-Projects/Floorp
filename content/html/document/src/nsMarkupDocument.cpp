/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */



#include "nsMarkupDocument.h"

#include "nsLayoutCID.h"
#include "nsIPresShell.h"
#include "nsIPresContext.h"
static NS_DEFINE_CID(kPresShellCID, NS_PRESSHELL_CID);


nsMarkupDocument::nsMarkupDocument() : nsDocument()
{
}

nsMarkupDocument::~nsMarkupDocument()
{
}

// XXX Temp hack: moved from nsDocument to fix circular linkage problem...

NS_IMETHODIMP
nsMarkupDocument::CreateShell(nsIPresContext* aContext,
                              nsIViewManager* aViewManager,
                              nsIStyleSet* aStyleSet,
                              nsIPresShell** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsresult rv;
  nsCOMPtr<nsIPresShell> shell(do_CreateInstance(kPresShellCID,&rv));
  if (NS_FAILED(rv)) {
    return rv;
  }

  rv = shell->Init(this, aContext, aViewManager, aStyleSet);
  if (NS_FAILED(rv)) {
    return rv;
  }

  // Note: we don't hold a ref to the shell (it holds a ref to us)
  mPresShells.AppendElement(shell);
  *aInstancePtrResult = shell.get();
  NS_ADDREF(*aInstancePtrResult);

  // tell the context the mode we want (always standard, which
  // should be correct for everything except HTML)
  // nsHTMLDocument overrides this method and sets it differently
  aContext->SetCompatibilityMode(eCompatibility_Standard);

  return NS_OK;
}

