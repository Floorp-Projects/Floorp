/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */
#include "nsCOMPtr.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsUnicharUtilCIID.h"
#include "nsCaseConversionImp2.h"
#include "nsHankakuToZenkakuCID.h"
#include "nsTextTransformFactory.h"
#include "nsICaseConversion.h"
#include "nsEntityConverter.h"
#include "nsSaveAsCharset.h"
#include "nsUUDll.h"
#include "nsFileSpec.h"
#include "nsIFile.h"
#ifdef IBMBIDI
#include "nsBidiUtilsImp.h"
#include "nsBidiImp.h"
#endif // IBMBIDI

// Functions used to create new instances of a given object by the
// generic factory.

#define MAKE_CTOR(_name)                                             \
static NS_IMETHODIMP                                                 \
CreateNew##_name(nsISupports* aOuter, REFNSIID aIID, void **aResult) \
{                                                                    \
    if (!aResult) {                                                  \
        return NS_ERROR_INVALID_POINTER;                             \
    }                                                                \
    if (aOuter) {                                                    \
        *aResult = nsnull;                                           \
        return NS_ERROR_NO_AGGREGATION;                              \
    }                                                                \
    nsISupports* inst;                                               \
    nsresult rv = NS_New##_name(&inst);                              \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
        return rv;                                                   \
    }                                                                \
    rv = inst->QueryInterface(aIID, aResult);                        \
    if (NS_FAILED(rv)) {                                             \
        *aResult = nsnull;                                           \
    }                                                                \
    NS_RELEASE(inst);                /* get rid of extra refcnt */   \
    return rv;                                                       \
}


MAKE_CTOR(HankakuToZenkaku)

NS_GENERIC_FACTORY_CONSTRUCTOR(nsCaseConversionImp2)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsEntityConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSaveAsCharset)
#ifdef IBMBIDI
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidiUtilsImp)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBidi)
#endif // IBMBIDI

//----------------------------------------------------------------------
// Since our class still refer to this two per dll global  leave it here untill
// we change their code

static nsModuleComponentInfo components[] =
{
  { "Unichar Utility", NS_UNICHARUTIL_CID, 
      NS_UNICHARUTIL_CONTRACTID, nsCaseConversionImp2Constructor},
  { "Unicode To Entity Converter", NS_ENTITYCONVERTER_CID, 
      NS_ENTITYCONVERTER_CONTRACTID, nsEntityConverterConstructor },
  { "Unicode To Charset Converter", NS_SAVEASCHARSET_CID, 
      NS_SAVEASCHARSET_CONTRACTID, nsSaveAsCharsetConstructor},
  { "Japanese Hankaku To Zenkaku", NS_HANKAKUTOZENKAKU_CID, 
      NS_HANKAKUTOZENKAKU_CONTRACTID, CreateNewHankakuToZenkaku},
#ifdef IBMBIDI
  { "Unichar Bidi Utility", NS_UNICHARBIDIUTIL_CID,
      NS_UNICHARBIDIUTIL_CONTRACTID, nsBidiUtilsImpConstructor},
  { "Bidi Reordering Engine", NS_BIDI_CID,
      NS_BIDI_CONTRACTID, nsBidiConstructor},
#endif // IBMBIDI
};

NS_IMPL_NSGETMODULE(UcharUtil, components)

