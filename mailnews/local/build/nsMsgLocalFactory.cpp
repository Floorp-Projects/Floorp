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

#include "msgCore.h" // for pre-compiled headers...
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgLocalCID.h"

// include files for components this factory creates...
#include "nsMailboxUrl.h"
#include "nsPop3URL.h"
#include "nsMailboxService.h"
#include "nsLocalMailFolder.h"
#include "nsParseMailbox.h"
#include "nsPop3Service.h"
#include "nsNoneService.h"
#include "nsPop3IncomingServer.h"
#include "nsNoIncomingServer.h"
#include "nsLocalStringBundle.h"
#include "nsCOMPtr.h"

static NS_DEFINE_CID(kMailboxUrlCID, NS_MAILBOXURL_CID);
static NS_DEFINE_CID(kMailboxParserCID, NS_MAILBOXPARSER_CID);
static NS_DEFINE_CID(kMailboxServiceCID, NS_MAILBOXSERVICE_CID);
static NS_DEFINE_CID(kLocalMailFolderResourceCID, NS_LOCALMAILFOLDERRESOURCE_CID);
static NS_DEFINE_CID(kPop3ServiceCID, NS_POP3SERVICE_CID);
static NS_DEFINE_CID(kNoneServiceCID, NS_NONESERVICE_CID);
static NS_DEFINE_CID(kPop3UrlCID, NS_POP3URL_CID);
static NS_DEFINE_CID(kPop3IncomingServerCID, NS_POP3INCOMINGSERVER_CID);
static NS_DEFINE_CID(kNoIncomingServerCID, NS_NOINCOMINGSERVER_CID);
static NS_DEFINE_CID(kParseMailMsgStateCID, NS_PARSEMAILMSGSTATE_CID);
static NS_DEFINE_CID(kLocalStringServiceCID, NS_MSG_LOCALSTRINGSERVICE_CID);


// private factory declarations for each component we know how to produce

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3URL)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgMailboxParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3Service)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoneService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgLocalMailFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParseMailMessageState)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3IncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLocalStringService)

// Module implementation for the sample library
class nsMsgLocalModule : public nsIModule
{
public:
    nsMsgLocalModule();
    virtual ~nsMsgLocalModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;
    nsCOMPtr<nsIGenericFactory> mMailboxUrlFactory;
    nsCOMPtr<nsIGenericFactory> mPop3UrlFactory;
    nsCOMPtr<nsIGenericFactory> mMailboxParserFactory;
    nsCOMPtr<nsIGenericFactory> mMailboxServiceFactory;
    nsCOMPtr<nsIGenericFactory> mPop3ServiceFactory;
    nsCOMPtr<nsIGenericFactory> mNoneServiceFactory;
    nsCOMPtr<nsIGenericFactory> mLocalMailFolderFactory;
    nsCOMPtr<nsIGenericFactory> mParseMailMsgStateFactory;
    nsCOMPtr<nsIGenericFactory> mPop3IncomingServerFactory;
    nsCOMPtr<nsIGenericFactory> mNoIncomingServerFactory;
    nsCOMPtr<nsIGenericFactory> mLocalStringBundleFactory;
};


nsMsgLocalModule::nsMsgLocalModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMsgLocalModule::~nsMsgLocalModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsMsgLocalModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult nsMsgLocalModule::Initialize()
{
    if (mInitialized)
        return NS_OK;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void nsMsgLocalModule::Shutdown()
{
    // Release the factory object
    mMailboxUrlFactory = null_nsCOMPtr();
    mPop3UrlFactory = null_nsCOMPtr();
    mMailboxParserFactory = null_nsCOMPtr();
    mMailboxServiceFactory = null_nsCOMPtr();
    mPop3ServiceFactory = null_nsCOMPtr();
    mNoneServiceFactory = null_nsCOMPtr();
    mLocalMailFolderFactory = null_nsCOMPtr();
    mParseMailMsgStateFactory = null_nsCOMPtr();
    mPop3IncomingServerFactory = null_nsCOMPtr();
    mNoIncomingServerFactory = null_nsCOMPtr();
    mLocalStringBundleFactory = null_nsCOMPtr();
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP nsMsgLocalModule::GetClassObject(nsIComponentManager *aCompMgr,
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

    if (aClass.Equals(kMailboxUrlCID))
    {
        if (!mMailboxUrlFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMailboxUrlFactory), &nsMailboxUrlConstructor);
        fact = mMailboxUrlFactory;
    }
    else if (aClass.Equals(kPop3UrlCID))
    {
        if (!mPop3UrlFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mPop3UrlFactory), &nsPop3URLConstructor);
        fact = mPop3UrlFactory;
    }
    else if (aClass.Equals(kMailboxParserCID)) 
    {
        if (!mMailboxParserFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMailboxParserFactory), &nsMsgMailboxParserConstructor);
        fact = mMailboxParserFactory;
    }
    else if (aClass.Equals(kMailboxServiceCID)) 
    {
        if (!mMailboxServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMailboxServiceFactory), &nsMailboxServiceConstructor);
        fact = mMailboxServiceFactory;
    }
    else if (aClass.Equals(kPop3ServiceCID)) 
    {
        if (!mPop3ServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mPop3ServiceFactory), &nsPop3ServiceConstructor);
        fact = mPop3ServiceFactory;
    }
    else if (aClass.Equals(kNoneServiceCID)) 
    {
        if (!mNoneServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mNoneServiceFactory), &nsNoneServiceConstructor);
        fact = mNoneServiceFactory;
    }
    else if (aClass.Equals(kLocalMailFolderResourceCID)) 
    {
        if (!mLocalMailFolderFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mLocalMailFolderFactory), &nsMsgLocalMailFolderConstructor);
        fact = mLocalMailFolderFactory;
    }
    else if (aClass.Equals(kParseMailMsgStateCID)) 
    {
        if (!mParseMailMsgStateFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mParseMailMsgStateFactory), &nsParseMailMessageStateConstructor);
        fact = mParseMailMsgStateFactory;
    }
    else if (aClass.Equals(kPop3IncomingServerCID)) 
    {
        if (!mPop3IncomingServerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mPop3IncomingServerFactory), &nsPop3IncomingServerConstructor);
        fact = mPop3IncomingServerFactory;
    }
    else if (aClass.Equals(kNoIncomingServerCID)) 
    {
        if (!mNoIncomingServerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mNoIncomingServerFactory), &nsNoIncomingServerConstructor);
        fact = mNoIncomingServerFactory;
    }
    else if (aClass.Equals(kLocalStringServiceCID)) 
    {
      if (!mLocalStringBundleFactory)
       rv = NS_NewGenericFactory(getter_AddRefs(mLocalStringBundleFactory), &nsLocalStringServiceConstructor);
      fact = mLocalStringBundleFactory;
    }
    
    if (fact)
        rv = fact->QueryInterface(aIID, r_classObj);

    return rv;
}

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
};

// The list of components we register
static Components gComponents[] = {
    { "Mailbox URL", &kMailboxUrlCID,
      NS_MAILBOXURL_PROGID },
    { "Mailbox Service", &kMailboxServiceCID,
      NS_MAILBOXSERVICE_PROGID1 },
    { "Mailbox Service", &kMailboxServiceCID,
      NS_MAILBOXSERVICE_PROGID2 },
    { "Mailbox Service", &kMailboxServiceCID,
      NS_MAILBOXSERVICE_PROGID3 },
    { "Mailbox Protocol Handler", &kMailboxServiceCID,
      NS_MAILBOXSERVICE_PROGID4 },
    { "Mailbox Parser", &kMailboxParserCID,
      NS_MAILBOXPARSER_PROGID },
    { "Pop3 URL", &kPop3UrlCID,
      NS_POP3URL_PROGID },
    { "Pop3 Service", &kPop3ServiceCID,
      NS_POP3SERVICE_PROGID1 },
    { "POP Protocol Handler", &kPop3ServiceCID,
      NS_POP3SERVICE_PROGID2 },
    { "None Service", &kNoneServiceCID,
      NS_NONESERVICE_PROGID },
    { "pop3 Protocol Information", &kPop3ServiceCID,
      NS_POP3PROTOCOLINFO_PROGID },
    { "none Protocol Information", &kNoneServiceCID,
      NS_NONEPROTOCOLINFO_PROGID },
    { "Local Mail Folder Resource Factory", &kLocalMailFolderResourceCID,
      NS_LOCALMAILFOLDERRESOURCE_PROGID },
    { "Pop3 Incoming Server", &kPop3IncomingServerCID,
      NS_POP3INCOMINGSERVER_PROGID },
    { "No Incoming Server", &kNoIncomingServerCID,
      NS_NOINCOMINGSERVER_PROGID },
    { "Parse MailMessage State", &kParseMailMsgStateCID,
      NS_PARSEMAILMSGSTATE_PROGID },
    { "Mailbox String Bundle Service", &kLocalStringServiceCID,
      NS_MSG_MAILBOXSTRINGSERVICE_PROGID },
    { "Pop String Bundle Service", &kLocalStringServiceCID,
      NS_MSG_POPSTRINGSERVICE_PROGID },
};
#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP nsMsgLocalModule::RegisterSelf(nsIComponentManager *aCompMgr,
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
                                             cp->mProgID, aPath, PR_TRUE,
                                             PR_TRUE);
        if (NS_FAILED(rv)) 
            break;
        cp++;
    }

    return rv;
}

NS_IMETHODIMP nsMsgLocalModule::UnregisterSelf(nsIComponentManager* aCompMgr,
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

NS_IMETHODIMP nsMsgLocalModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload)
        return NS_ERROR_INVALID_POINTER;

    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsMsgLocalModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                            nsIFile* aPath,
                                            nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsMsgLocalModule: Module already created.");

    // Create an initialize the imap module instance
    nsMsgLocalModule *module = new nsMsgLocalModule();
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
