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
