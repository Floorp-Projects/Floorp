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


class mozXMLTermShell : public mozIXMLTermShell
{
 public: 

  mozXMLTermShell();
  virtual ~mozXMLTermShell();

  NS_DECL_ISUPPORTS

  // mozIXMLTermShell interface
    
  NS_IMETHOD GetCurrentEntryNumber(PRInt32 *aNumber);

  NS_IMETHOD SetHistory(PRInt32 aHistory, const PRUnichar* aCookie);

  NS_IMETHOD SetPrompt(const PRUnichar* aPrompt, const PRUnichar* aCookie);

  NS_IMETHOD IgnoreKeyPress(PRBool aIgnore,
                            const PRUnichar* aCookie);

  NS_IMETHOD Init(nsIDOMWindowInternal* aContentWin,
                  const PRUnichar* URL,
                  const PRUnichar* args);

  NS_IMETHOD Close(const PRUnichar* aCookie);

  NS_IMETHOD Poll(void);

  NS_IMETHOD Resize(void);

  NS_IMETHOD SendText(const PRUnichar* aString, const PRUnichar* aCookie);

  NS_IMETHOD Exit(void);

  NS_IMETHOD Finalize(void);

protected:

  /** object initialization flag */
  PRBool mInitialized;

  /** non-owning reference to content window for XMLterm */		
  nsIDOMWindowInternal* mContentWindow;

  /** non-owning reference (??) to doc shell for content window */
  nsIDocShell* mContentAreaDocShell;

  /** owning reference to XMLTerminal object created by us */
  nsCOMPtr<mozIXMLTerminal> mXMLTerminal;
};
