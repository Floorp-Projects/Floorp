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

#include "nsIGenericFactory.h"

#include "nsDOMDSResource.h"
#include "nsDOMDataSource.h"
#include "nsCSSRuleDataSource.h"
#include "nsCSSDecDataSource.h"
#include "nsCSSDecIntHolder.h"
#include "inFlasher.h"
#include "inCSSValueSearch.h"
#include "inFileSearch.h"

NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsDOMDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCSSRuleDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsCSSDecDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsDOMDSResource)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCSSDecIntHolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFlasher)
NS_GENERIC_FACTORY_CONSTRUCTOR(inCSSValueSearch)
NS_GENERIC_FACTORY_CONSTRUCTOR(inFileSearch)

static nsModuleComponentInfo components[] =
{
  { "DOM DS Resource", NS_DOMDSRESOURCE_CID, NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX NS_DOMDSRESOURCE_ID, nsDOMDSResourceConstructor },
  { "DOM Datasource", NS_INSDOMDATASOURCE_CID, NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_DOMDSDATASOURCE_ID, nsDOMDataSourceConstructor },
  { "CSS Rule Datasource", NS_CSSRULE_DATASOURCE_CID, NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CSSRULEDATASOURCE_ID, nsCSSRuleDataSourceConstructor },
  { "CSS Dec Datasource", NS_CSSDEC_DATASOURCE_CID, NS_RDF_DATASOURCE_CONTRACTID_PREFIX NS_CSSDECDATASOURCE_ID, nsCSSDecDataSourceConstructor },
  { "Flasher", IN_FLASHER_CID, IN_FLASHER_CONTRACTID, inFlasherConstructor },
  { "Data Holder", NS_CSSDECINTHOLDER_CID, NS_CSSDECINTHOLDER_CONTRACTID, nsCSSDecIntHolderConstructor },
  { "CSS Value Search", IN_CSSVALUESEARCH_CID, IN_CSSVALUESEARCH_CONTRACTID, inCSSValueSearchConstructor },
  { "File Search", IN_FILESEARCH_CID, IN_FILESEARCH_CONTRACTID, inFileSearchConstructor }
};


NS_IMPL_NSGETMODULE(nsInspectorModule, components)
