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
#include "nsIGenericFactory.h"

#include "mozXMLT.h"
#include "mozIXMLTerminal.h"
#include "mozIXMLTermShell.h"


class mozXMLTermShell : public mozIXMLTermShell
{
 public: 

  mozXMLTermShell();
  virtual ~mozXMLTermShell();

  NS_DECL_ISUPPORTS
  NS_DECL_MOZIXMLTERMSHELL

  // Define a Create method to be used with a factory:
  static NS_METHOD
    Create(nsISupports *aOuter, REFNSIID aIID, void **aResult);

  static NS_METHOD
    RegisterProc(nsIComponentManager *aCompMgr,
                 nsIFile *aPath,
                 const char *registryLocation,
                 const char *componentType,
                 const nsModuleComponentInfo *info);

  static NS_METHOD
    UnregisterProc(nsIComponentManager *aCompMgr,
                   nsIFile *aPath,
                   const char *registryLocation,
                   const nsModuleComponentInfo *info);

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

  static PRBool mLoggingInitialized;
};
