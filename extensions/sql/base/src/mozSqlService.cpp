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
 * The Initial Developer of the Original Code is Jan Varga
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsReadableUtils.h"
#include "nsXPIDLString.h"
#include "nsCRT.h"
#include "nsIAtom.h"
#include "nsISupportsUtils.h"
#include "nsIServiceManager.h"
#include "rdf.h"
#include "nsRDFCID.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNetUtil.h"
#include "nsIRDFXMLSink.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"
#include "mozSqlService.h"
#include "mozSqlConnection.h"

#define SQL_NAMESPACE_URI "http://www.mozilla.org/SQL-rdf#"

static NS_DEFINE_CID(kRDFServiceCID,            NS_RDFSERVICE_CID);
static NS_DEFINE_CID(kRDFContainerUtilsCID,     NS_RDFCONTAINERUTILS_CID);

nsIRDFService*          mozSqlService::gRDFService;
nsIRDFContainerUtils*   mozSqlService::gRDFContainerUtils;

nsIRDFResource*         mozSqlService::kSQL_AliasesRoot;
nsIRDFResource*         mozSqlService::kSQL_Name;
nsIRDFResource*         mozSqlService::kSQL_Type;
nsIRDFResource*         mozSqlService::kSQL_Hostname;
nsIRDFResource*         mozSqlService::kSQL_Port;
nsIRDFResource*         mozSqlService::kSQL_Database;


mozSqlService::mozSqlService()
  : mConnectionCache(nsnull)
{
}

mozSqlService::~mozSqlService()
{
  gRDFService->UnregisterDataSource(this);

  delete mConnectionCache;

  NS_IF_RELEASE(kSQL_AliasesRoot);
  NS_IF_RELEASE(kSQL_Name);
  NS_IF_RELEASE(kSQL_Type);
  NS_IF_RELEASE(kSQL_Hostname);
  NS_IF_RELEASE(kSQL_Port);
  NS_IF_RELEASE(kSQL_Database);

  nsServiceManager::ReleaseService(kRDFContainerUtilsCID, gRDFContainerUtils);
  gRDFContainerUtils = nsnull;

  nsServiceManager::ReleaseService(kRDFServiceCID, gRDFService);
  gRDFService = nsnull;
}

NS_IMPL_ISUPPORTS3(mozSqlService,
                   mozISqlService,
                   nsIRDFDataSource,
                   nsIRDFRemoteDataSource);

NS_IMETHODIMP
mozSqlService::GetErrorMessage(nsAString& aErrorMessage)
{
  aErrorMessage = mErrorMessage;
  return NS_OK;
}

nsresult
mozSqlService::Init()
{
  nsresult rv;

  rv = nsServiceManager::GetService(kRDFServiceCID, NS_GET_IID(nsIRDFService), 
                                    (nsISupports**) &gRDFService);
  if (NS_FAILED(rv)) return rv;

  rv = nsServiceManager::GetService(kRDFContainerUtilsCID, NS_GET_IID(nsIRDFContainerUtils),
                                    (nsISupports**) &gRDFContainerUtils);

  if (NS_FAILED(rv)) return rv;

  gRDFService->GetResource(NS_LITERAL_CSTRING("SQL:AliasesRoot"),
                           &kSQL_AliasesRoot);
  gRDFService->GetResource(NS_LITERAL_CSTRING(SQL_NAMESPACE_URI "name"),
                           &kSQL_Name);
  gRDFService->GetResource(NS_LITERAL_CSTRING(SQL_NAMESPACE_URI "type"),
                           &kSQL_Type);
  gRDFService->GetResource(NS_LITERAL_CSTRING(SQL_NAMESPACE_URI "hostname"),
                           &kSQL_Hostname);
  gRDFService->GetResource(NS_LITERAL_CSTRING(SQL_NAMESPACE_URI "port"),
                           &kSQL_Port);
  gRDFService->GetResource(NS_LITERAL_CSTRING(SQL_NAMESPACE_URI "database"),
                           &kSQL_Database);

  nsCOMPtr<nsIFile> file;
  rv = NS_GetSpecialDirectory(NS_APP_USER_PROFILE_50_DIR, getter_AddRefs(file));
  if (NS_FAILED(rv)) return rv;
  rv = file->AppendNative(NS_LITERAL_CSTRING("sql.rdf"));
  if (NS_FAILED(rv)) return rv;

  nsCAutoString sql;
  NS_GetURLSpecFromFile(file, sql);

  rv = gRDFService->GetDataSourceBlocking(sql.get(), getter_AddRefs(mInner));
  if (NS_FAILED(rv)) return rv;
  nsCOMPtr<nsIRDFXMLSink> sink = do_QueryInterface(mInner);
  if (sink) {
    nsCOMPtr<nsIAtom> prefix = do_GetAtom("SQL");
    sink->AddNameSpace(prefix, NS_ConvertASCIItoUCS2(SQL_NAMESPACE_URI));
  }

  return gRDFService->RegisterDataSource(this, PR_FALSE);
}

NS_IMETHODIMP
mozSqlService::AddAlias(const nsAString& aName,
                        const nsAString& aType,
                        const nsAString& aHostname,
                        PRInt32 aPort,
                        const nsAString& aDatabase,
                        nsIRDFResource** aResult)
{
  nsCOMPtr<nsIRDFResource> alias;
  gRDFService->GetAnonymousResource(getter_AddRefs(alias));

  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;

  gRDFService->GetLiteral(PromiseFlatString(aName).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(alias, kSQL_Name, rdfLiteral, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aType).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(alias, kSQL_Type, rdfLiteral, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aHostname).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(alias, kSQL_Hostname, rdfLiteral, PR_TRUE);

  gRDFService->GetIntLiteral(aPort, getter_AddRefs(rdfInt));
  mInner->Assert(alias, kSQL_Port, rdfInt, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aDatabase).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(alias, kSQL_Database, rdfLiteral, PR_TRUE);

  nsresult rv = EnsureAliasesContainer();
  if (NS_FAILED(rv))
    return rv;
  mAliasesContainer->AppendElement(alias);

  Flush();

  NS_ADDREF(*aResult = alias);

  return NS_OK;
}


NS_IMETHODIMP
mozSqlService::FetchAlias(nsIRDFResource* aAlias,
                          nsAString& aName,
                          nsAString& aType,
                          nsAString& aHostname,
                          PRInt32* aPort,
                          nsAString& aDatabase)
{
  nsCOMPtr<nsIRDFNode> rdfNode;
  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;
  const PRUnichar* value;
  
  mInner->GetTarget(aAlias, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aName.Assign(value);
  }

  mInner->GetTarget(aAlias, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aType.Assign(value);
  }

  mInner->GetTarget(aAlias, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aHostname.Assign(value);
  }

  mInner->GetTarget(aAlias, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfInt = do_QueryInterface(rdfNode);
    rdfInt->GetValue(aPort);
  }

  mInner->GetTarget(aAlias, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aDatabase.Assign(value);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::UpdateAlias(nsIRDFResource* aAlias,
                           const nsAString& aName,
                           const nsAString& aType,
                           const nsAString& aHostname,
                           PRInt32 aPort,
                           const nsAString& aDatabase)
{
  nsCOMPtr<nsIRDFNode> rdfNode;
  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;

  mInner->GetTarget(aAlias, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aName).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(aAlias, kSQL_Name, rdfNode, rdfLiteral);

  mInner->GetTarget(aAlias, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aType).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(aAlias, kSQL_Type, rdfNode, rdfLiteral);

  mInner->GetTarget(aAlias, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aHostname).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(aAlias, kSQL_Hostname, rdfNode, rdfLiteral);

  mInner->GetTarget(aAlias, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetIntLiteral(aPort, getter_AddRefs(rdfInt));
  mInner->Change(aAlias, kSQL_Port, rdfNode, rdfInt);

  mInner->GetTarget(aAlias, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aDatabase).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(aAlias, kSQL_Database, rdfNode, rdfLiteral);

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::RemoveAlias(nsIRDFResource* aAlias)
{
  nsCOMPtr<nsIRDFNode> rdfNode;

  mInner->GetTarget(aAlias, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(aAlias, kSQL_Name, rdfNode);

  mInner->GetTarget(aAlias, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(aAlias, kSQL_Type, rdfNode);

  mInner->GetTarget(aAlias, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(aAlias, kSQL_Hostname, rdfNode);

  mInner->GetTarget(aAlias, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(aAlias, kSQL_Port, rdfNode);

  mInner->GetTarget(aAlias, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(aAlias, kSQL_Database, rdfNode);

  nsresult rv = EnsureAliasesContainer();
  if (NS_FAILED(rv))
    return rv;
  mAliasesContainer->RemoveElement(aAlias, PR_TRUE);

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetAlias(const nsAString& aName, nsIRDFResource** _retval)
{
  nsCOMPtr<nsIRDFLiteral> nameLiteral;
  nsresult rv = gRDFService->GetLiteral(PromiseFlatString(aName).get(),
                                        getter_AddRefs(nameLiteral));
  if (NS_FAILED(rv))
    return rv;

  nsCOMPtr<nsIRDFResource> alias;
  rv = mInner->GetSource(kSQL_Name, nameLiteral, PR_TRUE, getter_AddRefs(alias));
  if (NS_FAILED(rv))
    return rv;

  NS_IF_ADDREF(*_retval = alias);

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetConnection(nsIRDFResource* aAlias, mozISqlConnection **_retval)
{
  nsISupportsKey key(aAlias);
  nsCOMPtr<nsIWeakReference> weakRef;
  nsCOMPtr<mozISqlConnection> conn;

  if (mConnectionCache) {
    weakRef = getter_AddRefs(NS_STATIC_CAST(nsIWeakReference*, mConnectionCache->Get(&key)));
    if (weakRef) {
      conn = do_QueryReferent(weakRef);
      if (conn)
        NS_ADDREF(*_retval = conn);
    }
  }

  if (! *_retval) {
    nsresult rv = GetNewConnection(aAlias, getter_AddRefs(conn));
    if (NS_FAILED(rv))
      return rv;

    weakRef = do_GetWeakReference(conn);

    if (! mConnectionCache) {
      mConnectionCache = new nsSupportsHashtable(16);
      if (! mConnectionCache)
        return NS_ERROR_OUT_OF_MEMORY;
    }
    mConnectionCache->Put(&key, weakRef);

    NS_ADDREF(*_retval = conn);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetNewConnection(nsIRDFResource* aAlias, mozISqlConnection **_retval)
{
  nsAutoString name;
  nsAutoString type;
  nsAutoString hostname;
  PRInt32 port;
  nsAutoString database;
  nsresult rv = FetchAlias(aAlias, name, type, hostname, &port, database);
  if (NS_FAILED(rv))
    return rv;

  nsCAutoString contractID("@mozilla.org/sql/connection;1?type=");
  AppendUTF16toUTF8(type, contractID);

  nsCOMPtr<mozISqlConnection> conn = do_CreateInstance(contractID.get());
  if (! conn)
    return NS_ERROR_FAILURE;

  nsCOMPtr<nsIWindowWatcher> watcher(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
  nsCOMPtr<nsIPrompt> prompter;
  watcher->GetNewPrompter(0, getter_AddRefs(prompter));

  PRBool retval;
  do {
    nsXPIDLString username;
    nsXPIDLString password;
    prompter->PromptUsernameAndPassword(
      nsnull, // in wstring dialogTitle
      nsnull, // in wstring text
      getter_Copies(username),
      getter_Copies(password),
      nsnull, // in wstring checkMsg
      nsnull, // inout boolean checkValue
      &retval
    );

    if (retval) {
      rv = conn->Init(hostname, port, database, username, password);
      if (NS_FAILED(rv)) {
        conn->GetErrorMessage(mErrorMessage);
        prompter->Alert(nsnull, mErrorMessage.get());
      }
    }
  } while(retval && NS_FAILED(rv));

  NS_IF_ADDREF(*_retval = conn);

  return rv;
}

NS_IMETHODIMP
mozSqlService::GetURI(char** aURI)
{
  if (!aURI)
    return NS_ERROR_NULL_POINTER;
  
  *aURI = nsCRT::strdup("rdf:sql");
  if (!(*aURI))
    return NS_ERROR_OUT_OF_MEMORY;
  
  return NS_OK;
}

NS_IMETHODIMP 
mozSqlService::GetSource(nsIRDFResource* aProperty,
                         nsIRDFNode* aTarget,
                         PRBool aTruthValue,
                         nsIRDFResource** aSource) 
{
  return mInner->GetSource(aProperty, aTarget, aTruthValue, aSource);
}
   
NS_IMETHODIMP
mozSqlService::GetSources(nsIRDFResource* aProperty,
                          nsIRDFNode* aTarget,
                          PRBool aTruthValue,
                          nsISimpleEnumerator** aSources) {
  return mInner->GetSources(aProperty, aTarget, aTruthValue, aSources);
}
   
NS_IMETHODIMP
mozSqlService::GetTarget(nsIRDFResource* aSource,
                         nsIRDFResource* aProperty,
                         PRBool aTruthValue,
                         nsIRDFNode** aTarget) {
  return mInner->GetTarget(aSource, aProperty, aTruthValue, aTarget);
}
   
NS_IMETHODIMP
mozSqlService::GetTargets(nsIRDFResource* aSource,
                          nsIRDFResource* aProperty,
                          PRBool aTruthValue,
                          nsISimpleEnumerator** aTargets) {
  return mInner->GetTargets(aSource, aProperty, aTruthValue, aTargets);
}
   
NS_IMETHODIMP 
mozSqlService::Assert(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aTarget,
                      PRBool aTruthValue) 
{
  return mInner->Assert(aSource, aProperty, aTarget, aTruthValue);
}
   
NS_IMETHODIMP 
mozSqlService::Unassert(nsIRDFResource* aSource,
                        nsIRDFResource* aProperty,
                        nsIRDFNode* aTarget) 
{
  return mInner->Unassert(aSource, aProperty, aTarget);
}
   
NS_IMETHODIMP 
mozSqlService::Change(nsIRDFResource* aSource,
                      nsIRDFResource* aProperty,
                      nsIRDFNode* aOldTarget,
                      nsIRDFNode* aNewTarget) 
{
  return mInner->Change(aSource, aProperty, aOldTarget, aNewTarget);
}
   
NS_IMETHODIMP 
mozSqlService::Move(nsIRDFResource* aOldSource,
                    nsIRDFResource* aNewSource,
                    nsIRDFResource* aProperty,
                    nsIRDFNode* aTarget) 
{
  return mInner->Move(aOldSource, aNewSource, aProperty, aTarget);
}
   
NS_IMETHODIMP 
mozSqlService::HasAssertion(nsIRDFResource* aSource,
                            nsIRDFResource* aProperty,
                            nsIRDFNode* aTarget,
                            PRBool aTruthValue,
                            PRBool* hasAssertion) 
{
  return mInner->HasAssertion(aSource, aProperty, aTarget, aTruthValue, hasAssertion);
}
   
NS_IMETHODIMP 
mozSqlService::AddObserver(nsIRDFObserver* aObserver) 
{
  return mInner->AddObserver(aObserver);
}
   
NS_IMETHODIMP 
mozSqlService::RemoveObserver(nsIRDFObserver* aObserver) 
{
  return mInner->RemoveObserver(aObserver);
}
   
NS_IMETHODIMP 
mozSqlService::HasArcIn(nsIRDFNode* aNode,
                       nsIRDFResource* aArc,
                       PRBool* _retval) 
{
  return mInner->HasArcIn(aNode, aArc, _retval);
}

NS_IMETHODIMP 
mozSqlService::HasArcOut(nsIRDFResource* aSource,
                         nsIRDFResource* aArc,
                         PRBool* _retval) 
{
  return mInner->HasArcOut(aSource, aArc, _retval);
}

NS_IMETHODIMP 
mozSqlService::ArcLabelsIn(nsIRDFNode* aNode,
                           nsISimpleEnumerator** aLabels) 
{
  return mInner->ArcLabelsIn(aNode, aLabels);
}

NS_IMETHODIMP 
mozSqlService::ArcLabelsOut(nsIRDFResource* aSource,
                            nsISimpleEnumerator** aLabels) 
{
  return mInner->ArcLabelsIn(aSource, aLabels);
}

NS_IMETHODIMP 
mozSqlService::GetAllResources(nsISimpleEnumerator** aResult) 
{
  return mInner->GetAllResources(aResult);
}

NS_IMETHODIMP 
mozSqlService::GetAllCmds(nsIRDFResource* aSource,
                          nsISimpleEnumerator** aCommands) 
{
  return mInner->GetAllCmds(aSource, aCommands);
}

NS_IMETHODIMP 
mozSqlService::IsCommandEnabled(nsISupportsArray* aSources,
                                nsIRDFResource* aCommand,
                                nsISupportsArray* aArguments,
                                PRBool* aResult) 
{
  return mInner->IsCommandEnabled(aSources, aCommand, aArguments, aResult);
}

NS_IMETHODIMP 
mozSqlService::DoCommand(nsISupportsArray* aSources,
                         nsIRDFResource* aCommand,
                         nsISupportsArray* aArguments) 
{
  return mInner->DoCommand(aSources, aCommand, aArguments);
}

NS_IMETHODIMP 
mozSqlService::BeginUpdateBatch()
{
  return mInner->BeginUpdateBatch();
}

NS_IMETHODIMP 
mozSqlService::EndUpdateBatch()
{
  return mInner->EndUpdateBatch();
}


// nsIRDFRemoteDataSource
NS_IMETHODIMP
mozSqlService::GetLoaded(PRBool* aResult)
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->GetLoaded(aResult);
}

NS_IMETHODIMP
mozSqlService::Init(const char* aURI)
{
  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::Refresh(PRBool aBlocking)
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->Refresh(aBlocking);
}

NS_IMETHODIMP
mozSqlService::Flush()
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->Flush();
}

NS_IMETHODIMP
mozSqlService::FlushTo(const char *aURI)
{
  nsCOMPtr<nsIRDFRemoteDataSource> remote(do_QueryInterface(mInner));
  return remote->FlushTo(aURI);
}


nsresult
mozSqlService::EnsureAliasesContainer()
{
  if (! mAliasesContainer) {
    PRBool isContainer;
    nsresult rv = gRDFContainerUtils->IsContainer(mInner, kSQL_AliasesRoot, &isContainer);
    if (NS_FAILED(rv)) return rv;

    if (!isContainer) {
      rv = gRDFContainerUtils->MakeSeq(mInner, kSQL_AliasesRoot, getter_AddRefs(mAliasesContainer));
      if (NS_FAILED(rv)) return rv;
    }
    else {
      mAliasesContainer = do_CreateInstance(NS_RDF_CONTRACTID "/container;1", &rv);
      if (NS_FAILED(rv)) return rv;
      rv = mAliasesContainer->Init(mInner, kSQL_AliasesRoot);
      if (NS_FAILED(rv)) return rv;
    }
  }

  return NS_OK;
}
