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

// mozLineTerm.h: Declaration of mozLineTerm class
// which implements the LineTerm and LineTermAux interfaces.
// and provides an XPCOM/XPCONNECT wrapper to the LINETERM module

#include "nspr.h"
#include "nscore.h"
#include "nsString.h"
#include "nsIGenericFactory.h"

#include "nsIServiceManager.h"

#include "nsIDocument.h"
#include "nsIDOMHTMLDocument.h"

#include "mozILineTerm.h"
#include "lineterm.h"

#include <glibconfig.h>
#include <gdk/gdk.h>

#define MAXCOL 4096            // Maximum columns in line buffer

class mozLineTerm : public mozILineTermAux
{
public:
  mozLineTerm();
  virtual ~mozLineTerm();

  // nsISupports interface
  NS_DECL_ISUPPORTS
  NS_DECL_MOZILINETERM
  NS_DECL_MOZILINETERMAUX

  // Define a Create method to be used with a factory:
  static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  // others

  /** GTK event callback function
   */
  static void Callback(gpointer data,
                       gint source,
                       GdkInputCondition condition);

protected:
  /** Checks if Mozilla preference settings are secure
   * @param _retval (output) PR_TRUE if settings are secure
   */
  NS_IMETHOD ArePrefsSecure(PRBool *_retval);

  /** Checks if document principal is secure and returns principal string
   * @param domDOC DOM document object
   * @param aPrincipalStr (output) document principal string
   */
  NS_IMETHOD GetSecurePrincipal(nsIDOMDocument *domDoc,
                                char** aPrincipalStr);

  /** lineterm descriptor index returned by lterm_new (>= 0) */
  int mLTerm;

  /** current cursor row position */
  int mCursorRow;

  /** current cursor column position */
  int mCursorColumn;

  /** flag controlling whether is LineTerm is suspended */
  PRBool mSuspended;

  /** flag controlling input echo in LineTerm */
  PRBool mEchoFlag;

  /** non-owning reference to Observer to be notified when data is available
   * for reading from LineTerm
   */
  nsIObserver* mObserver;

  /** cookie string used for authentication (stored in document.cookie) */
  nsString mCookie;

  /** record of last time when timestamp was displayed in user input log */
  PRTime mLastTime;

  /** Flag controlling logging of user input to STDERR */
  static PRBool mLoggingEnabled;

  static PRBool mLoggingInitialized;

};
