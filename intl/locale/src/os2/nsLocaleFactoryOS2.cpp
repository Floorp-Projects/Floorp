/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the Mozilla OS/2 libraries.
 *
 * The Initial Developer of the Original Code is John Fairhurst,
 * <john_fairhurst@iname.com>.  Portions created by John Fairhurst are
 * Copyright (C) 1999 John Fairhurst. All Rights Reserved.
 *
 * Contributor(s): Henry Sobotka <sobotka@axess.com> 01/2000 review and update
 *
 * This Original Code has been modified by IBM Corporation.
 * Modifications made by IBM described herein are
 * Copyright (c) International Business Machines
 * Corporation, 2000
 *
 * Modifications to Mozilla code or documentation
 * identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 03/23/2000       IBM Corp.      Fixed unitialized members in ctor.
 *
 */

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"
#include "nsCollation.h"
#include "nsCollationOS2.h"
#include "nsIScriptableDateFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsDateTimeFormatOS2.h"
#include "nsIOS2Locale.h"
#include "nsLocaleOS2.h"
#include "nsLocaleFactoryOS2.h"
#include "nsLocaleCID.h"

NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
NS_DEFINE_IID(kIOS2LocaleIID, NS_IOS2LOCALE_IID);
NS_DEFINE_IID(kLocaleFactoryOS2CID, NS_OS2LOCALEFACTORY_CID);
NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);                                                        
NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);                                                         
NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
NS_DEFINE_CID(kScriptableDateFormatCID, NS_SCRIPTABLEDATEFORMAT_CID);

// ctor/dtor
nsLocaleFactoryOS2::nsLocaleFactoryOS2() : mSysLocale(nsnull),
                                           mAppLocale(nsnull)
{
   // don't need to init the ref-count because XP nsLocaleFactory does that.
  // ??? Not sure the above comment is still valid 
  NS_INIT_REFCNT();
}

nsLocaleFactoryOS2::nsLocaleFactoryOS2(const nsCID &aClass) : mSysLocale(nsnull), 
                                           mAppLocale(nsnull)
{   
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
}   

nsLocaleFactoryOS2::~nsLocaleFactoryOS2()
{
   NS_IF_RELEASE(mSysLocale);
   NS_IF_RELEASE(mAppLocale);
}

// nsILocaleFactory
nsresult nsLocaleFactoryOS2::NewLocale( nsString  **aCatList,
                                        nsString  **aValList,
                                        PRUint8     aCount,
                                        nsILocale **aLocale)
{
   if( !aCatList || !aValList || !aLocale)
      return NS_ERROR_NULL_POINTER;

   *aLocale = nsnull;
   nsOS2Locale *aLoc = new nsOS2Locale;
   if(nsnull == aLoc)
       return NS_ERROR_OUT_OF_MEMORY;

   nsresult rc = aLoc->Init( aCatList, aValList, aCount);

   if( NS_FAILED(rc))
      delete aLoc;
   else
   {
      NS_ADDREF(aLoc);
      *aLocale = (nsILocale*)aLoc;
   }

   return rc;
}

nsresult nsLocaleFactoryOS2::NewLocale( const nsString *aName,
                                        nsILocale     **aLocale)
{
   if( !aName || !aLocale)
      return NS_ERROR_NULL_POINTER;

   *aLocale = nsnull;
   nsOS2Locale *aLoc = new nsOS2Locale;
   if(nsnull == aLoc)
       return NS_ERROR_OUT_OF_MEMORY;

   nsresult rc = aLoc->Init( *aName);

   if( NS_FAILED(rc))
      delete aLoc;
   else
   {
      NS_ADDREF(aLoc);
      *aLocale = (nsILocale*)aLoc;
   }

   return rc;
}

nsresult nsLocaleFactoryOS2::GetSystemLocale( nsILocale **aSysLocale)
{
   if( !aSysLocale)
      return NS_ERROR_NULL_POINTER;

   if( !mSysLocale)
      mSysLocale = new nsSystemLocale;

   NS_ADDREF(mSysLocale);

   return NS_OK;
}

nsresult nsLocaleFactoryOS2::GetApplicationLocale( nsILocale **aAppLocale)
{
   if( !aAppLocale)
      return NS_ERROR_NULL_POINTER;

   if( !mAppLocale)
      mAppLocale = new nsApplicationLocale;

   NS_ADDREF(mAppLocale);

   return NS_OK;
}


NS_IMETHODIMP
nsLocaleFactoryOS2::CreateInstance(nsISupports* aOuter, REFNSIID aIID,
		void** aResult)
{
  if (aResult == NULL) {   
    return NS_ERROR_NULL_POINTER;   
  }   

  // Always NULL result, in case of failure   
  *aResult = NULL;   
  nsISupports *inst = NULL;

  if (aIID.Equals(kISupportsIID)) {
    *aResult = (void *)(nsISupports*)this;   
    NS_ADDREF_THIS(); // Increase reference count for caller   
  }
  else if (aIID.Equals(kIFactoryIID)) {   
    *aResult = (void *)(nsIFactory*)this;   
    NS_ADDREF_THIS(); // Increase reference count for caller   
  }
  else if (aIID.Equals(kIOS2LocaleIID)) {
    nsOS2Locale *localeImpl = new nsOS2Locale();
    if(localeImpl)
      localeImpl->AddRef();
    *aResult = (void*)localeImpl;
  }
  else if (aIID.Equals(kICollationFactoryIID)) {
     NS_NEWXPCOM(inst, nsCollationFactory);
  }
  else if (aIID.Equals(kICollationIID)) {
     NS_NEWXPCOM(inst, nsCollationOS2);
  }
  else if (aIID.Equals(kIDateTimeFormatIID)) {
     NS_NEWXPCOM(inst, nsDateTimeFormatOS2);
  }
  else if (aIID.Equals(nsIScriptableDateFormat::GetIID())) {
     inst = NEW_SCRIPTABLE_DATEFORMAT();
  }
  else if (mClassID.Equals(kScriptableDateFormatCID)) {
     inst = NEW_SCRIPTABLE_DATEFORMAT();
  }
  if (*aResult == NULL && !inst)
    return NS_NOINTERFACE;   

  nsresult ret = NS_OK;

  if (inst) {
    NS_ADDREF(inst);
    ret = inst->QueryInterface(aIID, aResult);
    NS_RELEASE(inst);
  }
  return ret;
}

NS_IMETHODIMP
nsLocaleFactoryOS2::LockFactory(PRBool	aBool)
{
	return NS_OK;
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocaleFactoryOS2, nsIFactory)
