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
