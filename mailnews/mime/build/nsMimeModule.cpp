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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */

#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgMimeCID.h"
#include "nsCOMPtr.h"

// include files for components this factory creates...
#include "nsStreamConverter.h"
#include "nsMimeObjectClassAccess.h"
#include "nsMimeConverter.h"
#include "nsMsgHeaderParser.h"
#include "nsMimeHeaders.h"

static NS_DEFINE_CID(kCMimeMimeObjectClassAccessCID, NS_MIME_OBJECT_CLASS_ACCESS_CID);
static NS_DEFINE_CID(kCMimeConverterCID, NS_MIME_CONVERTER_CID);
static NS_DEFINE_CID(kCStreamConverterCID, NS_MAILNEWS_MIME_STREAM_CONVERTER_CID);
static NS_DEFINE_CID(kCMsgHeaderParserCID, NS_MSGHEADERPARSER_CID);
static NS_DEFINE_CID(kCIMimeHeadersCID, NS_IMIMEHEADERS_CID);

// private factory declarations for each component we know how to produce

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeObjectClassAccess)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgHeaderParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeHeaders)

// Module implementation for the sample library
class nsMimeModule : public nsIModule
{
public:
    nsMimeModule();
    virtual ~nsMimeModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mObjectClassAccessFactory;
    nsCOMPtr<nsIGenericFactory> mMimeConverterFactory;
    nsCOMPtr<nsIGenericFactory> mStreamConverterFactory;
    nsCOMPtr<nsIGenericFactory> mMsgHeaderParserFactory;
    nsCOMPtr<nsIGenericFactory> mMimeHeadersFactory;
};


nsMimeModule::nsMimeModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMimeModule::~nsMimeModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsMimeModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult nsMimeModule::Initialize()
{
    if (mInitialized)
        return NS_OK;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void nsMimeModule::Shutdown()
{
    // Release the factory object
    mObjectClassAccessFactory = null_nsCOMPtr();
    mMimeConverterFactory = null_nsCOMPtr();
    mStreamConverterFactory = null_nsCOMPtr();
    mMsgHeaderParserFactory = null_nsCOMPtr();
    mMimeHeadersFactory = null_nsCOMPtr();
}

// Destructor for stream converter
extern "C" void comi18n_destructor();
static NS_IMETHODIMP streamConverterDestructor()
{
  comi18n_destructor();
  return NS_OK;
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP nsMimeModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv;

    // Defensive programming: Initialize *r_classObj in case of error below
    if (!r_classObj)
        return NS_ERROR_INVALID_POINTER;

    *r_classObj = NULL;

    // Do one-time-only initialization if necessary
    if (!mInitialized) 
    {
        rv = Initialize();
        if (NS_FAILED(rv)) // Initialization failed! yikes!
            return rv;
    }

    // Choose the appropriate factory, based on the desired instance
    // class type (aClass).
    nsCOMPtr<nsIGenericFactory> fact;

    if (aClass.Equals(kCMimeMimeObjectClassAccessCID))
    {
        if (!mObjectClassAccessFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mObjectClassAccessFactory), &nsMimeObjectClassAccessConstructor);
        fact = mObjectClassAccessFactory;
    }
    else if (aClass.Equals(kCMimeConverterCID))
    {
        if (!mMimeConverterFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMimeConverterFactory), &nsMimeConverterConstructor);
        fact = mMimeConverterFactory;
    }
    else if (aClass.Equals(kCStreamConverterCID)) 
    {
        if (!mStreamConverterFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mStreamConverterFactory), &nsStreamConverterConstructor);
        fact = mStreamConverterFactory;
        fact->SetDestructor(streamConverterDestructor);
    }
    else if (aClass.Equals(kCMsgHeaderParserCID)) 
    {
        if (!mMsgHeaderParserFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgHeaderParserFactory), &nsMsgHeaderParserConstructor);
        fact = mMsgHeaderParserFactory;
    }
    else if (aClass.Equals(kCIMimeHeadersCID)) 
    {
        if (!mMimeHeadersFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMimeHeadersFactory), &nsMimeHeadersConstructor);
        fact = mMimeHeadersFactory;
    }
    
    if (fact)
        rv = fact->QueryInterface(aIID, r_classObj);

    return rv;
}

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mContractID;
};

// The list of components we register
static Components gComponents[] = {
    { "MimeObjectClassAccess", &kCMimeMimeObjectClassAccessCID,
      nsnull },
    { "Mime Converter", &kCMimeConverterCID,
      nsnull},
    { "Msg Header Parser", &kCMsgHeaderParserCID,
      NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID },
    { "Mailnews Mime Stream Converter", &kCStreamConverterCID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID },
    { "Mailnews Mime Stream Converter", &kCStreamConverterCID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID1 },
    { "Mailnews Mime Stream Converter", &kCStreamConverterCID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID2 },
    { "Mime Headers", &kCIMimeHeadersCID,
      nsnull }
};

#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP nsMimeModule::RegisterSelf(nsIComponentManager *aCompMgr,
                                         nsIFile* aPath,
                                         const char* registryLocation,
                                         const char* componentType)
{
    nsresult rv = NS_OK;

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        rv = aCompMgr->RegisterComponentSpec(*cp->mCID, cp->mDescription,
                                             cp->mContractID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) 
            break;
        cp++;
    }

    return rv;
}

NS_IMETHODIMP nsMimeModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                           nsIFile* aPath,
                                           const char* registryLocation)
{
    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
       aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);
       cp++;
    }

    return NS_OK;
}

NS_IMETHODIMP nsMimeModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload)
        return NS_ERROR_INVALID_POINTER;

    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsMimeModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* aPath,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsMimeModule: Module already created.");

    // Create an initialize the imap module instance
    nsMimeModule *module = new nsMimeModule();
    if (!module)
        return NS_ERROR_OUT_OF_MEMORY;

    // Increase refcnt and store away nsIModule interface to m in return_cobj
    rv = module->QueryInterface(NS_GET_IID(nsIModule), (void**)return_cobj);
    if (NS_FAILED(rv)) 
    {
        delete module;
        module = nsnull;
    }
    gModule = module;                  // WARNING: Weak Reference
    return rv;
}
