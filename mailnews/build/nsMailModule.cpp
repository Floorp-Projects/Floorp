/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Seth Spitzer <sspitzer@netscape.com>
 *   Scott MacGregor <mscott@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

////////////////////////////////////////////////////////////////////////////////
// Core Module Include Files
////////////////////////////////////////////////////////////////////////////////

#include "nsIFactory.h"
#include "nsISupports.h"
#include "nsIModule.h"
#include "nsIGenericFactory.h"
#include "pratom.h"
#include "nsIComponentManager.h"
#include "nsIServiceManager.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "msgCore.h"
// #include "nsIRegistry.h"

////////////////////////////////////////////////////////////////////////////////
// mailnews base includes
////////////////////////////////////////////////////////////////////////////////
#include "nsMsgBaseCID.h"
#include "rdf.h"
#include "nsMessengerBootstrap.h"
#include "nsMessenger.h"
#include "nsMsgGroupRecord.h"
#include "nsIContentViewer.h"
#include "nsIUrlListenerManager.h"
#include "nsUrlListenerManager.h"
#include "nsMsgMailSession.h"
#include "nsMsgAccount.h"
#include "nsMsgAccountManager.h"
#include "nsMessengerMigrator.h"
#include "nsMsgIdentity.h"
#include "nsMsgIncomingServer.h"
#include "nsMsgFolderDataSource.h"
#include "nsMsgAccountManagerDS.h"
#include "nsMsgBiffManager.h"
#include "nsMsgPurgeService.h"
#include "nsStatusBarBiffManager.h"
#include "nsCopyMessageStreamListener.h"
#include "nsMsgCopyService.h"
#include "nsMsgFolderCache.h"
#include "nsMsgStatusFeedback.h"
#include "nsMsgFilterService.h"
#include "nsMsgFilterDataSource.h"
#include "nsMsgFilterDelegateFactory.h"
#include "nsMsgWindow.h"
#include "nsMsgServiceProvider.h"
#include "nsSubscribeDataSource.h"
#include "nsSubscribableServer.h"
#include "nsMsgPrintEngine.h"
#include "nsMsgSearchSession.h"
#include "nsMsgSearchTerm.h"
#include "nsMsgSearchAdapter.h"
#include "nsMsgFolderCompactor.h"
#include "nsMsgThreadedDBView.h"
#include "nsMsgSpecialViews.h"
#include "nsMsgXFVirtualFolderDBView.h"
#include "nsMsgQuickSearchDBView.h"
#include "nsMsgOfflineManager.h"
#include "nsMsgProgress.h"
#include "nsSpamSettings.h"
#include "nsMsgContentPolicy.h"
#include "nsCidProtocolHandler.h"
#include "nsRssIncomingServer.h"
#include "nsRssService.h"
#ifdef XP_WIN
#include "nsMessengerWinIntegration.h"
#endif
#ifdef XP_OS2
#include "nsMessengerOS2Integration.h"
#endif

#include "nsCURILoader.h"
#include "nsMessengerContentHandler.h"

////////////////////////////////////////////////////////////////////////////////
// addrbook includes
////////////////////////////////////////////////////////////////////////////////
#include "nsAbBaseCID.h"
#include "nsDirectoryDataSource.h"
#include "nsAbBSDirectory.h"
#include "nsAbMDBDirectory.h"
#include "nsAbMDBCard.h"
#include "nsAbDirFactoryService.h"
#include "nsAbMDBDirFactory.h"
#include "nsAddrDatabase.h"
#include "nsAddressBook.h"
#include "nsAddrBookSession.h"
#include "nsAbDirProperty.h"
#include "nsAbAutoCompleteSession.h"
#include "nsAbAddressCollecter.h"
#include "nsAddbookProtocolHandler.h"
#include "nsAddbookUrl.h"

#if defined(XP_WIN) && !defined(__MINGW32__)
#include "nsAbOutlookDirectory.h"
#include "nsAbOutlookCard.h"
#include "nsAbOutlookDirFactory.h"
#endif

#include "nsAbDirectoryQuery.h"
#include "nsAbBooleanExpression.h"
#include "nsAbDirectoryQueryProxy.h"
#include "nsAbView.h"
#include "nsMsgVCardService.h"

#if defined(MOZ_LDAP_XPCOM)
#include "nsAbLDAPDirectory.h"
#include "nsAbLDAPCard.h"
#include "nsAbLDAPDirFactory.h"
#include "nsAbLDAPAutoCompFormatter.h"
#include "nsAbLDAPReplicationService.h"
#include "nsAbLDAPReplicationQuery.h"
#include "nsAbLDAPReplicationData.h"
#include "nsAbLDAPChangeLogQuery.h"
#include "nsAbLDAPChangeLogData.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// bayesian spam filter includes
////////////////////////////////////////////////////////////////////////////////
#include "nsBayesianFilterCID.h"
#include "nsBayesianFilter.h"

////////////////////////////////////////////////////////////////////////////////
// compose includes
////////////////////////////////////////////////////////////////////////////////
#include "nsMsgCompCID.h"

#include "nsMsgSendLater.h"
#include "nsSmtpUrl.h"
#include "nsISmtpService.h"
#include "nsSmtpService.h"
#include "nsMsgComposeService.h"
#include "nsMsgComposeContentHandler.h"
#include "nsMsgCompose.h"
#include "nsMsgComposeParams.h"
#include "nsMsgComposeProgressParams.h"
#include "nsMsgAttachment.h"
#include "nsMsgSend.h"
#include "nsMsgQuote.h"
#include "nsIMsgDraft.h"
#include "nsMsgCreate.h"
#include "nsURLFetcher.h"
#include "nsSmtpServer.h"
#include "nsSmtpDataSource.h"
#include "nsSmtpDelegateFactory.h"
#include "nsMsgRecipientArray.h"
#include "nsMsgComposeStringBundle.h"
#include "nsMsgCompUtils.h"

////////////////////////////////////////////////////////////////////////////////
// imap includes
////////////////////////////////////////////////////////////////////////////////
#include "nsMsgImapCID.h"
#include "nsIMAPHostSessionList.h"
#include "nsImapIncomingServer.h"
#include "nsImapService.h"
#include "nsImapMailFolder.h"
#include "nsImapUrl.h"
#include "nsImapProtocol.h"

////////////////////////////////////////////////////////////////////////////////
// local includes
////////////////////////////////////////////////////////////////////////////////
#include "nsMsgLocalCID.h"

#include "nsMailboxUrl.h"
#include "nsPop3URL.h"
#include "nsMailboxService.h"
#include "nsLocalMailFolder.h"
#include "nsParseMailbox.h"
#include "nsPop3Service.h"

#ifdef HAVE_MOVEMAIL
#include "nsMovemailService.h"
#include "nsMovemailIncomingServer.h"
#endif /* HAVE_MOVEMAIL */

#include "nsNoneService.h"
#include "nsPop3IncomingServer.h"
#include "nsNoIncomingServer.h"
#include "nsLocalStringBundle.h"

///////////////////////////////////////////////////////////////////////////////
// msgdb includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMsgDBCID.h"
#include "nsMailDatabase.h"
#include "nsNewsDatabase.h"
#include "nsImapMailDatabase.h"

///////////////////////////////////////////////////////////////////////////////
// mime includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMsgMimeCID.h"
#include "nsStreamConverter.h"
#include "nsMimeObjectClassAccess.h"
#include "nsMimeConverter.h"
#include "nsMsgHeaderParser.h"
#include "nsMimeHeaders.h"

///////////////////////////////////////////////////////////////////////////////
// mime emitter includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMimeEmitterCID.h"
#include "nsIMimeEmitter.h"
#include "nsMimeHtmlEmitter.h"
#include "nsMimeRawEmitter.h"
#include "nsMimeXmlEmitter.h"
#include "nsMimePlainEmitter.h"

///////////////////////////////////////////////////////////////////////////////
// news includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMsgNewsCID.h"
#include "nsNntpUrl.h"
#include "nsNntpService.h"
#include "nsNntpIncomingServer.h"
#include "nsNNTPNewsgroupPost.h"
#include "nsNNTPNewsgroupList.h"
#include "nsNNTPArticleList.h"
#include "nsNewsDownloadDialogArgs.h"
#include "nsNewsFolder.h"

///////////////////////////////////////////////////////////////////////////////
// mail views includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMsgMailViewsCID.h"
#include "nsMsgMailViewList.h"

///////////////////////////////////////////////////////////////////////////////
// mdn includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMsgMdnCID.h"
#include "nsMsgMdnGenerator.h"

///////////////////////////////////////////////////////////////////////////////
// vcard includes
///////////////////////////////////////////////////////////////////////////////
#include "nsMimeContentTypeHandler.h"

////////////////////////////////////////////////////////////////////////////////
// mailnews base factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMessengerBootstrap)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsUrlListenerManager)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgMailSession, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMessenger)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgAccountManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMessengerMigrator, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgAccount)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgIdentity)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgFolderDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgAccountManagerDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSearchSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSearchTerm)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSearchValidityManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFilterService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFilterDataSource)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFilterDelegateFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgBiffManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgPurgeService)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsStatusBarBiffManager, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCopyMessageStreamListener)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCopyService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgFolderCache)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgStatusFeedback)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgWindow,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgServiceProviderService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSubscribeDataSource, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsSubscribableServer, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgPrintEngine)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsFolderCompactState)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsOfflineStoreCompactState)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgThreadedDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgThreadsWithUnreadDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgWatchedThreadsWithUnreadDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSearchDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgXFVirtualFolderDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuickSearchDBView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgOfflineManager)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgProgress)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSpamSettings)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsCidProtocolHandler)
#ifdef XP_WIN
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMessengerWinIntegration, Init)
#endif
#ifdef XP_OS2
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMessengerOS2Integration, Init)
#endif
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMessengerContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgContentPolicy, Init)

////////////////////////////////////////////////////////////////////////////////
// addrbook factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddressBook)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbDirectoryDataSource,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryProperties)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbCardProperty)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBSDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddrBookSession)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbAutoCompleteSession)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsAbAddressCollecter,Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirFactoryService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbMDBDirFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAddbookProtocolHandler)

#if defined(XP_WIN) && !defined(__MINGW32__)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbOutlookDirFactory)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryQueryArguments)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBooleanConditionString)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbBooleanExpression)

#if defined(MOZ_LDAP_XPCOM)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPDirectory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPCard)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPDirFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPAutoCompFormatter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPReplicationService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPReplicationQuery)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPProcessReplicationData)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPChangeLogQuery)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbLDAPProcessChangeLogData)
#endif

NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbDirectoryQueryProxy)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsAbView)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgVCardService) 

////////////////////////////////////////////////////////////////////////////////
// bayesian spam filter factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsBayesianFilter)

////////////////////////////////////////////////////////////////////////////////
// compose factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompose)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeParams)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeSendListener)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeProgressParams)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompFields)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgAttachment)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeAndSend)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgSendLater)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDraft)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMsgComposeService, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgComposeContentHandler)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuote)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgQuoteListener)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailtoUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgRecipientArray)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsComposeStringService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpDataSource)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsSmtpDelegateFactory)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsURLFetcher)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgCompUtils)

////////////////////////////////////////////////////////////////////////////////
// imap factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsImapUrl, Initialize)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapProtocol)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsIMAPHostSessionList, Init)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMockChannel)

////////////////////////////////////////////////////////////////////////////////
// local factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3URL)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgMailboxParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailboxService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3Service)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoneService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgLocalMailFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsParseMailMessageState)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsPop3IncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRssIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsRssService)
#ifdef HAVE_MOVEMAIL
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMovemailIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMovemailService)
#endif /* HAVE_MOVEMAIL */
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNoIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsLocalStringService)

////////////////////////////////////////////////////////////////////////////////
// msgdb factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDBService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsImapMailDatabase)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgRetentionSettings)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgDownloadSettings)

////////////////////////////////////////////////////////////////////////////////
// mime factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeObjectClassAccess)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsStreamConverter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgHeaderParser)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeHeaders)

////////////////////////////////////////////////////////////////////////////////
// mime emitter factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeRawEmitter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimeXmlEmitter)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMimePlainEmitter)
NS_GENERIC_FACTORY_CONSTRUCTOR_INIT(nsMimeHtmlDisplayEmitter, Init)

static NS_METHOD RegisterMimeEmitter(nsIComponentManager *aCompMgr, nsIFile *aPath, const char *registryLocation, 
                                     const char *componentType, const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString previous;
  
  return catman->AddCategoryEntry("mime-emitter", info->mContractID, info->mContractID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

////////////////////////////////////////////////////////////////////////////////
// news factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpUrl)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpService)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNntpIncomingServer)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPArticleList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupPost)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNNTPNewsgroupList)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgNewsFolder)
NS_GENERIC_FACTORY_CONSTRUCTOR(nsNewsDownloadDialogArgs)

////////////////////////////////////////////////////////////////////////////////
// mail view factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgMailViewList)

////////////////////////////////////////////////////////////////////////////////
// mdn factories
////////////////////////////////////////////////////////////////////////////////
NS_GENERIC_FACTORY_CONSTRUCTOR(nsMsgMdnGenerator)

////////////////////////////////////////////////////////////////////////////////
// vcard factories 
////////////////////////////////////////////////////////////////////////////////

// XXX this vcard stuff needs cleaned up to use a generic factory constructor
extern "C" MimeObjectClass *
MIME_VCardCreateContentTypeHandlerClass(const char *content_type, 
                                        contentTypeHandlerInitStruct *initStruct);

static NS_IMETHODIMP nsVCardMimeContentTypeHandlerConstructor(nsISupports *aOuter,
                                         REFNSIID aIID,
                                         void **aResult)
{
  nsresult rv;
  nsMimeContentTypeHandler *inst = nsnull;

  if (NULL == aResult) 
  {
    rv = NS_ERROR_NULL_POINTER;
    return rv;
  }
  *aResult = NULL;
  if (NULL != aOuter) 
  {
    rv = NS_ERROR_NO_AGGREGATION;
    return rv;
  }
  inst = new nsMimeContentTypeHandler("text/x-vcard", &MIME_VCardCreateContentTypeHandlerClass);
  if (inst == NULL)
    return NS_ERROR_OUT_OF_MEMORY;

  NS_ADDREF(inst);
  rv = inst->QueryInterface(aIID,aResult);
  NS_RELEASE(inst);

  return rv;
}

static NS_METHOD RegisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                                       const char *registryLocation, const char *componentType,
                                       const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;
  nsXPIDLCString previous;
  return catman->AddCategoryEntry("content-policy",
                                  NS_MSGCONTENTPOLICY_CONTRACTID,
                                  NS_MSGCONTENTPOLICY_CONTRACTID,
                                  PR_TRUE, PR_TRUE, getter_Copies(previous));
}

static NS_METHOD UnregisterContentPolicy(nsIComponentManager *aCompMgr, nsIFile *aPath,
                                         const char *registryLocation,
                                         const nsModuleComponentInfo *info)
{
  nsresult rv;
  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return rv;

  return catman->DeleteCategoryEntry("content-policy", NS_MSGCONTENTPOLICY_CONTRACTID, PR_TRUE);
}

// The list of components we register
static const nsModuleComponentInfo gComponents[] = {
    ////////////////////////////////////////////////////////////////////////////////
    // mailnews base components
    ////////////////////////////////////////////////////////////////////////////////
    { "Netscape Messenger Bootstrapper", NS_MESSENGERBOOTSTRAP_CID,
      NS_MESSENGERBOOTSTRAP_CONTRACTID,
      nsMessengerBootstrapConstructor,
    },
    { "Netscape Messenger Window Service", NS_MESSENGERWINDOWSERVICE_CID,
      NS_MESSENGERWINDOWSERVICE_CONTRACTID,
      nsMessengerBootstrapConstructor,
    },
    { "Mail Startup Handler", NS_MESSENGERBOOTSTRAP_CID,
      NS_MAILSTARTUPHANDLER_CONTRACTID,
      nsMessengerBootstrapConstructor,
      nsMessengerBootstrap::RegisterProc,
      nsMessengerBootstrap::UnregisterProc
    },
    { "UrlListenerManager", NS_URLLISTENERMANAGER_CID,
      NS_URLLISTENERMANAGER_CONTRACTID,
      nsUrlListenerManagerConstructor,
    },
    { "Mail Session", NS_MSGMAILSESSION_CID,
      NS_MSGMAILSESSION_CONTRACTID,
      nsMsgMailSessionConstructor,
    },
    { "Messenger DOM interaction object", NS_MESSENGER_CID,
      NS_MESSENGER_CONTRACTID,
      nsMessengerConstructor,
    },
    { "Messenger Account Manager", NS_MSGACCOUNTMANAGER_CID,
      NS_MSGACCOUNTMANAGER_CONTRACTID,
      nsMsgAccountManagerConstructor,
    },
    { "Messenger Migrator", NS_MESSENGERMIGRATOR_CID,
      NS_MESSENGERMIGRATOR_CONTRACTID,
      nsMessengerMigratorConstructor,
    },
    { "Messenger User Account", NS_MSGACCOUNT_CID,
      NS_MSGACCOUNT_CONTRACTID,
      nsMsgAccountConstructor,
    },
    { "Messenger User Identity", NS_MSGIDENTITY_CID,
      NS_MSGIDENTITY_CONTRACTID,
      nsMsgIdentityConstructor,
    },
    { "Mail/News Folder Data Source", NS_MAILNEWSFOLDERDATASOURCE_CID,
      NS_MAILNEWSFOLDERDATASOURCE_CONTRACTID,
      nsMsgFolderDataSourceConstructor,
    },
    { "Mail/News Account Manager Data Source", NS_MSGACCOUNTMANAGERDATASOURCE_CID,
      NS_RDF_DATASOURCE_CONTRACTID_PREFIX "msgaccountmanager",
      nsMsgAccountManagerDataSourceConstructor,
    },
    { "Message Filter Service", NS_MSGFILTERSERVICE_CID,
      NS_MSGFILTERSERVICE_CONTRACTID,
      nsMsgFilterServiceConstructor,
    },
    { "Message Search Session", NS_MSGSEARCHSESSION_CID,
      NS_MSGSEARCHSESSION_CONTRACTID,
      nsMsgSearchSessionConstructor
    },
    { "Message Search Term", NS_MSGSEARCHTERM_CID,
      NS_MSGSEARCHTERM_CONTRACTID,
      nsMsgSearchTermConstructor
    },
    { "Message Search Validity Manager", NS_MSGSEARCHVALIDITYMANAGER_CID,
        NS_MSGSEARCHVALIDITYMANAGER_CONTRACTID,
        nsMsgSearchValidityManagerConstructor,
    },
    { "Message Filter Service", NS_MSGFILTERSERVICE_CID,
      NS_MSGFILTERSERVICE_CONTRACTID,
      nsMsgFilterServiceConstructor,
    },
    { "Message Filter Datasource", NS_MSGFILTERDATASOURCE_CID,
      NS_MSGFILTERDATASOURCE_CONTRACTID,
      nsMsgFilterDataSourceConstructor,
    },
    // XXX temporarily do all the protocols here
    { "Message Filter Delegate Factory", NS_MSGFILTERDELEGATEFACTORY_CID,
      NS_MSGFILTERDELEGATEFACTORY_IMAP_CONTRACTID,
      nsMsgFilterDelegateFactoryConstructor,
    },
    { "Message Filter Delegate Factory", NS_MSGFILTERDELEGATEFACTORY_CID,
      NS_MSGFILTERDELEGATEFACTORY_MAILBOX_CONTRACTID,
      nsMsgFilterDelegateFactoryConstructor,
    },
    { "Message Filter Delegate Factory", NS_MSGFILTERDELEGATEFACTORY_CID,
      NS_MSGFILTERDELEGATEFACTORY_NEWS_CONTRACTID,
      nsMsgFilterDelegateFactoryConstructor,
    },
    // XXX done temporary registration
    
    { "Messenger Biff Manager", NS_MSGBIFFMANAGER_CID,
      NS_MSGBIFFMANAGER_CONTRACTID,
      nsMsgBiffManagerConstructor,
    },
    { "Messenger Purge Service", NS_MSGPURGESERVICE_CID,
      NS_MSGPURGESERVICE_CONTRACTID,
      nsMsgPurgeServiceConstructor,
    },
    { "Status Bar Biff Manager", NS_STATUSBARBIFFMANAGER_CID,
      NS_STATUSBARBIFFMANAGER_CONTRACTID,
      nsStatusBarBiffManagerConstructor,
    },
    { "Mail/News CopyMessage Stream Listener", NS_COPYMESSAGESTREAMLISTENER_CID,
      NS_COPYMESSAGESTREAMLISTENER_CONTRACTID,
      nsCopyMessageStreamListenerConstructor,
    },
    { "Mail/News Message Copy Service", NS_MSGCOPYSERVICE_CID,
      NS_MSGCOPYSERVICE_CONTRACTID,
      nsMsgCopyServiceConstructor,
    },
    { "Mail/News Folder Cache", NS_MSGFOLDERCACHE_CID,
      NS_MSGFOLDERCACHE_CONTRACTID,
      nsMsgFolderCacheConstructor,
    },
    { "Mail/News Status Feedback", NS_MSGSTATUSFEEDBACK_CID,
      NS_MSGSTATUSFEEDBACK_CONTRACTID,
      nsMsgStatusFeedbackConstructor,
    },
    { "Mail/News MsgWindow", NS_MSGWINDOW_CID,
      NS_MSGWINDOW_CONTRACTID,
      nsMsgWindowConstructor,
    },
    { "Mail/News Print Engine", NS_MSG_PRINTENGINE_CID,
      NS_MSGPRINTENGINE_CONTRACTID,
      nsMsgPrintEngineConstructor,
    },
    { "Mail/News Service Provider Service", NS_MSGSERVICEPROVIDERSERVICE_CID,
      NS_MSGSERVICEPROVIDERSERVICE_CONTRACTID,
      nsMsgServiceProviderServiceConstructor,
    },
    { "Mail/News Subscribe Data Source", NS_SUBSCRIBEDATASOURCE_CID,
      NS_SUBSCRIBEDATASOURCE_CONTRACTID,
      nsSubscribeDataSourceConstructor,
    },
    { "Mail/News Subscribable Server", NS_SUBSCRIBABLESERVER_CID,
      NS_SUBSCRIBABLESERVER_CONTRACTID,
      nsSubscribableServerConstructor,
    },
    { "Local folder compactor", NS_MSGLOCALFOLDERCOMPACTOR_CID,
      NS_MSGLOCALFOLDERCOMPACTOR_CONTRACTID,
      nsFolderCompactStateConstructor,
    },
    { "offline store compactor", NS_MSG_OFFLINESTORECOMPACTOR_CID,
      NS_MSGOFFLINESTORECOMPACTOR_CONTRACTID,
      nsOfflineStoreCompactStateConstructor,
    },
    { "threaded db view", NS_MSGTHREADEDDBVIEW_CID,
      NS_MSGTHREADEDDBVIEW_CONTRACTID,
      nsMsgThreadedDBViewConstructor,
    },
    { "threads with unread db view", NS_MSGTHREADSWITHUNREADDBVIEW_CID,
      NS_MSGTHREADSWITHUNREADDBVIEW_CONTRACTID,
      nsMsgThreadsWithUnreadDBViewConstructor,
    },
    { "watched threads with unread db view", NS_MSGWATCHEDTHREADSWITHUNREADDBVIEW_CID,
      NS_MSGWATCHEDTHREADSWITHUNREADDBVIEW_CONTRACTID,
      nsMsgWatchedThreadsWithUnreadDBViewConstructor,
    },
    { "search db view", NS_MSGSEARCHDBVIEW_CID,
      NS_MSGSEARCHDBVIEW_CONTRACTID,
      nsMsgSearchDBViewConstructor,
    },
    { "quick search db view", NS_MSGQUICKSEARCHDBVIEW_CID,
      NS_MSGQUICKSEARCHDBVIEW_CONTRACTID,
      nsMsgQuickSearchDBViewConstructor,
    },
    { "cross folder virtual folder db view", NS_MSG_XFVFDBVIEW_CID,
       NS_MSGXFVFDBVIEW_CONTRACTID,
       nsMsgXFVirtualFolderDBViewConstructor,
    },
    { "Messenger Offline Manager", NS_MSGOFFLINEMANAGER_CID,
      NS_MSGOFFLINEMANAGER_CONTRACTID,
      nsMsgOfflineManagerConstructor,
    },
    { "Messenger Progress Manager", NS_MSGPROGRESS_CID,
      NS_MSGPROGRESS_CONTRACTID,
      nsMsgProgressConstructor,
    },
    { "Spam Settings", NS_SPAMSETTINGS_CID,
      NS_SPAMSETTINGS_CONTRACTID,
      nsSpamSettingsConstructor,
    },
    { "cid protocol", NS_CIDPROTOCOL_CID,
      NS_CIDPROTOCOLHANDLER_CONTRACTID,
      nsCidProtocolHandlerConstructor,
    },
#ifdef XP_WIN
    { "Windows OS Integration", NS_MESSENGERWININTEGRATION_CID,
      NS_MESSENGEROSINTEGRATION_CONTRACTID,
      nsMessengerWinIntegrationConstructor,
    },
#endif
#ifdef XP_OS2
    { "OS/2 OS Integration", NS_MESSENGEROS2INTEGRATION_CID,
      NS_MESSENGEROSINTEGRATION_CONTRACTID,
      nsMessengerOS2IntegrationConstructor,
    },
#endif
    { "x-message-display content handler",
       NS_MESSENGERCONTENTHANDLER_CID,
       NS_MESSENGERCONTENTHANDLER_CONTRACTID,
       nsMessengerContentHandlerConstructor
    },
    { "mail content policy enforcer", 
       NS_MSGCONTENTPOLICY_CID,
       NS_MSGCONTENTPOLICY_CONTRACTID,
       nsMsgContentPolicyConstructor,
       RegisterContentPolicy, UnregisterContentPolicy },
    
    ////////////////////////////////////////////////////////////////////////////////
    // addrbook components
    ////////////////////////////////////////////////////////////////////////////////
    { "Address Book", NS_ADDRESSBOOK_CID,
      NS_ADDRESSBOOK_CONTRACTID, nsAddressBookConstructor },
    { "Address Book Startup Handler", NS_ADDRESSBOOK_CID,
      NS_ADDRESSBOOKSTARTUPHANDLER_CONTRACTID, nsAddressBookConstructor,
      nsAddressBook::RegisterProc, nsAddressBook::UnregisterProc },
    { "Address Book Directory Datasource", NS_ABDIRECTORYDATASOURCE_CID,
      NS_ABDIRECTORYDATASOURCE_CONTRACTID, nsAbDirectoryDataSourceConstructor },
    { "Address Boot Strap Directory", NS_ABDIRECTORY_CID,
      NS_ABDIRECTORY_CONTRACTID, nsAbBSDirectoryConstructor },
    { "Address MDB Book Directory", NS_ABMDBDIRECTORY_CID,
      NS_ABMDBDIRECTORY_CONTRACTID, nsAbMDBDirectoryConstructor },
    { "Address MDB Book Card", NS_ABMDBCARD_CID,
       NS_ABMDBCARD_CONTRACTID, nsAbMDBCardConstructor },
    { "Address Database", NS_ADDRDATABASE_CID,
      NS_ADDRDATABASE_CONTRACTID, nsAddrDatabaseConstructor },
    { "Address Book Card Property", NS_ABCARDPROPERTY_CID,
      NS_ABCARDPROPERTY_CONTRACTID, nsAbCardPropertyConstructor },
    { "Address Book Directory Property", NS_ABDIRPROPERTY_CID,
      NS_ABDIRPROPERTY_CONTRACTID, nsAbDirPropertyConstructor },
    { "AB Directory Properties", NS_ABDIRECTORYPROPERTIES_CID,
      NS_ABDIRECTORYPROPERTIES_CONTRACTID, nsAbDirectoryPropertiesConstructor },
    { "Address Book Session", NS_ADDRBOOKSESSION_CID,
      NS_ADDRBOOKSESSION_CONTRACTID, nsAddrBookSessionConstructor },
    { "Address Book Auto Complete Session", NS_ABAUTOCOMPLETESESSION_CID,
      NS_ABAUTOCOMPLETESESSION_CONTRACTID, nsAbAutoCompleteSessionConstructor },
    { "Address Book Address Collector", NS_ABADDRESSCOLLECTER_CID,
      NS_ABADDRESSCOLLECTER_CONTRACTID, nsAbAddressCollecterConstructor },
    { "The addbook URL Interface", NS_ADDBOOKURL_CID,
      NS_ADDBOOKURL_CONTRACTID, nsAddbookUrlConstructor },   
    { "The addbook Protocol Handler", NS_ADDBOOK_HANDLER_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "addbook", nsAddbookProtocolHandlerConstructor },
    { "add vCard content handler", NS_ADDRESSBOOK_CID, NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-addvcard", nsAddressBookConstructor },
    { "add vCard content handler", NS_ADDRESSBOOK_CID, NS_CONTENT_HANDLER_CONTRACTID_PREFIX"text/x-vcard", nsAddressBookConstructor },

    { "The directory factory service interface", NS_ABDIRFACTORYSERVICE_CID,
      NS_ABDIRFACTORYSERVICE_CONTRACTID, nsAbDirFactoryServiceConstructor },
    { "The MDB directory factory interface", NS_ABMDBDIRFACTORY_CID,
      NS_ABMDBDIRFACTORY_CONTRACTID, nsAbMDBDirFactoryConstructor },
#if defined(XP_WIN) && !defined(__MINGW32__)
    { "Address OUTLOOK Book Directory", NS_ABOUTLOOKDIRECTORY_CID,
      NS_ABOUTLOOKDIRECTORY_CONTRACTID, nsAbOutlookDirectoryConstructor },
    { "Address OUTLOOK Book Card", NS_ABOUTLOOKCARD_CID,
      NS_ABOUTLOOKCARD_CONTRACTID, nsAbOutlookCardConstructor },
    { "The outlook factory Interface", NS_ABOUTLOOKDIRFACTORY_CID,
      NS_ABOUTLOOKDIRFACTORY_CONTRACTID, nsAbOutlookDirFactoryConstructor },
#endif
    { "The addbook query arguments", NS_ABDIRECTORYQUERYARGUMENTS_CID,
      NS_ABDIRECTORYQUERYARGUMENTS_CONTRACTID, nsAbDirectoryQueryArgumentsConstructor },   
    { "The query boolean condition string", NS_BOOLEANCONDITIONSTRING_CID,
      NS_BOOLEANCONDITIONSTRING_CONTRACTID, nsAbBooleanConditionStringConstructor },   
    { "The query n-peer expression", NS_BOOLEANEXPRESSION_CID,
      NS_BOOLEANEXPRESSION_CONTRACTID, nsAbBooleanExpressionConstructor },
#if defined(MOZ_LDAP_XPCOM)
    { "Address LDAP Book Directory", NS_ABLDAPDIRECTORY_CID,
      NS_ABLDAPDIRECTORY_CONTRACTID, nsAbLDAPDirectoryConstructor },
    { "Address LDAP Book Card", NS_ABLDAPCARD_CID,
      NS_ABLDAPCARD_CONTRACTID, nsAbLDAPCardConstructor },
    { "Address LDAP factory Interface", NS_ABLDAPDIRFACTORY_CID,
      NS_ABLDAPDIRFACTORY_CONTRACTID, nsAbLDAPDirFactoryConstructor },
    { "Address LDAP Replication Service Interface", NS_ABLDAP_REPLICATIONSERVICE_CID,
      NS_ABLDAP_REPLICATIONSERVICE_CONTRACTID, nsAbLDAPReplicationServiceConstructor },
    { "Address LDAP Replication Query Interface", NS_ABLDAP_REPLICATIONQUERY_CID, 
      NS_ABLDAP_REPLICATIONQUERY_CONTRACTID, nsAbLDAPReplicationQueryConstructor },
    { "Address LDAP Replication Processor Interface", NS_ABLDAP_PROCESSREPLICATIONDATA_CID,
      NS_ABLDAP_PROCESSREPLICATIONDATA_CONTRACTID, nsAbLDAPProcessReplicationDataConstructor},
    { "Address LDAP ChangeLog Query Interface", NS_ABLDAP_CHANGELOGQUERY_CID,
      NS_ABLDAP_CHANGELOGQUERY_CONTRACTID, nsAbLDAPChangeLogQueryConstructor },
    { "Address LDAP ChangeLog Processor Interface", NS_ABLDAP_PROCESSCHANGELOGDATA_CID,
      NS_ABLDAP_PROCESSCHANGELOGDATA_CONTRACTID, nsAbLDAPProcessChangeLogDataConstructor },
    { "Address LDAP autocomplete factory Interface", NS_ABLDAPDIRFACTORY_CID,
      NS_ABLDAPACDIRFACTORY_CONTRACTID, nsAbLDAPDirFactoryConstructor },
    { "Address LDAP over SSL autocomplete factory Interface", NS_ABLDAPDIRFACTORY_CID,
      NS_ABLDAPSACDIRFACTORY_CONTRACTID, nsAbLDAPDirFactoryConstructor },
    { "Address book LDAP autocomplete formatter", NS_ABLDAPAUTOCOMPFORMATTER_CID,
      NS_ABLDAPAUTOCOMPFORMATTER_CONTRACTID,nsAbLDAPAutoCompFormatterConstructor },
#endif
    { "The directory query proxy interface", NS_ABDIRECTORYQUERYPROXY_CID,
      NS_ABDIRECTORYQUERYPROXY_CONTRACTID, nsAbDirectoryQueryProxyConstructor},
    { "addressbook view", NS_ABVIEW_CID, NS_ABVIEW_CONTRACTID, nsAbViewConstructor},
    { "vcard helper service", NS_MSGVCARDSERVICE_CID, NS_MSGVCARDSERVICE_CONTRACTID, nsMsgVCardServiceConstructor },

    ////////////////////////////////////////////////////////////////////////////////
    // bayesian spam filter components
    ////////////////////////////////////////////////////////////////////////////////
    { "Bayesian Filter", NS_BAYESIANFILTER_CID,
      NS_BAYESIANFILTER_CONTRACTID, nsBayesianFilterConstructor},

    ////////////////////////////////////////////////////////////////////////////////
    // compose components
    ////////////////////////////////////////////////////////////////////////////////
    { "Msg Compose", NS_MSGCOMPOSE_CID, 
      NS_MSGCOMPOSE_CONTRACTID,  nsMsgComposeConstructor },
    { "Msg Compose Service", NS_MSGCOMPOSESERVICE_CID, 
      NS_MSGCOMPOSESERVICE_CONTRACTID, nsMsgComposeServiceConstructor },
    { "Msg Compose Startup Handler", NS_MSGCOMPOSESERVICE_CID,
      NS_MSGCOMPOSESTARTUPHANDLER_CONTRACTID, nsMsgComposeServiceConstructor, nsMsgComposeService::RegisterProc,
      nsMsgComposeService::UnregisterProc },
    { "mailto content handler", NS_MSGCOMPOSECONTENTHANDLER_CID,
      NS_MSGCOMPOSECONTENTHANDLER_CONTRACTID, nsMsgComposeContentHandlerConstructor },
    { "Msg Compose Parameters", NS_MSGCOMPOSEPARAMS_CID,
      NS_MSGCOMPOSEPARAMS_CONTRACTID, nsMsgComposeParamsConstructor },
    { "Msg Compose Send Listener", NS_MSGCOMPOSESENDLISTENER_CID,
      NS_MSGCOMPOSESENDLISTENER_CONTRACTID, nsMsgComposeSendListenerConstructor },
    { "Msg Compose Progress Parameters", NS_MSGCOMPOSEPROGRESSPARAMS_CID,
      NS_MSGCOMPOSEPROGRESSPARAMS_CONTRACTID, nsMsgComposeProgressParamsConstructor },
    { "Msg Compose Fields", NS_MSGCOMPFIELDS_CID, 
      NS_MSGCOMPFIELDS_CONTRACTID, nsMsgCompFieldsConstructor },
    { "Msg Compose Attachment", NS_MSGATTACHMENT_CID, 
      NS_MSGATTACHMENT_CONTRACTID, nsMsgAttachmentConstructor },
    { "Msg Draft", NS_MSGDRAFT_CID, 
      NS_MSGDRAFT_CONTRACTID, nsMsgDraftConstructor },
    { "Msg Send", NS_MSGSEND_CID,
      NS_MSGSEND_CONTRACTID, nsMsgComposeAndSendConstructor },
    { "Msg Send Later", NS_MSGSENDLATER_CID,
      NS_MSGSENDLATER_CONTRACTID, nsMsgSendLaterConstructor },
    { "SMTP Service", NS_SMTPSERVICE_CID,
      NS_SMTPSERVICE_CONTRACTID, nsSmtpServiceConstructor },
    { "SMTP Service", NS_SMTPSERVICE_CID, 
      NS_MAILTOHANDLER_CONTRACTID, nsSmtpServiceConstructor },
    { "SMTP Server", NS_SMTPSERVER_CID,
      NS_SMTPSERVER_CONTRACTID, nsSmtpServerConstructor },
    { "SMTP URL", NS_SMTPURL_CID,
      NS_SMTPURL_CONTRACTID, nsSmtpUrlConstructor },
    { "MAILTO URL", NS_MAILTOURL_CID,
      NS_MAILTOURL_CONTRACTID, nsMailtoUrlConstructor },
    { "Msg Quote", NS_MSGQUOTE_CID,
      NS_MSGQUOTE_CONTRACTID, nsMsgQuoteConstructor },
    { "Msg Quote Listener", NS_MSGQUOTELISTENER_CID,
      NS_MSGQUOTELISTENER_CONTRACTID, nsMsgQuoteListenerConstructor },
    { "Msg Recipient Array", NS_MSGRECIPIENTARRAY_CID,
      NS_MSGRECIPIENTARRAY_CONTRACTID, nsMsgRecipientArrayConstructor },
    { "Compose string bundle", NS_MSG_COMPOSESTRINGSERVICE_CID,
      NS_MSG_COMPOSESTRINGSERVICE_CONTRACTID, nsComposeStringServiceConstructor },
    { "SMTP string bundle", NS_MSG_COMPOSESTRINGSERVICE_CID,
      NS_MSG_SMTPSTRINGSERVICE_CONTRACTID, nsComposeStringServiceConstructor },
    { "SMTP Datasource", NS_SMTPDATASOURCE_CID,
      NS_SMTPDATASOURCE_CONTRACTID, nsSmtpDataSourceConstructor },
    { "SMTP Delegate Factory", NS_SMTPDELEGATEFACTORY_CID,
      NS_SMTPDELEGATEFACTORY_CONTRACTID, nsSmtpDelegateFactoryConstructor },
    { "URL Fetcher", NS_URLFETCHER_CID,
      NS_URLFETCHER_CONTRACTID, nsURLFetcherConstructor },
    { "Msg Compose Utils", NS_MSGCOMPUTILS_CID,
      NS_MSGCOMPUTILS_CONTRACTID, nsMsgCompUtilsConstructor },
    
     ////////////////////////////////////////////////////////////////////////////////
    // imap components
    ////////////////////////////////////////////////////////////////////////////////
    { "IMAP URL", NS_IMAPURL_CID,
      nsnull, nsImapUrlConstructor },
    { "IMAP Protocol Channel", NS_IMAPPROTOCOL_CID,
      nsnull, nsImapProtocolConstructor },    
    { "IMAP Mock Channel", NS_IMAPMOCKCHANNEL_CID,
      nsnull, nsImapMockChannelConstructor },
    { "IMAP Host Session List", NS_IIMAPHOSTSESSIONLIST_CID,
      nsnull, nsIMAPHostSessionListConstructor },
    { "IMAP Incoming Server", NS_IMAPINCOMINGSERVER_CID,
      NS_IMAPINCOMINGSERVER_CONTRACTID, nsImapIncomingServerConstructor },
    { "Mail/News IMAP Resource Factory", NS_IMAPRESOURCE_CID,
      NS_RDF_RESOURCE_FACTORY_CONTRACTID_PREFIX "imap", 
      nsImapMailFolderConstructor },
    { "IMAP Service", NS_IMAPSERVICE_CID,
      "@mozilla.org/messenger/messageservice;1?type=imap-message",
      nsImapServiceConstructor },
    { "IMAP Service", NS_IMAPSERVICE_CID,
      "@mozilla.org/messenger/messageservice;1?type=imap",
      nsImapServiceConstructor },
    { "IMAP Service", NS_IMAPSERVICE_CID,
      NS_IMAPSERVICE_CONTRACTID,
      nsImapServiceConstructor },
    { "IMAP Protocol Handler", NS_IMAPSERVICE_CID,
      NS_NETWORK_PROTOCOL_CONTRACTID_PREFIX "imap", nsImapServiceConstructor},
    { "IMAP Protocol Handler", NS_IMAPSERVICE_CID,
      NS_IMAPPROTOCOLINFO_CONTRACTID, nsImapServiceConstructor },
    { "imap folder content handler", NS_IMAPSERVICE_CID,
      NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-imapfolder", nsImapServiceConstructor},

    ////////////////////////////////////////////////////////////////////////////////
    // local components
    ////////////////////////////////////////////////////////////////////////////////
    { "Mailbox URL", NS_MAILBOXURL_CID,
      NS_MAILBOXURL_CONTRACTID, nsMailboxUrlConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID1, nsMailboxServiceConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID2, nsMailboxServiceConstructor },

    { "Mailbox Service", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID3, nsMailboxServiceConstructor },

    { "Mailbox Protocol Handler", NS_MAILBOXSERVICE_CID,
      NS_MAILBOXSERVICE_CONTRACTID4, nsMailboxServiceConstructor },

    { "Mailbox Parser", NS_MAILBOXPARSER_CID,
      NS_MAILBOXPARSER_CONTRACTID, nsMsgMailboxParserConstructor },

    { "Pop3 URL", NS_POP3URL_CID,
      NS_POP3URL_CONTRACTID, nsPop3URLConstructor },

    { "Pop3 Service", NS_POP3SERVICE_CID,
      NS_POP3SERVICE_CONTRACTID1, nsPop3ServiceConstructor },

    { "POP Protocol Handler", NS_POP3SERVICE_CID,
      NS_POP3SERVICE_CONTRACTID2, nsPop3ServiceConstructor },

    { "None Service", NS_NONESERVICE_CID,
      NS_NONESERVICE_CONTRACTID, nsNoneServiceConstructor },
    { "pop3 Protocol Information", NS_POP3SERVICE_CID,
      NS_POP3PROTOCOLINFO_CONTRACTID, nsPop3ServiceConstructor },

    { "none Protocol Information", NS_NONESERVICE_CID,
      NS_NONEPROTOCOLINFO_CONTRACTID, nsNoneServiceConstructor },
    { "Local Mail Folder Resource Factory", NS_LOCALMAILFOLDERRESOURCE_CID,
      NS_LOCALMAILFOLDERRESOURCE_CONTRACTID, nsMsgLocalMailFolderConstructor },

    { "Pop3 Incoming Server", NS_POP3INCOMINGSERVER_CID,
      NS_POP3INCOMINGSERVER_CONTRACTID, nsPop3IncomingServerConstructor },

#ifdef HAVE_MOVEMAIL 
    { "Movemail Service", NS_MOVEMAILSERVICE_CID,
      NS_MOVEMAILSERVICE_CONTRACTID, nsMovemailServiceConstructor },
    { "movemail Protocol Information", NS_MOVEMAILSERVICE_CID,
      NS_MOVEMAILPROTOCOLINFO_CONTRACTID, nsMovemailServiceConstructor },
    { "Movemail Incoming Server", NS_MOVEMAILINCOMINGSERVER_CID,
      NS_MOVEMAILINCOMINGSERVER_CONTRACTID, nsMovemailIncomingServerConstructor },
#endif /* HAVE_MOVEMAIL */

    { "No Incoming Server", NS_NOINCOMINGSERVER_CID,
      NS_NOINCOMINGSERVER_CONTRACTID, nsNoIncomingServerConstructor },

    { "Parse MailMessage State", NS_PARSEMAILMSGSTATE_CID,
      NS_PARSEMAILMSGSTATE_CONTRACTID, nsParseMailMessageStateConstructor },

    { "Mailbox String Bundle Service", NS_MSG_LOCALSTRINGSERVICE_CID,
      NS_MSG_MAILBOXSTRINGSERVICE_CONTRACTID, nsLocalStringServiceConstructor },

    { "Pop String Bundle Service", NS_MSG_LOCALSTRINGSERVICE_CID,
      NS_MSG_POPSTRINGSERVICE_CONTRACTID, nsLocalStringServiceConstructor },
    
    { "RSS Service", NS_RSSSERVICE_CID,
      NS_RSSSERVICE_CONTRACTID, nsRssServiceConstructor },
    
    { "RSS Protocol Information", NS_RSSSERVICE_CID,
      NS_RSSPROTOCOLINFO_CONTRACTID, nsRssServiceConstructor },
        
    { "RSS Incoming Server", NS_RSSINCOMINGSERVER_CID,
      NS_RSSINCOMINGSERVER_CONTRACTID, nsRssIncomingServerConstructor },
    ////////////////////////////////////////////////////////////////////////////////
    // msgdb components
    ////////////////////////////////////////////////////////////////////////////////
    { "Msg DB Service", NS_MSGDB_SERVICE_CID, NS_MSGDB_SERVICE_CONTRACTID, nsMsgDBServiceConstructor },
    { "Mail DB", NS_MAILDB_CID, NS_MAILBOXDB_CONTRACTID, nsMailDatabaseConstructor },
    { "News DB", NS_NEWSDB_CID, NS_NEWSDB_CONTRACTID, nsNewsDatabaseConstructor },
    { "Imap DB", NS_IMAPDB_CID, NS_IMAPDB_CONTRACTID, nsImapMailDatabaseConstructor },
    { "Msg Retention Settings", NS_MSG_RETENTIONSETTINGS_CID,
      NS_MSG_RETENTIONSETTINGS_CONTRACTID, nsMsgRetentionSettingsConstructor },
    { "Msg Download Settings", NS_MSG_DOWNLOADSETTINGS_CID,
      NS_MSG_DOWNLOADSETTINGS_CONTRACTID, nsMsgDownloadSettingsConstructor },
    
    ////////////////////////////////////////////////////////////////////////////////
    // mime components
    ////////////////////////////////////////////////////////////////////////////////
    { "MimeObjectClassAccess", NS_MIME_OBJECT_CLASS_ACCESS_CID,
      nsnull, nsMimeObjectClassAccessConstructor },
    { "Mime Converter", NS_MIME_CONVERTER_CID,
      NS_MIME_CONVERTER_CONTRACTID, nsMimeConverterConstructor },
    { "Msg Header Parser", NS_MSGHEADERPARSER_CID, 
      NS_MAILNEWS_MIME_HEADER_PARSER_CONTRACTID, nsMsgHeaderParserConstructor },
    { "Mailnews Mime Stream Converter", NS_MAILNEWS_MIME_STREAM_CONVERTER_CID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID, nsStreamConverterConstructor },
    { "Mailnews Mime Stream Converter", NS_MAILNEWS_MIME_STREAM_CONVERTER_CID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID1, nsStreamConverterConstructor},
    { "Mailnews Mime Stream Converter", NS_MAILNEWS_MIME_STREAM_CONVERTER_CID,
      NS_MAILNEWS_MIME_STREAM_CONVERTER_CONTRACTID2, nsStreamConverterConstructor },
    { "Mime Headers", NS_IMIMEHEADERS_CID,
      NS_IMIMEHEADERS_CONTRACTID, nsMimeHeadersConstructor },

    ////////////////////////////////////////////////////////////////////////////////
    // mime emitter components
    ////////////////////////////////////////////////////////////////////////////////
    { "HTML MIME Emitter", NS_HTML_MIME_EMITTER_CID,
      NS_HTML_MIME_EMITTER_CONTRACTID, nsMimeHtmlDisplayEmitterConstructor, RegisterMimeEmitter },
    { "XML MIME Emitter", NS_XML_MIME_EMITTER_CID,
      NS_XML_MIME_EMITTER_CONTRACTID, nsMimeXmlEmitterConstructor, RegisterMimeEmitter },
    { "PLAIN MIME Emitter", NS_PLAIN_MIME_EMITTER_CID,
      NS_PLAIN_MIME_EMITTER_CONTRACTID, nsMimePlainEmitterConstructor, RegisterMimeEmitter },
    { "RAW MIME Emitter", NS_RAW_MIME_EMITTER_CID,
      NS_RAW_MIME_EMITTER_CONTRACTID, nsMimeRawEmitterConstructor, RegisterMimeEmitter },

    ////////////////////////////////////////////////////////////////////////////////
    // news components
    ////////////////////////////////////////////////////////////////////////////////
    { "NNTP URL", NS_NNTPURL_CID,
      NS_NNTPURL_CONTRACTID, nsNntpUrlConstructor },
    { "NNTP Service", NS_NNTPSERVICE_CID,
      NS_NNTPSERVICE_CONTRACTID, nsNntpServiceConstructor },
    { "News Startup Handler", NS_NNTPSERVICE_CID,
      NS_NEWSSTARTUPHANDLER_CONTRACTID, nsNntpServiceConstructor,
      nsNntpService::RegisterProc, nsNntpService::UnregisterProc },
    { "NNTP Protocol Info", NS_NNTPSERVICE_CID,
      NS_NNTPPROTOCOLINFO_CONTRACTID, nsNntpServiceConstructor },
    { "NNTP Message Service",NS_NNTPSERVICE_CID,
      NS_NNTPMESSAGESERVICE_CONTRACTID, nsNntpServiceConstructor },
    { "News Message Service", NS_NNTPSERVICE_CID,
      NS_NEWSMESSAGESERVICE_CONTRACTID, nsNntpServiceConstructor },
    { "News Protocol Handler", NS_NNTPSERVICE_CID,
      NS_NEWSPROTOCOLHANDLER_CONTRACTID, nsNntpServiceConstructor },
    { "Secure News Protocol Handler", NS_NNTPSERVICE_CID,
      NS_SNEWSPROTOCOLHANDLER_CONTRACTID, nsNntpServiceConstructor },
    { "NNTP Protocol Handler", NS_NNTPSERVICE_CID,
      NS_NNTPPROTOCOLHANDLER_CONTRACTID, nsNntpServiceConstructor },
    { "newsgroup content handler",NS_NNTPSERVICE_CID,
      NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-newsgroup", nsNntpServiceConstructor },
    { "newsgroup listids content handler", NS_NNTPSERVICE_CID,
      NS_CONTENT_HANDLER_CONTRACTID_PREFIX"x-application-newsgroup-listids", nsNntpServiceConstructor },
    { "News Folder Resource", NS_NEWSFOLDERRESOURCE_CID,
      NS_NEWSFOLDERRESOURCE_CONTRACTID, nsMsgNewsFolderConstructor },
    { "NNTP Incoming Servier", NS_NNTPINCOMINGSERVER_CID,
      NS_NNTPINCOMINGSERVER_CONTRACTID, nsNntpIncomingServerConstructor },
    { "NNTP Newsgroup Post", NS_NNTPNEWSGROUPPOST_CID,
      NS_NNTPNEWSGROUPPOST_CONTRACTID, nsNNTPNewsgroupPostConstructor },
    { "NNTP Newsgroup List", NS_NNTPNEWSGROUPLIST_CID,
      NS_NNTPNEWSGROUPLIST_CONTRACTID, nsNNTPNewsgroupListConstructor },
    { "NNTP Article List", NS_NNTPARTICLELIST_CID,
      NS_NNTPARTICLELIST_CONTRACTID, nsNNTPArticleListConstructor },
    { "News download dialog args", NS_NEWSDOWNLOADDIALOGARGS_CID,
      NS_NEWSDOWNLOADDIALOGARGS_CONTRACTID, nsNewsDownloadDialogArgsConstructor },
    
    ////////////////////////////////////////////////////////////////////////////////
    // mail view components
    ////////////////////////////////////////////////////////////////////////////////
    { "Default Mail List View", NS_MSGMAILVIEWLIST_CID,
       NS_MSGMAILVIEWLIST_CONTRACTID, nsMsgMailViewListConstructor },
    
    ////////////////////////////////////////////////////////////////////////////////
    // mdn  components
    ////////////////////////////////////////////////////////////////////////////////
    { "Msg Mdn Generator", NS_MSGMDNGENERATOR_CID,
      NS_MSGMDNGENERATOR_CONTRACTID, nsMsgMdnGeneratorConstructor },
    
    ////////////////////////////////////////////////////////////////////////////////
    // mdn  components
    ////////////////////////////////////////////////////////////////////////////////
    { "MIME VCard Handler", NS_VCARD_CONTENT_TYPE_HANDLER_CID, "@mozilla.org/mimecth;1?type=text/x-vcard",
       nsVCardMimeContentTypeHandlerConstructor, }
};

PR_STATIC_CALLBACK(void) nsMailModuleDtor(nsIModule* self)
{
    nsAddrDatabase::CleanupCache();
    nsMsgDatabase::CleanupCache();
}

NS_IMPL_NSGETMODULE_WITH_DTOR(nsMailModule, gComponents, nsMailModuleDtor)
