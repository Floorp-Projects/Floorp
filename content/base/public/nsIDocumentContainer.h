/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef nsIDocumentContainer_h___
#define nsIDocumentContainer_h___

#include "nsISupports.h"
class nsIScriptable;
class nsIScriptEnvironment;
class nsIURI;

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
  virtual void Display(nsIURI* aURL) = 0;

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
};

#endif /* nsIDocumentContainer_h___ */
