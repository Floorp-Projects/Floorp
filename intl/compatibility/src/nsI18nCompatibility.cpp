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
 *   Pierre Phaneuf <pp@ludusdesign.com>
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

#include "pratom.h"
#include "nsISupports.h"
#include "nsIComponentManager.h"
#include "nsCOMPtr.h"
#include "nsReadableUtils.h"
#include "nsIFactory.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsIServiceManager.h"
#include "nsII18nCompatibility.h"
#include "nsI18nCompatibility.h"
#include "nsIFile.h"
#include "nsFileSpec.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);
static NS_DEFINE_CID(kI18nCompatibilityCID, NS_I18NCOMPATIBILITY_CID);

 
///////////////////////////////////////////////////////////////////////////////////////////

class nsI18nCompatibility : public nsII18nCompatibility {
 public: 
  NS_DECL_ISUPPORTS 

  /* wstring CSIDtoCharsetName (in unsigned short csid); */
  NS_IMETHOD  CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval);

  nsI18nCompatibility() {NS_INIT_REFCNT();}
  virtual ~nsI18nCompatibility() {}
};

NS_IMPL_ISUPPORTS1(nsI18nCompatibility, nsII18nCompatibility)

NS_IMETHODIMP nsI18nCompatibility::CSIDtoCharsetName(PRUint16 csid, PRUnichar **_retval)
{
  nsString charsetname; charsetname.AssignWithConversion(I18N_CSIDtoCharsetName(csid));

  *_retval = ToNewUnicode(charsetname);

  return NS_OK;
}

NS_GENERIC_FACTORY_CONSTRUCTOR(nsI18nCompatibility)
static nsModuleComponentInfo components[] = 
{
 { "I18n compatibility", NS_I18NCOMPATIBILITY_CID, 
    NS_I18NCOMPATIBILITY_CONTRACTID, nsI18nCompatibilityConstructor}
};

NS_IMPL_NSGETMODULE(I18nCompatibility, components)
