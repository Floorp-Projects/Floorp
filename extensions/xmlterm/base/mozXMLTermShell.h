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

// mozXMLTermShell.h: declaration of mozXMLTermShell
// which implements mozIXMLTermShell, providing an XPCONNECT wrapper
// to the XMLTerminal interface, thus allowing easy (and controlled)
// access from scripts

#include <stdio.h>

#include "nscore.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsString.h"

#include "mozXMLT.h"
#include "mozIXMLTerminal.h"
#include "mozIXMLTermShell.h"


class mozXMLTermShell : public mozIXMLTermShell {
 public: 

  mozXMLTermShell();
  virtual ~mozXMLTermShell();

  NS_DECL_ISUPPORTS

  // mozIXMLTermShell interface
    
  NS_IMETHOD GetCurrentEntryNumber(PRInt32 *aNumber);

  NS_IMETHOD GetHistory(PRInt32 *aHistory);
  NS_IMETHOD SetHistory(PRInt32 aHistory);
  NS_IMETHOD GetPrompt(PRUnichar **aPrompt);
  NS_IMETHOD SetPrompt(const PRUnichar* aPrompt);

  NS_IMETHOD Init(nsIDOMWindow* aContentWin,
                  const PRUnichar* URL,
                  const PRUnichar* args);

  NS_IMETHOD Finalize(void);
  NS_IMETHOD Poll(void);

  NS_IMETHOD SendText(const PRUnichar* buf, const PRUnichar* cookie);

  NS_IMETHOD NewXMLTermWindow(const PRUnichar* args);

  NS_IMETHOD Exit(void);

protected:

  /** object initialization flag */
  PRBool mInitialized;

  /** non-owning reference to content window for XMLterm */		
  nsIDOMWindow* mContentWindow;

  /** non-owning reference (??) to web shell for content window */
  nsIWebShell* mContentAreaWebShell;

  /** owning reference to XMLTerminal object created by us */
  nsCOMPtr<mozIXMLTerminal> mXMLTerminal;
};
