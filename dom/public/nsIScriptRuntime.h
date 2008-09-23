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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is mozilla.org.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mark Hammond (initial development)
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

#ifndef nsIScriptRuntime_h__
#define nsIScriptRuntime_h__

#include "nsIScriptContext.h"

// {47032A4D-0C22-4125-94B7-864A4B744335}
#define NS_ISCRIPTRUNTIME_IID \
{ 0x47032a4d, 0xc22, 0x4125, { 0x94, 0xb7, 0x86, 0x4a, 0x4b, 0x74, 0x43, 0x35 } }


/**
 * A singleton language environment for an application.  Responsible for
 * initializing and cleaning up the global language environment, and a factory
 * for language contexts
 */
class nsIScriptRuntime : public nsISupports
{
public:
  NS_DECLARE_STATIC_IID_ACCESSOR(NS_ISCRIPTRUNTIME_IID)
  /*
   * Return the language ID of this script language
   */
  virtual PRUint32 GetScriptTypeID() = 0;

  /* Parses a "version string" for the language into a bit-mask used by
   * the language implementation.  If the specified version is not supported
   * an error should be returned.  If the specified version is blank, a default
   * version should be assumed
   */
  virtual nsresult ParseVersion(const nsString &aVersionStr, PRUint32 *verFlags) = 0;
  
  /* Factory for a new context for this language */
  virtual nsresult CreateContext(nsIScriptContext **ret) = 0;
  
  /* Memory managment for script objects returned from various
   * nsIScriptContext methods.  These are identical to those in
   * nsIScriptContext, but are useful when a script context is not known.
   */
  virtual nsresult DropScriptObject(void *object) = 0;
  virtual nsresult HoldScriptObject(void *object) = 0;
};

NS_DEFINE_STATIC_IID_ACCESSOR(nsIScriptRuntime, NS_ISCRIPTRUNTIME_IID)

/* helper functions */
nsresult NS_GetScriptRuntime(const nsAString &aLanguageName,
                             nsIScriptRuntime **aRuntime);

nsresult NS_GetScriptRuntimeByID(PRUint32 aLanguageID,
                                 nsIScriptRuntime **aRuntime);

#endif // nsIScriptRuntime_h__
