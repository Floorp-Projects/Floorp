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
 *  Mark Hammond (original author)
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

#include "nsIScriptRuntime.h"
#include "nsIGenericFactory.h"


#define NS_SCRIPT_LANGUAGE_PYTHON_CID \
  { 0xcee4ee7d, 0xf230, 0x49da, { 0x94, 0xd8, 0x6a, 0x9a, 0x48, 0xe, 0x12, 0xb3 } }

#define NS_SCRIPT_LANGUAGE_PYTHON_CONTRACTID_NAME \
  "@mozilla.org/script-language;1?script-type=application/x-python"

// nsIProgrammingLanguage::PYTHON == 3
#define NS_SCRIPT_LANGUAGE_PYTHON_CONTRACTID_ID \
  "@mozilla.org/script-language;1?id=3"

class nsPythonRuntime : public nsIScriptRuntime
{
public:
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIScriptLanguage
  virtual PRUint32 GetScriptTypeID() {
    return nsIProgrammingLanguage::PYTHON;
  }

  virtual void ShutDown() {;}

  virtual nsresult CreateContext(nsIScriptContext **ret);

  virtual nsresult ParseVersion(const nsString &aVersionStr, PRUint32 *flags);

  virtual nsresult HoldScriptObject(void *object);
  virtual nsresult DropScriptObject(void *object);
 
};
