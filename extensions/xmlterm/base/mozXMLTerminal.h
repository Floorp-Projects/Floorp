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

#include "mozXMLT.h"

#include "mozILineTermAux.h"
#include "mozIXMLTerminal.h"
#include "mozXMLTermSession.h"
#include "mozXMLTermListeners.h"
#include "mozIXMLTermStream.h"


class mozXMLTerminal : public mozIXMLTerminal,
                       public nsIDocumentLoaderObserver,
                       public nsIObserver,
                       public nsSupportsWeakReference
{
  public:

  mozXMLTerminal();
  virtual ~mozXMLTerminal();

  // nsISupports interface
  NS_DECL_ISUPPORTS

  // mozIXMLTerminal interface

  NS_IMETHOD Init(nsIDocShell* aDocShell,
                  mozIXMLTermShell* aXMLTermShell,
                  const PRUnichar* aURL,
                  const PRUnichar* args);

  NS_IMETHOD Finalize(void);
  NS_IMETHOD Poll(void);

  NS_IMETHOD GetCurrentEntryNumber(PRInt32 *aNumber);
  NS_IMETHOD GetHistory(PRInt32 *aHistory);
  NS_IMETHOD SetHistory(PRInt32 aHistory);
  NS_IMETHOD GetPrompt(PRUnichar **aPrompt);
  NS_IMETHOD SetPrompt(const PRUnichar* aPrompt);
  NS_IMETHOD GetKeyIgnore(PRBool* aIgnore);
  NS_IMETHOD SetKeyIgnore(const PRBool aIgnore);

  NS_IMETHOD SendTextAux(const nsString& aString);
  NS_IMETHOD SendText(const nsString& aString, const PRUnichar* aCookie);

  NS_IMETHOD Paste();

  NS_IMETHOD GetDocument(nsIDOMDocument** aDoc);

  NS_IMETHOD GetDocShell(nsIDocShell** aDocShell);

  NS_IMETHOD GetPresShell(nsIPresShell** aPresShell);

  NS_IMETHOD GetDOMDocument(nsIDOMDocument** aDOMDocument);

  NS_IMETHOD GetSelectionController(nsISelectionController** aSelectionController);

  NS_IMETHOD GetScreenMode(PRBool* aFlag);

  NS_IMETHOD MatchesCookie(const PRUnichar* aCookie, PRBool *_retval);

  NS_IMETHOD Resize(void);

  NS_IMETHOD ShowCaret(void);

  NS_IMETHOD ScreenSize(PRInt32& rows, PRInt32& cols,
                        PRInt32& xPixels, PRInt32& yPixels);

  // nsIDocumentLoaderObserver interface
  NS_DECL_NSIDOCUMENTLOADEROBSERVER

  // nsIObserver interface
  NS_IMETHOD Observe(nsISupports *aSubject, const PRUnichar *aTopic,
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
