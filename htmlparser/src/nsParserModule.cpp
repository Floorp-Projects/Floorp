/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsString.h"
#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsIGenericFactory.h"
#include "nsIModule.h"
#include "nsParserCIID.h"
#include "nsParser.h"
#include "nsWellFormedDTD.h"
#include "CNavDTD.h"
#include "COtherDTD.h"
#include "COtherDTD.h"
#include "nsViewSourceHTML.h"
#include "nsHTMLEntities.h"
#include "nsHTMLTokenizer.h"
//#include "nsTextTokenizer.h"
#include "nsExpatTokenizer.h"
#include "nsIParserService.h"
#include "nsElementTable.h"

#if !defined(MOZ_DISABLE_DTD_DEBUG)
#include "nsLoggingSink.h"
#endif

static NS_DEFINE_IID(kIParserServiceIID, NS_IPARSERSERVICE_IID);

class nsParserService : public nsIParserService {
public:
  nsParserService();
  virtual ~nsParserService();

  NS_DECL_ISUPPORTS

  NS_IMETHOD HTMLAtomTagToId(nsIAtom* aAtom, PRInt32* aId) const;

  NS_IMETHOD HTMLStringTagToId(const nsString &aTag, PRInt32* aId) const;

  NS_IMETHOD HTMLIdToStringTag(PRInt32 aId, nsString& aTag) const;
  
  NS_IMETHOD HTMLConvertEntityToUnicode(const nsString& aEntity, 
                                        PRInt32* aUnicode) const;
  NS_IMETHOD HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                        nsCString& aEntity) const;
  NS_IMETHOD IsContainer(PRInt32 aId, PRBool& aIsContainer) const;
  NS_IMETHOD IsBlock(PRInt32 aId, PRBool& aIsBlock) const;
};

nsParserService::nsParserService()
{
  NS_INIT_ISUPPORTS();
}

nsParserService::~nsParserService()
{
}

NS_IMPL_ISUPPORTS(nsParserService, kIParserServiceIID)
  
  
NS_IMETHODIMP 
nsParserService::HTMLAtomTagToId(nsIAtom* aAtom, PRInt32* aId) const
{
  NS_ENSURE_ARG_POINTER(aAtom);
    
  nsAutoString tagName;
  aAtom->ToString(tagName);
  *aId = nsHTMLTags::LookupTag(tagName);
 
  return NS_OK;
}
 
NS_IMETHODIMP 
nsParserService::HTMLStringTagToId(const nsString &aTag, PRInt32* aId) const
{
  *aId = nsHTMLTags::LookupTag(aTag);
  return NS_OK;
}

NS_IMETHODIMP 
nsParserService::HTMLIdToStringTag(PRInt32 aId, nsString& aTag) const
{
  aTag.AssignWithConversion( nsHTMLTags::GetStringValue((nsHTMLTag)aId) );
  return NS_OK;
}
  
NS_IMETHODIMP 
nsParserService::HTMLConvertEntityToUnicode(const nsString& aEntity, 
                                            PRInt32* aUnicode) const
{
  *aUnicode = nsHTMLEntities::EntityToUnicode(aEntity);
  return NS_OK;
}

NS_IMETHODIMP 
nsParserService::HTMLConvertUnicodeToEntity(PRInt32 aUnicode,
                                            nsCString& aEntity) const
{
  const char* str = nsHTMLEntities::UnicodeToEntity(aUnicode);
  if (str) {
    aEntity.Assign(str);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsParserService::IsContainer(PRInt32 aId, PRBool& aIsContainer) const
{
  aIsContainer = nsHTMLElement::IsContainer((eHTMLTags)aId);
  return NS_OK;
}

NS_IMETHODIMP 
nsParserService::IsBlock(PRInt32 aId, PRBool& aIsBlock) const
{
  if((aId>eHTMLTag_unknown) && (aId<eHTMLTag_userdefined)) {
    aIsBlock=((gHTMLElements[aId].IsMemberOf(kBlock))       || 
              (gHTMLElements[aId].IsMemberOf(kBlockEntity)) || 
              (gHTMLElements[aId].IsMemberOf(kHeading))     || 
              (gHTMLElements[aId].IsMemberOf(kPreformatted))|| 
              (gHTMLElements[aId].IsMemberOf(kList))); 
  }
  else {
    aIsBlock = PR_FALSE;
  }

  return NS_OK;
}

//----------------------------------------------------------------------

#if !defined(MOZ_DISABLE_DTD_DEBUG)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLoggingSink)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(CWellFormedDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(CNavDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(COtherDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(CTransitionalDTD)
NS_GENERIC_FACTORY_CONSTRUCTOR(CViewSourceHTML)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParserService)

static nsModuleComponentInfo gComponents[] = {

#if !defined(MOZ_DISABLE_DTD_DEBUG)
  { "Logging sink", NS_LOGGING_SINK_CID, NULL, nsLoggingSinkConstructor },
#endif

  { "Parser", NS_PARSER_CID, NULL, nsParserConstructor },
  { "Well formed DTD", NS_WELLFORMEDDTD_CID, NULL, CWellFormedDTDConstructor },
  { "Navigator HTML DTD", NS_CNAVDTD_CID, NULL, CNavDTDConstructor },
  { "OTHER DTD", NS_COTHER_DTD_CID, NULL, COtherDTDConstructor },
  { "Transitional DTD", NS_CTRANSITIONAL_DTD_CID, NULL,
    CTransitionalDTDConstructor },
  { "ViewSource DTD", NS_VIEWSOURCE_DTD_CID, NULL, CViewSourceHTMLConstructor },
  { "ParserService", NS_PARSERSERVICE_CID, NULL, nsParserServiceConstructor }
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

static PRBool gInitialized = PR_FALSE;

static nsresult
Initialize(nsIModule* aSelf)
{
  if (!gInitialized) {
    nsHTMLTags::AddRefTable();
    nsHTMLEntities::AddRefTable();
    InitializeElementTable();
    CNewlineToken::AllocNewline();
    gInitialized = PR_TRUE;
  }
  return NS_OK;
}

static void
Shutdown(nsIModule* aSelf)
{
  if (gInitialized) {
    nsHTMLTags::ReleaseTable();
    nsHTMLEntities::ReleaseTable();
    nsDTDContext::ReleaseGlobalObjects();
    nsParser::FreeSharedObjects();
    DeleteElementTable();
    CNewlineToken::FreeNewline();
    gInitialized = PR_FALSE;
  }
}

NS_IMPL_NSGETMODULE_WITH_CTOR_DTOR(nsParserModule, gComponents, Initialize, Shutdown)
