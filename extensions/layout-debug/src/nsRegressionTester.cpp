/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * 
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

#include "nsISupports.h"

#include "nsXPIDLString.h"
#include "nsReadableUtils.h"
#include "nsIFileSpec.h"
#include "nsIWindowWatcher.h"
#include "nsVoidArray.h"
#include "prmem.h"
#include "nsDebugObject.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDOMWindowInternal.h"
#include "nsIPresShell.h"
#include "nsIDOMDocument.h"
#include "nsIURI.h"
#include "nsIDOMHTMLDocument.h"
#include "nsISimpleEnumerator.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocShell.h"
#include "nsIFrameDebug.h"
#include "nsIFrame.h"
#include "nsStyleStruct.h"
 
NS_IMPL_ISUPPORTS1(nsDebugObject, nsIDebugObject)


/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 6/21/00 dwc
 */
nsDebugObject::nsDebugObject() 
{
  NS_INIT_ISUPPORTS();
}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 6/21/00 dwc
 */
nsDebugObject::~nsDebugObject() 
{

}

/** ---------------------------------------------------
 *  See documentation in nsDebugObject.h
 *	@update 6/21/00 dwc
 */
NS_IMETHODIMP
nsDebugObject::DumpContent(nsISupports *aWindow, const PRUnichar *aFilePath, const PRUnichar *aFileName) 
{
nsresult            found;

  nsCOMPtr<nsIDOMWindowInternal> theInternWindow(do_QueryInterface(aWindow));
  if (theInternWindow) {

    nsCOMPtr<nsIPresShell> presShell;
    if (theInternWindow != nsnull) {
      nsIFrameDebug*  fdbg;
      nsIFrame*       root;
      nsIPresContext  *thePC;

      nsCOMPtr<nsIScriptGlobalObject> scriptObj(do_QueryInterface(theInternWindow));
      nsCOMPtr<nsIDocShell> docShell;
      scriptObj->GetDocShell(getter_AddRefs(docShell));
      docShell->GetPresShell(getter_AddRefs(presShell));
      presShell->GetRootFrame(&root);
      if (NS_SUCCEEDED(CallQueryInterface(root, &fdbg))) {
        FILE* fp = fopen("s:/testdump.txt", "wt");
        presShell->GetPresContext(&thePC);
        fdbg->DumpRegressionData(thePC, fp, 0, PR_TRUE);
        fclose(fp);
      }
    }
  }

  return NS_OK;
}

