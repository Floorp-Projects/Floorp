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
 * 07/05/2000       IBM Corp.      Reworded file after unix model.
 */

#include "nscore.h"
#include "nsISupports.h"
#include "nsIFactory.h"
#include "nsCOMPtr.h"
#include "nsCollationOS2.h"
#include "nsIScriptableDateFormat.h"
#include "nsDateTimeFormatCID.h"
#include "nsDateTimeFormatOS2.h"
#include "nsLocaleFactoryOS2.h"
#include "nsLanguageAtomService.h"


NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
NS_DEFINE_IID(kIFactoryIID,  NS_IFACTORY_IID);
NS_DEFINE_IID(kICollationFactoryIID, NS_ICOLLATIONFACTORY_IID);                                                        
NS_DEFINE_IID(kICollationIID, NS_ICOLLATION_IID);                                                         
NS_DEFINE_IID(kIDateTimeFormatIID, NS_IDATETIMEFORMAT_IID);
NS_DEFINE_CID(kScriptableDateFormatCID, NS_SCRIPTABLEDATEFORMAT_CID);
NS_DEFINE_CID(kLanguageAtomServiceCID, NS_LANGUAGEATOMSERVICE_CID);

nsLocaleOS2Factory::nsLocaleOS2Factory(const nsCID &aClass)  
{   
  NS_INIT_ISUPPORTS();
  mClassID = aClass;
}   

nsLocaleOS2Factory::~nsLocaleOS2Factory()
{
}

NS_IMPL_THREADSAFE_ISUPPORTS1(nsLocaleOS2Factory, nsIFactory)

nsresult nsLocaleOS2Factory::CreateInstance(nsISupports *aOuter,  
                                         const nsIID &aIID,  
                                         void **aResult)
{
  if (aResult == NULL) {  
    return NS_ERROR_NULL_POINTER;  
  }  

  *aResult = NULL;  

  nsISupports *inst = NULL;

  if (aIID.Equals(kICollationFactoryIID)) {
     NS_NEWXPCOM(inst, nsCollationFactory);
  }
  else if (aIID.Equals(kICollationIID)) {
     NS_NEWXPCOM(inst, nsCollationOS2);
  }
  else if (aIID.Equals(kIDateTimeFormatIID)) {
     NS_NEWXPCOM(inst, nsDateTimeFormatOS2);
  }
  else if (aIID.Equals(NS_GET_IID(nsIScriptableDateFormat))) {
     inst = NEW_SCRIPTABLE_DATEFORMAT();
  }
  else if (mClassID.Equals(kScriptableDateFormatCID)) {
     inst = NEW_SCRIPTABLE_DATEFORMAT();
  }
  else if (mClassID.Equals(kLanguageAtomServiceCID)) {
     NS_NEWXPCOM(inst, nsLanguageAtomService);
  }
  else 
  {
    return NS_NOINTERFACE;
  }

  if (NULL == inst) {
    return NS_ERROR_OUT_OF_MEMORY;  
  }
  
  NS_ADDREF(inst);
  nsresult ret = inst->QueryInterface(aIID, aResult);
  NS_RELEASE(inst);
  return ret;
}

nsresult nsLocaleOS2Factory::LockFactory(PRBool aLock)
{
   // Not implemented in simplest case.
   return NS_OK;
}

