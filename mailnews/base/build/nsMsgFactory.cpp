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

#include "nsIFactory.h"
#include "nsISupports.h"
#include "msgCore.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "nsMsgBaseCID.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"

#include "nsMessengerBootstrap.h"
#include "nsMessenger.h"
#include "nsMsgGroupRecord.h"

#include "nsIAppShellComponent.h"
#include "nsIRegistry.h"

/* Include all of the interfaces our factory can generate components for */

#include "nsIUrlListenerManager.h"
#include "nsUrlListenerManager.h"
#include "nsMsgMailSession.h"
#include "nsMsgAccount.h"
#include "nsMsgAccountManager.h"
#include "nsMessengerMigrator.h"
#include "nsMsgIdentity.h"
#include "nsMsgIncomingServer.h"
#include "nsMessageViewDataSource.h"
#include "nsMsgFolderDataSource.h"
#include "nsMsgMessageDataSource.h"

#include "nsMsgAccountManagerDS.h"

#include "nsMsgBiffManager.h"
#include "nsMsgNotificationManager.h"

#include "nsCopyMessageStreamListener.h"
#include "nsMsgCopyService.h"

#include "nsMsgFolderCache.h"

#include "nsMsgStatusFeedback.h"

#include "nsMsgFilterService.h"
#include "nsMessageView.h"
#include "nsMsgWindow.h"
#include "nsMsgViewNavigationService.h"

#include "nsMsgServiceProvider.h"
#include "nsSubscribeDataSource.h"

#include "nsMsgPrintEngine.h"

static NS_DEFINE_CID(kComponentManagerCID, NS_COMPONENTMANAGER_CID);

static NS_DEFINE_CID(kCMsgMailSessionCID, NS_MSGMAILSESSION_CID); 

static NS_DEFINE_CID(kCUrlListenerManagerCID, NS_URLLISTENERMANAGER_CID);

static NS_DEFINE_CID(kCMessengerBootstrapCID, NS_MESSENGERBOOTSTRAP_CID);

static NS_DEFINE_CID(kCMsgFolderEventCID, NS_MSGFOLDEREVENT_CID);

static NS_DEFINE_CID(kCMessengerCID, NS_MESSENGER_CID);
static NS_DEFINE_CID(kCMsgGroupRecordCID, NS_MSGGROUPRECORD_CID);

static NS_DEFINE_CID(kMailNewsFolderDataSourceCID, NS_MAILNEWSFOLDERDATASOURCE_CID);
static NS_DEFINE_CID(kMailNewsMessageDataSourceCID, NS_MAILNEWSMESSAGEDATASOURCE_CID);

// account manager stuff
static NS_DEFINE_CID(kMsgAccountManagerCID, NS_MSGACCOUNTMANAGER_CID);
static NS_DEFINE_CID(kMsgAccountCID, NS_MSGACCOUNT_CID);
static NS_DEFINE_CID(kMsgIdentityCID, NS_MSGIDENTITY_CID);
static NS_DEFINE_CID(kMsgIncomingServerCID, NS_MSGINCOMINGSERVER_CID);

// account manager RDF stuff
static NS_DEFINE_CID(kMsgAccountManagerDataSourceCID, NS_MSGACCOUNTMANAGERDATASOURCE_CID);

// migrator stuff
static NS_DEFINE_CID(kMessengerMigratorCID, NS_MESSENGERMIGRATOR_CID);
// search and filter stuff
static NS_DEFINE_CID(kMsgSearchSessionCID, NS_MSGSEARCHSESSION_CID);
static NS_DEFINE_CID(kMsgFilterServiceCID, NS_MSGFILTERSERVICE_CID);

// Biff and notifications
static NS_DEFINE_CID(kMsgBiffManagerCID, NS_MSGBIFFMANAGER_CID);
static NS_DEFINE_CID(kMsgNotificationManagerCID, NS_MSGNOTIFICATIONMANAGER_CID);

// Copy
static NS_DEFINE_CID(kCopyMessageStreamListenerCID,
                     NS_COPYMESSAGESTREAMLISTENER_CID);
static NS_DEFINE_CID(kMsgCopyServiceCID, NS_MSGCOPYSERVICE_CID);

// Msg Folder Cache stuff
static NS_DEFINE_CID(kMsgFolderCacheCID, NS_MSGFOLDERCACHE_CID);

//Feedback stuff
static NS_DEFINE_CID(kMsgStatusFeedbackCID, NS_MSGSTATUSFEEDBACK_CID);

//MessageView
static NS_DEFINE_CID(kMessageViewCID, NS_MESSAGEVIEW_CID);

//MsgWindow
static NS_DEFINE_CID(kMsgWindowCID, NS_MSGWINDOW_CID);

//MsgViewNavigationService
static NS_DEFINE_CID(kMsgViewNavigationServiceCID, NS_MSGVIEWNAVIGATIONSERVICE_CID);

//MsgServiceProviderService
static NS_DEFINE_CID(kMsgServiceProviderServiceCID, NS_MSGSERVICEPROVIDERSERVICE_CID);

//SubscribeDataSource
static NS_DEFINE_CID(kSubscribeDataSourceCID, NS_SUBSCRIBEDATASOURCE_CID);

// Print Engine
static NS_DEFINE_CID(kMsgPrintEngineCID, NS_MSG_PRINTENGINE_CID);

// private factory declarations for each component we know how to produce

NS_GENERIC_FACTORY_CONSTRUCTOR(nsMessengerBootstrap)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUrlListenerManager)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgMailSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMessenger)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgAccountManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMessengerMigrator, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgAccount)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgIdentity)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgFolderDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgMessageDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgAccountManagerDataSource)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFilterService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgBiffManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgNotificationManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCopyMessageStreamListener)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCopyService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFolderCache)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgStatusFeedback)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMessageView,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgWindow,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgViewNavigationService,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgServiceProviderService, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSubscribeDataSource, Init);
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgPrintEngine, Init)
// Module implementation for the sample library
class nsMsgBaseModule : public nsIModule
{
public:
    nsMsgBaseModule();
    virtual ~nsMsgBaseModule();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIMODULE

protected:
    nsresult Initialize();

    void Shutdown();

    PRBool mInitialized;

    nsCOMPtr<nsIGenericFactory> mMessengerBootstrapFactory;
    nsCOMPtr<nsIGenericFactory> mUrlListenerManagerFactory;
    nsCOMPtr<nsIGenericFactory> mMsgMailSessionFactory;
    nsCOMPtr<nsIGenericFactory> mMessengerFactory;
    nsCOMPtr<nsIGenericFactory> mMsgAccountManagerFactory;
    nsCOMPtr<nsIGenericFactory> mMessengerMigratorFactory;
    nsCOMPtr<nsIGenericFactory> mMsgAccountFactory;
    nsCOMPtr<nsIGenericFactory> mMsgIdentityFactory;
    nsCOMPtr<nsIGenericFactory> mMsgFolderDataSourceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgMessageDataSourceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgAccountManagerDataSourceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgFilterServiceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgBiffManagerFactory;
    nsCOMPtr<nsIGenericFactory> mMsgNotificationManagerFactory;
    nsCOMPtr<nsIGenericFactory> mCopyMessageStreamListenerFactory;
    nsCOMPtr<nsIGenericFactory> mMsgCopyServiceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgFolderCacheFactory;
    nsCOMPtr<nsIGenericFactory> mMsgStatusFeedbackFactory;
    nsCOMPtr<nsIGenericFactory> mMessageViewFactory;
    nsCOMPtr<nsIGenericFactory> mMsgWindowFactory;
    nsCOMPtr<nsIGenericFactory> mMsgViewNavigationServiceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgServiceProviderServiceFactory;
    nsCOMPtr<nsIGenericFactory> mSubscribeDataSourceFactory;
    nsCOMPtr<nsIGenericFactory> mMsgPrintEngineFactory;
};

nsMsgBaseModule::nsMsgBaseModule()
    : mInitialized(PR_FALSE)
{
    NS_INIT_ISUPPORTS();
}

nsMsgBaseModule::~nsMsgBaseModule()
{
    Shutdown();
}

NS_IMPL_ISUPPORTS(nsMsgBaseModule, NS_GET_IID(nsIModule))

// Perform our one-time intialization for this module
nsresult nsMsgBaseModule::Initialize()
{
    if (mInitialized)
        return NS_OK;

    mInitialized = PR_TRUE;
    return NS_OK;
}

// Shutdown this module, releasing all of the module resources
void nsMsgBaseModule::Shutdown()
{
    // Release the factory object
    mMessengerBootstrapFactory = null_nsCOMPtr();
    mUrlListenerManagerFactory = null_nsCOMPtr();
    mMsgMailSessionFactory = null_nsCOMPtr();
    mMessengerFactory = null_nsCOMPtr();
    mMsgAccountManagerFactory = null_nsCOMPtr();
    mMessengerMigratorFactory = null_nsCOMPtr();
    mMsgAccountFactory = null_nsCOMPtr();
    mMsgIdentityFactory = null_nsCOMPtr();
    mMsgFolderDataSourceFactory = null_nsCOMPtr();
    mMsgMessageDataSourceFactory = null_nsCOMPtr();
    mMsgAccountManagerDataSourceFactory = null_nsCOMPtr();
    mMsgFilterServiceFactory = null_nsCOMPtr();
    mMsgBiffManagerFactory = null_nsCOMPtr();
    mMsgNotificationManagerFactory = null_nsCOMPtr();
    mCopyMessageStreamListenerFactory = null_nsCOMPtr();
    mMsgCopyServiceFactory = null_nsCOMPtr();
    mMsgFolderCacheFactory = null_nsCOMPtr();
    mMsgStatusFeedbackFactory = null_nsCOMPtr();
    mMessageViewFactory = null_nsCOMPtr();
    mMsgWindowFactory = null_nsCOMPtr();
    mMsgViewNavigationServiceFactory = null_nsCOMPtr();
    mMsgServiceProviderServiceFactory = null_nsCOMPtr();
    mSubscribeDataSourceFactory = null_nsCOMPtr();
}

// Create a factory object for creating instances of aClass.
NS_IMETHODIMP nsMsgBaseModule::GetClassObject(nsIComponentManager *aCompMgr,
                               const nsCID& aClass,
                               const nsIID& aIID,
                               void** r_classObj)
{
    nsresult rv=NS_OK;

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

    if (aClass.Equals(kCMessengerBootstrapCID))
    {
        if (!mMessengerBootstrapFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMessengerBootstrapFactory), &nsMessengerBootstrapConstructor);
        fact = mMessengerBootstrapFactory;
    }
    else if (aClass.Equals(kCUrlListenerManagerCID))
    {
        if (!mUrlListenerManagerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mUrlListenerManagerFactory), &nsUrlListenerManagerConstructor);
        fact = mUrlListenerManagerFactory;
    }
    else if (aClass.Equals(kCMsgMailSessionCID))
    {
        if (!mMsgMailSessionFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgMailSessionFactory), &nsMsgMailSessionConstructor);
        fact = mMsgMailSessionFactory;
    }
    else if (aClass.Equals(kCMessengerCID))
    {
        if (!mMessengerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMessengerFactory), &nsMessengerConstructor);
        fact = mMessengerFactory;
    }

    else if (aClass.Equals(kMsgAccountManagerCID))
    {
        if (!mMsgAccountManagerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgAccountManagerFactory), &nsMsgAccountManagerConstructor);
        fact = mMsgAccountManagerFactory;
    }
    else if (aClass.Equals(kMessengerMigratorCID))
    {
        if (!mMessengerMigratorFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMessengerMigratorFactory), &nsMessengerMigratorConstructor);
        fact = mMessengerMigratorFactory;
    }
    else if (aClass.Equals(kMsgAccountCID))
    {
        if (!mMsgAccountFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgAccountFactory), &nsMsgAccountConstructor);
        fact = mMsgAccountFactory;
    }
    else if (aClass.Equals(kMsgIdentityCID))
    {
        if (!mMsgIdentityFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgIdentityFactory), &nsMsgIdentityConstructor);
        fact = mMsgIdentityFactory;
    }
    else if (aClass.Equals(kMailNewsFolderDataSourceCID)) 
    {
        if (!mMsgFolderDataSourceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgFolderDataSourceFactory), &nsMsgFolderDataSourceConstructor);
        fact = mMsgFolderDataSourceFactory;
    }
    else if (aClass.Equals(kMailNewsMessageDataSourceCID)) 
    {
        if (!mMsgMessageDataSourceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgMessageDataSourceFactory), &nsMsgMessageDataSourceConstructor);
        fact = mMsgMessageDataSourceFactory;
    }
    else if (aClass.Equals(kMsgAccountManagerDataSourceCID)) 
    {
        if (!mMsgAccountManagerDataSourceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgAccountManagerDataSourceFactory), &nsMsgAccountManagerDataSourceConstructor);
        fact = mMsgAccountManagerDataSourceFactory;
    }
    else if (aClass.Equals(kMsgFilterServiceCID)) 
    {
        if (!mMsgFilterServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgFilterServiceFactory), &nsMsgFilterServiceConstructor);
        fact = mMsgFilterServiceFactory;
    }
    else if (aClass.Equals(kMsgBiffManagerCID)) 
    {
        if (!mMsgBiffManagerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgBiffManagerFactory), &nsMsgBiffManagerConstructor);
        fact = mMsgBiffManagerFactory;
    }
    else if (aClass.Equals(kMsgNotificationManagerCID)) 
    {
        if (!mMsgNotificationManagerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgNotificationManagerFactory), &nsMsgNotificationManagerConstructor);
        fact = mMsgNotificationManagerFactory;
    }
    else if (aClass.Equals(kCopyMessageStreamListenerCID)) 
    {
        if (!mCopyMessageStreamListenerFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mCopyMessageStreamListenerFactory), &nsCopyMessageStreamListenerConstructor);
        fact = mCopyMessageStreamListenerFactory;
    }
    else if (aClass.Equals(kMsgCopyServiceCID)) 
    {
        if (!mMsgCopyServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgCopyServiceFactory), &nsMsgCopyServiceConstructor);
        fact = mMsgCopyServiceFactory;
    }
    else if (aClass.Equals(kMsgFolderCacheCID)) 
    {
        if (!mMsgFolderCacheFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgFolderCacheFactory), &nsMsgFolderCacheConstructor);
        fact = mMsgFolderCacheFactory;
    }
    else if (aClass.Equals(kMsgStatusFeedbackCID)) 
    {
        if (!mMsgStatusFeedbackFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgStatusFeedbackFactory), &nsMsgStatusFeedbackConstructor);
        fact = mMsgStatusFeedbackFactory;
    }
    else if (aClass.Equals(kMessageViewCID)) 
    {
        if (!mMessageViewFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMessageViewFactory), &nsMessageViewConstructor);
        fact = mMessageViewFactory;
    }
    else if (aClass.Equals(kMsgWindowCID)) 
    {
        if (!mMsgWindowFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgWindowFactory), &nsMsgWindowConstructor);
        fact = mMsgWindowFactory;
    }
    else if (aClass.Equals(kMsgViewNavigationServiceCID)) 
    {
        if (!mMsgViewNavigationServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgViewNavigationServiceFactory), &nsMsgViewNavigationServiceConstructor);
        fact = mMsgViewNavigationServiceFactory;
    }

    else if (aClass.Equals(kMsgServiceProviderServiceCID))
    {
        if (!mMsgServiceProviderServiceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgServiceProviderServiceFactory), &nsMsgServiceProviderServiceConstructor);
        fact = mMsgServiceProviderServiceFactory;
    }
    else if (aClass.Equals(kSubscribeDataSourceCID))
    {
        if (!mSubscribeDataSourceFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mSubscribeDataSourceFactory), &nsSubscribeDataSourceConstructor);
        fact = mSubscribeDataSourceFactory;
    }
    else if (aClass.Equals(kMsgPrintEngineCID))
    {
        if (!mMsgPrintEngineFactory)
            rv = NS_NewGenericFactory(getter_AddRefs(mMsgPrintEngineFactory), &nsMsgPrintEngineConstructor);
        fact = mMsgPrintEngineFactory;
    }

    
    if (fact)
        rv = fact->QueryInterface(aIID, r_classObj);

    return rv;
}

struct Components {
    const char* mDescription;
    const nsID* mCID;
    const char* mProgID;
    NSRegisterSelfProcPtr mRegisterSelfProc;
    NSUnregisterSelfProcPtr mUnregisterSelfProc;
};

// The list of components we register
static Components gComponents[] = {
    { "Netscape Messenger Bootstrapper", &kCMessengerBootstrapCID,
      NS_MESSENGERBOOTSTRAP_PROGID },
    { "Mail Startup Handler", &kCMessengerBootstrapCID,
      NS_MAILSTARTUPHANDLER_PROGID,
      nsMessengerBootstrap::RegisterProc,
      nsMessengerBootstrap::UnregisterProc },
    { "UrlListenerManager", &kCUrlListenerManagerCID,
      NS_URLLISTENERMANAGER_PROGID },
    { "Mail Session", &kCMsgMailSessionCID,
      NS_MSGMAILSESSION_PROGID },    
    { "Messenger DOM interaction object", &kCMessengerCID,
      NS_MESSENGER_PROGID },
    { "Messenger Account Manager", &kMsgAccountManagerCID,
      NS_MSGACCOUNTMANAGER_PROGID },
    { "Messenger Migrator", &kMessengerMigratorCID,
      NS_MESSENGERMIGRATOR_PROGID },
    { "Messenger User Account", &kMsgAccountCID,
      NS_MSGACCOUNT_PROGID },
    { "Messenger User Identity", &kMsgIdentityCID,
      NS_MSGIDENTITY_PROGID },
    { "Mail/News Folder Data Source", &kMailNewsFolderDataSourceCID,
      NS_MAILNEWSFOLDERDATASOURCE_PROGID },
    { "Mail/News Message Data Source", &kMailNewsMessageDataSourceCID,
      NS_MAILNEWSMESSAGEDATASOURCE_PROGID},
    { "Mail/News Account Manager Data Source", &kMsgAccountManagerDataSourceCID,
      NS_RDF_DATASOURCE_PROGID_PREFIX "msgaccountmanager"},
    { "Message Filter Service", &kMsgFilterServiceCID,
      NS_MSGFILTERSERVICE_PROGID},
    { "Messenger Biff Manager", &kMsgBiffManagerCID,
      NS_MSGBIFFMANAGER_PROGID},
    { "Mail/News Notification Manager", &kMsgNotificationManagerCID,
      NS_MSGNOTIFICATIONMANAGER_PROGID},
    { "Mail/News CopyMessage Stream Listener", &kCopyMessageStreamListenerCID,
      NS_COPYMESSAGESTREAMLISTENER_PROGID},
    { "Mail/News Message Copy Service", &kMsgCopyServiceCID,
      NS_MSGCOPYSERVICE_PROGID},
    { "Mail/News Folder Cache", &kMsgFolderCacheCID,
      NS_MSGFOLDERCACHE_PROGID},
    { "Mail/News Status Feedback", &kMsgStatusFeedbackCID,
      NS_MSGSTATUSFEEDBACK_PROGID},
    { "Mail/News MessageView", &kMessageViewCID,
      NS_MESSAGEVIEW_PROGID},
    { "Mail/News MsgWindow", &kMsgWindowCID,
      NS_MSGWINDOW_PROGID},
    { "Mail/News Message Navigation Service", &kMsgViewNavigationServiceCID,
      NS_MSGVIEWNAVIGATIONSERVICE_PROGID},
    { "Mail/News Print Engine", &kMsgPrintEngineCID,
      NS_MSGPRINTENGINE_PROGID},
    { "Mail/News Service Provider Service", &kMsgServiceProviderServiceCID,
      NS_MSGSERVICEPROVIDERSERVICE_PROGID},
    { "Mail/News Subscribe Data Source", &kSubscribeDataSourceCID,
      NS_SUBSCRIBEDATASOURCE_PROGID}

};

#define NUM_COMPONENTS (sizeof(gComponents) / sizeof(gComponents[0]))

NS_IMETHODIMP nsMsgBaseModule::RegisterSelf(nsIComponentManager *aCompMgr,
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
        if (NS_FAILED(rv)) break;

        if (cp->mRegisterSelfProc) {
            rv = cp->mRegisterSelfProc(aCompMgr,aPath,registryLocation,componentType);
            if (NS_FAILED(rv)) break;
        }

        cp++;
    }

   /* Add to MessengerBootstrap appshell component list. */
    NS_WITH_SERVICE(nsIRegistry, registry, NS_REGISTRY_PROGID, &rv); 

    if ( NS_SUCCEEDED( rv ) ) { 
      registry->OpenWellKnownRegistry(nsIRegistry::ApplicationComponentRegistry);
      char buffer[256]; 
      char *cid = kCMessengerBootstrapCID.ToString();
      PR_snprintf( buffer, 
                   sizeof buffer, 
                   "%s/%s", 
                   NS_IAPPSHELLCOMPONENT_KEY, 
                   cid ? cid : "unknown" ); 
      nsCRT::free(cid); 
      nsRegistryKey key; 
      rv = registry->AddSubtree( nsIRegistry::Common, 
                                 buffer, 
                                 &key );
	}
	  
  return rv;
}

NS_IMETHODIMP nsMsgBaseModule::UnregisterSelf(nsIComponentManager* aCompMgr,
                                              nsIFile* aPath,
                                              const char* registryLocation)
{
    nsresult rv = NS_OK;

    Components* cp = gComponents;
    Components* end = cp + NUM_COMPONENTS;
    while (cp < end) 
    {
        aCompMgr->UnregisterComponentSpec(*cp->mCID, aPath);

        if (cp->mRegisterSelfProc) {
            rv = cp->mUnregisterSelfProc(aCompMgr,aPath,registryLocation);
            if (NS_FAILED(rv)) break;
        }


        cp++;
    }

    return rv;
}

NS_IMETHODIMP nsMsgBaseModule::CanUnload(nsIComponentManager *aCompMgr, PRBool *okToUnload)
{
    if (!okToUnload)
        return NS_ERROR_INVALID_POINTER;

    *okToUnload = PR_FALSE;
    return NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

static nsMsgBaseModule *gModule = NULL;

extern "C" NS_EXPORT nsresult NSGetModule(nsIComponentManager *servMgr,
                                          nsIFile* aPath,
                                          nsIModule** return_cobj)
{
    nsresult rv = NS_OK;

    NS_ASSERTION(return_cobj, "Null argument");
    NS_ASSERTION(gModule == NULL, "nsMsgBaseModule: Module already created.");

    // Create an initialize the imap module instance
    nsMsgBaseModule *module = new nsMsgBaseModule();
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
