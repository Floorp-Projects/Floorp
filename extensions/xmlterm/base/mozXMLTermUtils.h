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

// mozXMLTermUtils.h: XMLTerm utilities header

#ifndef _MOZXMLTERMUTILS_H

#define _MOZXMLTERMUTILS_H 1

#include "nscore.h"

#include "nsString.h"

#include "nsIWebShell.h"
#include "nsIDOMWindow.h"
#include "nsIDOMDocument.h"

class mozXMLTermUtils
{
 public:
  /** Gets presentation context for web shell
   * @param aWebShell web shell
   * @param aPresContext returned presentation context
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetWebShellPresContext(nsIWebShell* aWebShell,
                           nsIPresContext** aPresContext);

  /** Gets DOM document for web shell
   * @param aWebShell web shell
   * @param aDOMDocument returned DOM document
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetWebShellDOMDocument(nsIWebShell* aWebShell,
                           nsIDOMDocument** aDOMDocument);

  /** Gets DOM window for web shell
   * @param aWebShell web shell
   * @param aDOMWindow returned DOM window (frame)
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    ConvertWebShellToDOMWindow(nsIWebShell* aWebShell,
                               nsIDOMWindow** aDOMWindow);

  /** Gets web shell for DOM window
   * @param aDOMWindow DOM window (frame)
   * @param aWebShell returned web shell
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    ConvertDOMWindowToWebShell(nsIDOMWindow* aDOMWindow,
                               nsIWebShell** aWebShell);

  /** Locates named inner DOM window (frame) inside outer DOM window
   * @param outerDOMWindow outer DOM window (frame)
   * @param innerFrameName name of inner frame to be returned
   * @param innerDOMWindow returned inner DOM window (frame)
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetInnerDOMWindow(nsIDOMWindow* outerDOMWindow,
                      const nsString& innerFrameName,
                      nsIDOMWindow** innerDOMWindow);

  /** Gets the scrollable view for presentation context
   * @param aPresContext presentation context
   * @param aScrollableView returned scrollable view
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetPresContextScrollableView(nsIPresContext* aPresContext,
                                 nsIScrollableView** aScrollableView);

  /** Gets the device context for presentation context
   * @param aPresContext presentation context
   * @param aDeviceContext returned device context
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetPresContextDeviceContext(nsIPresContext* aPresContext,
                                nsIDeviceContext** aDeviceContext);

  /** Gets the script context for document
   * @param aDOMDocument document providing context
   * @param aScriptContext returned script context
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetScriptContext(nsIDOMDocument* aDOMDocument,
                     nsIScriptContext** aScriptContext);

  /** Executes script in specified document's context.
   * @param aDOMDocument document providing context for script execution
   * @param aScript string to be executed
   * @param aOutput output string produced by script execution
   * @return NS_OK if script was valid and got executed properly
   */
  static NS_EXPORT nsresult
    ExecuteScript(nsIDOMDocument* aDOMDocument,
                  const nsString& aScript,
                  nsString& aOutput);

  /** Returns a timestamp string containing the local time, if at least
   * deltaSec seconds have elapsed since the last timestamp. Otherwise,
   * a null string is returned.
   * @param deltaSec minimum elapsed seconds since last timestamp (>=0)
   * @param lastTime in/out parameter containing time of last timestamp
   * @param aTimeStamp  returned timestamp string
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    TimeStamp(PRInt32 deltaSec, PRTime& lastTime,
              nsString& aTimeStamp);

  /** Returns a string containing a 11-digit random cookie based upon the
   *  current local time and the elapsed execution of the program.
   * @param aCookie  returned cookie string
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    RandomCookie(nsString& aCookie);

};

#endif  /* _MOZXMLTERMUTILS_H */
