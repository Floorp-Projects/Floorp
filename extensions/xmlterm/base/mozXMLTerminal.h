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

// mozXMLTerminal.h: declaration of mozXMLTerminal
// which implements the mozIXMLTerminal interface
// to manage all XMLterm operations.

#include "nscore.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsWeakReference.h"
#include "nsWeakPtr.h"
#include "nsString.h"
#include "nsIGenericFactory.h"
#include "nsIWebProgressListener.h"
#include "nsIObserver.h"
#include "mozXMLT.h"

#include "mozILineTerm.h"
#include "mozIXMLTerminal.h"
#include "mozXMLTermSession.h"
#include "mozXMLTermListeners.h"
#include "mozIXMLTermStream.h"


class mozXMLTerminal : public mozIXMLTerminal,
                       public nsIWebProgressListener,
                       public nsIObserver,
                       public nsSupportsWeakReference
{
  public:

  mozXMLTerminal();
  virtual ~mozXMLTerminal();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // nsIWebProgressListener interface
  NS_DECL_NSIWEBPROGRESSLISTENER

  // mozIXMLTerminal interface
  NS_DECL_MOZIXMLTERMINAL

  // nsIObserver interface
  NS_IMETHOD Observe(nsISupports *aSubject, const char *aTopic,
                     const PRUnichar *someData);

  // Others

  /** Activates XMLterm and instantiates LineTerm;
   * called at the the end of Init page loading.
   */
  NS_IMETHOD Activate(void);

  protected:

  /** object initialization flag */
  PRBool             mInitialized;

  /** cookie string used for authentication (stored in document.cookie) */
  nsString           mCookie;

  nsString           mCommand;
  nsString           mPromptExpr;

  /** initial input string to be sent to LineTerm */
  nsString           mInitInput;

  /** non-owning reference to containing XMLTermShell object */
  mozIXMLTermShell*  mXMLTermShell;

  /** weak reference to containing doc shell */
  nsWeakPtr          mDocShell;

  /** weak reference to presentation shell for XMLterm */
  nsWeakPtr          mPresShell;

  /** weak reference to DOM document containing XMLterm */
  nsWeakPtr          mDOMDocument;

  /** XMLTermSession object created by us (not reference counted) */
  mozXMLTermSession* mXMLTermSession;

  /** owning reference to LineTermAux object created by us */
  nsCOMPtr<mozILineTermAux> mLineTermAux;

  /** terminal needs resizing flag */
  PRBool                    mNeedsResizing;

  /** owning referencing to key listener object created by us */
  nsCOMPtr<nsIDOMEventListener> mKeyListener;

  /** owning referencing to text listener object created by us */
  nsCOMPtr<nsIDOMEventListener> mTextListener;

  /** owning referencing to mouse listener object created by us */
  nsCOMPtr<nsIDOMEventListener> mMouseListener;

  /** owning referencing to drag listener object created by us */
  nsCOMPtr<nsIDOMEventListener> mDragListener;

};
