/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is XMLterm.
 *
 * The Initial Developer of the Original Code is
 * Ramalingam Saravanan.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// mozXMLTermUtils.h: XMLTerm utilities header

#ifndef _MOZXMLTERMUTILS_H

#define _MOZXMLTERMUTILS_H 1

#include "nscore.h"

#include "nsString.h"

#include "nsIDocShell.h"
#include "nsIScrollableView.h"
#include "nsIDOMWindowInternal.h"
#include "nsIDOMDocument.h"
#include "nsIScriptContext.h"

class mozXMLTermUtils
{
 public:

  /** Gets DOM window for doc shell
   * @param aDocShell doc shell
   * @param aDOMWindow returned DOM window (frame)
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    ConvertDocShellToDOMWindow(nsIDocShell* aDocShell,
                               nsIDOMWindowInternal** aDOMWindow);

  /** Gets doc shell for DOM window
   * @param aDOMWindow DOM window (frame)
   * @param aDocShell returned doc shell
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    ConvertDOMWindowToDocShell(nsIDOMWindowInternal* aDOMWindow,
                               nsIDocShell** aDocShell);

  /** Locates named inner DOM window (frame) inside outer DOM window
   * @param outerDOMWindow outer DOM window (frame)
   * @param innerFrameName name of inner frame to be returned
   * @param innerDOMWindow returned inner DOM window (frame)
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetInnerDOMWindow(nsIDOMWindowInternal* outerDOMWindow,
                      const nsString& innerFrameName,
                      nsIDOMWindowInternal** innerDOMWindow);

  /** Gets the scrollable view for presentation context
   * @param aPresContext presentation context
   * @param aScrollableView returned scrollable view
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetPresContextScrollableView(nsPresContext* aPresContext,
                                 nsIScrollableView** aScrollableView);

  /** Gets the device context for presentation context
   * @param aPresContext presentation context
   * @param aDeviceContext returned device context
   * @return NS_OK on success
   */
  static NS_EXPORT nsresult
    GetPresContextDeviceContext(nsPresContext* aPresContext,
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

  /** Returns the specified attribute value associated with a DOM node,
   * or a null string if the attribute is not defined, or if the DOM node
   * is not an element.
   * @param aDOMNode node whose attribute is to be determined
   * @param aAttName attribute to be determined
   * @param aAttValue output attribute value
   * @return NS_OK if no errors occurred
   */
  static NS_EXPORT nsresult
    GetNodeAttribute(nsIDOMNode* aDOMNode,
                     const char* aAttName,
                     nsString& aAttValue);

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
