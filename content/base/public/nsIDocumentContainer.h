/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef nsIDocumentContainer_h___
#define nsIDocumentContainer_h___

#include "nsISupports.h"
class nsIScriptable;
class nsIScriptEnvironment;
class nsIURL;

#define NS_IDOCUMENT_CONTAINER_IID \
{ 0x8efd4470, 0x944d, 0x11d1,      \
  {0x93, 0x23, 0x00, 0x80, 0x5f, 0x8a, 0xdd, 0x32} }

class nIDocumentContainer : public nsISupports {
public:
  /**
   * Display the specified URL with the given connection.
   *
   * @param url the URL to display
   * @param connection the connection to use.
   */
  virtual void Display(nsIURL* aURL) = 0;

  /**
   * Returns a script environment for the specified language and version.
   * The expectation is that the script environment already has been
   * set up with a container object. If a script environment has already
   * been requested for the given language, the same instance should
   * be returned.
   *
   * @param language the scripting language for the environment. If this
   *        is null, returns the default scripting environment.
   * @param majorVersion the major version number of the language
   * @param minorVersion the minor version number of the language
   * @return the script environment for the language
   * @see mg.magellan.script.IScriptEnvrionment
   */
  virtual nsIScriptEnvironment*
    GetScriptEnvironment(nsString* aLanguage,
                         PRInt32 aMajorVersion,
                         PRInt32 aMinorVersion) = 0;

  /**
   * Returns the scriptable container object for the document container.
   * The scriptable object will be used as the scoping object in the
   * definition of scriptable classes used in the Document Object Model.
   *
   * @return the scriptable container for the application
   * @see mg.magellan.script.IScriptable
   * @see mg.magellan.script.IScriptEnvrionment
   */
  virtual nsIScriptable* GetScriptableContainer() = 0;
}

#endif /* nsIDocumentContainer_h___ */
