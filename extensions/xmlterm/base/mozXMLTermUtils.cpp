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

// mozXMLTermUtils.cpp: XMLTerm utilities implementation

#include "nscore.h"
#include "nspr.h"
#include "prlog.h"

#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"

#include "nsIContentViewer.h"
#include "nsIDocumentViewer.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIDeviceContext.h"

#include "nsIPrincipal.h"
#include "nsIScriptContext.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDOMWindowCollection.h"
#include "nsIDOMElement.h"
#include "nsIDocument.h"

#include "mozXMLT.h"
#include "mozXMLTermUtils.h"

/////////////////////////////////////////////////////////////////////////

/** Gets DOM window for doc shell
 * @param aDocShell doc shell
 * @param aDOMWindow returned DOM window (frame)
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::ConvertDocShellToDOMWindow(nsIDocShell* aDocShell,
                                    nsIDOMWindowInternal** aDOMWindow)
{
  XMLT_LOG(mozXMLTermUtils::ConvertDocShellToDOMWindow,30,("\n"));

  if (!aDOMWindow)
    return NS_ERROR_FAILURE;

  *aDOMWindow = nsnull;

  nsCOMPtr<nsIScriptGlobalObject> scriptGlobalObject(do_GetInterface(aDocShell));

  nsCOMPtr<nsIDOMWindowInternal> domWindow(do_QueryInterface(scriptGlobalObject));
  if (!domWindow)
    return NS_ERROR_FAILURE;

  *aDOMWindow = domWindow.get();
  NS_ADDREF(*aDOMWindow);

  return NS_OK;
}


/** Gets doc shell for DOM window
 * @param aDOMWindow DOM window (frame)
 * @param aDocShell returned doc shell
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::ConvertDOMWindowToDocShell(nsIDOMWindowInternal* aDOMWindow,
                                             nsIDocShell** aDocShell)
{
  XMLT_LOG(mozXMLTermUtils::ConvertDOMWindowToDocShell,30,("\n"));

  nsCOMPtr<nsIScriptGlobalObject> globalObject = do_QueryInterface(aDOMWindow);
  if (!globalObject)
    return NS_ERROR_FAILURE;

  *aDocShell = globalObject->GetDocShell();
  if (!*aDocShell)
    return NS_ERROR_FAILURE;

  NS_ADDREF(*aDocShell);

  return NS_OK;
}


/** Locates named inner DOM window (frame) inside outer DOM window
 * @param outerDOMWindow outer DOM window (frame)
 * @param innerFrameName name of inner frame to be returned
 * @param innerDOMWindow returned inner DOM window (frame)
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::GetInnerDOMWindow(nsIDOMWindowInternal* outerDOMWindow,
                                    const nsString& innerFrameName,
                                    nsIDOMWindowInternal** innerDOMWindow)
{
  nsresult result;

  XMLT_LOG(mozXMLTermUtils::GetInnerDOMWindow,30,("\n"));

  nsCOMPtr<nsIDOMWindowCollection> innerDOMWindowList;
  result = outerDOMWindow->GetFrames(getter_AddRefs(innerDOMWindowList));
  if (NS_FAILED(result) || !innerDOMWindowList)
    return NS_ERROR_FAILURE;

  PRUint32 frameCount = 0;
  result = innerDOMWindowList->GetLength(&frameCount);
  XMLT_LOG(mozXMLTermUtils::GetInnerDOMWindow,31,("frameCount=%d\n",
                                                   frameCount));

  result = innerDOMWindowList->NamedItem(innerFrameName, (nsIDOMWindow **)innerDOMWindow);
  if (NS_FAILED(result) || !*innerDOMWindow)
    return NS_ERROR_FAILURE;

  return NS_OK;
}


/** Gets the scrollable view for presentation context
 * @param aPresContext presentation context
 * @param aScrollableView returned scrollable view
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::GetPresContextScrollableView(nsIPresContext* aPresContext,
                                         nsIScrollableView** aScrollableView)
{
  XMLT_LOG(mozXMLTermUtils::GetPresContextScrollableView,30,("\n"));

  if (!aScrollableView)
    return NS_ERROR_FAILURE;

  *aScrollableView = nsnull;

  nsIPresShell *presShell = aPresContext->GetPresShell();
  if (!presShell)
    return NS_ERROR_FAILURE;

  nsIViewManager* viewManager = presShell->GetViewManager();
  if (!viewManager)
    return NS_ERROR_FAILURE;

  return viewManager->GetRootScrollableView(aScrollableView);
}


/** Gets the device context for presentation context
 * @param aPresContext presentation context
 * @param aDeviceContext returned device context
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::GetPresContextDeviceContext(nsIPresContext* aPresContext,
                                         nsIDeviceContext** aDeviceContext)
{
  nsresult result;

  XMLT_LOG(mozXMLTermUtils::GetPresContextScrollableView,30,("\n"));

  if (!aDeviceContext)
    return NS_ERROR_FAILURE;

  *aDeviceContext = nsnull;

  nsIViewManager* viewManager = aPresContext->GetViewManager();
  if (!viewManager)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIDeviceContext> deviceContext;
  result = viewManager->GetDeviceContext(*getter_AddRefs(deviceContext));
  if (NS_FAILED(result) || !deviceContext)
    return NS_ERROR_FAILURE;

  *aDeviceContext = deviceContext.get();
  NS_ADDREF(*aDeviceContext);

  return NS_OK;
}


/** Gets the script context for document
 * @param aDOMDocument document providing context
 * @param aScriptContext returned script context
 * @return NS_OK on success
 */
NS_EXPORT nsresult
mozXMLTermUtils::GetScriptContext(nsIDOMDocument* aDOMDocument,
                                  nsIScriptContext** aScriptContext)
{
  XMLT_LOG(mozXMLTermUtils::GetScriptContext,20,("\n"));

  nsCOMPtr<nsIDocument> doc ( do_QueryInterface(aDOMDocument) );
  if (!doc)
    return NS_ERROR_FAILURE;

  nsIScriptGlobalObject *scriptGlobalObject = doc->GetScriptGlobalObject();

  if (!scriptGlobalObject)
    return NS_ERROR_FAILURE;

  *aScriptContext = scriptGlobalObject->GetContext();
  NS_IF_ADDREF(*aScriptContext);

  return NS_OK;
}


/** Executes script in specified document's context.
 * @param aDOMDocument document providing context for script execution
 * @param aScript string to be executed
 * @param aOutput output string produced by script execution
 * @return NS_OK if script was valid and got executed properly
 */
NS_EXPORT nsresult
mozXMLTermUtils::ExecuteScript(nsIDOMDocument* aDOMDocument,
                               const nsString& aScript,
                               nsString& aOutput)
{
  nsresult result;

  XMLT_LOG(mozXMLTermUtils::ExecuteScript,20,("\n"));

  // Get document principal
  nsCOMPtr<nsIDocument> doc = do_QueryInterface(aDOMDocument);
  if (!doc)
    return NS_ERROR_FAILURE;

  nsIPrincipal *docPrincipal = doc->GetPrincipal();
  if (!docPrincipal) 
    return NS_ERROR_FAILURE;

  // Get document script context
  nsCOMPtr<nsIScriptContext> scriptContext;
  result = GetScriptContext(aDOMDocument, getter_AddRefs(scriptContext));
  if (NS_FAILED(result) || !scriptContext)
    return NS_ERROR_FAILURE;

  // Execute script
  PRBool isUndefined = PR_FALSE;
  const char* URL = "";
  result = scriptContext->EvaluateString(aScript, (void *) nsnull,
                                         docPrincipal, URL, 0, nsnull,
                                         aOutput, &isUndefined);

  XMLT_LOG(mozXMLTermUtils::ExecuteScript,21,("result=0x%x,isUndefined=0x%x\n",
                                             result, isUndefined));

  return result;
}


/** Returns the specified attribute value associated with a DOM node,
 * or a null string if the attribute is not defined, or if the DOM node
 * is not an element.
 * @param aDOMNode node whose attribute is to be determined
 * @param aAttName attribute to be determined
 * @param aAttValue output attribute value
 * @return NS_OK if no errors occurred
 */
NS_EXPORT nsresult
mozXMLTermUtils::GetNodeAttribute(nsIDOMNode* aDOMNode,
                                  const char* aAttName,
                                  nsString& aAttValue)
{
  XMLT_LOG(mozXMLTermUtils::GetNodeAttribute,20,("aAttName=%s\n", aAttName));

  nsCOMPtr<nsIDOMElement> domElement = do_QueryInterface(aDOMNode);
  if (!domElement) {
    aAttValue.SetLength(0);
    return NS_OK;
  }

  nsAutoString attName;
  attName.AssignWithConversion(aAttName);
  return domElement->GetAttribute(attName, aAttValue);
}


/** Returns a timestamp string containing the local time, if at least
 * deltaSec seconds have elapsed since the last timestamp. Otherwise,
 * a null string is returned.
 * @param deltaSec minimum elapsed seconds since last timestamp (>=0)
 * @param lastTime in/out parameter containing time of last timestamp
 * @param aTimeStamp  returned timestamp string
 * @return NS_OK on success
 */
NS_IMETHODIMP mozXMLTermUtils::TimeStamp(PRInt32 deltaSec, PRTime& lastTime,
                                     nsString& aTimeStamp)
{
  static const PRInt32 DATE_LEN = 19;
  PRTime deltaTime ;
  char dateStr[DATE_LEN+1];
  PRTime curTime, difTime;

  curTime = PR_Now();
  LL_SUB(difTime, curTime, lastTime);

  LL_I2L(deltaTime, deltaSec*1000000);
  if (LL_CMP(difTime, <, deltaTime)) {
    // Not enough time has elapsed for a new time stamp
    aTimeStamp.SetLength(0);
    return NS_OK;
  }

  lastTime = curTime;

  // Current local time
  PRExplodedTime localTime;
  PR_ExplodeTime(curTime, PR_LocalTimeParameters, &localTime);

  PRInt32 nWritten = PR_snprintf(dateStr, DATE_LEN+1,
                     "%02d:%02d:%02d %02d/%02d/%04d",
                   localTime.tm_hour, localTime.tm_min, localTime.tm_sec,
                   localTime.tm_mday, localTime.tm_month+1, localTime.tm_year);

  if (nWritten != DATE_LEN)
    return NS_ERROR_FAILURE;

  XMLT_LOG(mozXMLTermUtils::LocalTime,99,("localTime=%s\n", dateStr));

  aTimeStamp.AssignWithConversion(dateStr);
  return NS_OK;
}


/** Returns a string containing a 11-digit random cookie based upon the
 *  current local time and the elapsed execution of the program.
 * @param aCookie  returned cookie string
 * @return NS_OK on success
 */
NS_IMETHODIMP mozXMLTermUtils::RandomCookie(nsString& aCookie)
{
  // Current local time
  PRExplodedTime localTime;
  PR_ExplodeTime(PR_Now(), PR_LocalTimeParameters, &localTime);

  PRInt32        randomNumberA = localTime.tm_sec*1000000+localTime.tm_usec;
  PRIntervalTime randomNumberB = PR_IntervalNow();

  XMLT_LOG(mozXMLTermUtils::RandomCookie,30,("ranA=0x%x, ranB=0x%x\n",
                                        randomNumberA, randomNumberB));
  PR_ASSERT(randomNumberA >= 0);
  PR_ASSERT(randomNumberB >= 0);

  static const char cookieDigits[17] = "0123456789abcdef";
  char cookie[12];
  int j;

  for (j=0; j<6; j++) {
    cookie[j] = cookieDigits[randomNumberA%16];
    randomNumberA = randomNumberA/16;
  }
  for (j=6; j<11; j++) {
    cookie[j] = cookieDigits[randomNumberB%16];
    randomNumberB = randomNumberB/16;
  }
  cookie[11] = '\0';

  aCookie.AssignWithConversion(cookie);

  return NS_OK;
}
