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
  NS_INIT_ISUPPORTS();
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

  gRDFService->GetResource("SQL:AliasesRoot", &kSQL_AliasesRoot);
  gRDFService->GetResource(SQL_NAMESPACE_URI "name", &kSQL_Name);
  gRDFService->GetResource(SQL_NAMESPACE_URI "type", &kSQL_Type);
  gRDFService->GetResource(SQL_NAMESPACE_URI "hostname", &kSQL_Hostname);
  gRDFService->GetResource(SQL_NAMESPACE_URI "port", &kSQL_Port);
  gRDFService->GetResource(SQL_NAMESPACE_URI "database", &kSQL_Database);

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
    nsCOMPtr<nsIAtom> prefix = getter_AddRefs(NS_NewAtom("SQL"));
    sink->AddNameSpace(prefix, NS_ConvertASCIItoUCS2(SQL_NAMESPACE_URI));
  }

  return gRDFService->RegisterDataSource(this, PR_FALSE);
}

NS_IMETHODIMP
mozSqlService::AddAlias(const nsACString& aURI,
                        const nsAString& aName,
                        const nsAString& aType,
                        const nsAString& aHostname,
                        PRInt32 aPort,
                        const nsAString& aDatabase)
{
  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetResource(PromiseFlatCString(aURI).get(), getter_AddRefs(resource));

  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;

  gRDFService->GetLiteral(PromiseFlatString(aName).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(resource, kSQL_Name, rdfLiteral, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aType).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(resource, kSQL_Type, rdfLiteral, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aHostname).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(resource, kSQL_Hostname, rdfLiteral, PR_TRUE);

  gRDFService->GetIntLiteral(aPort, getter_AddRefs(rdfInt));
  mInner->Assert(resource, kSQL_Port, rdfInt, PR_TRUE);

  gRDFService->GetLiteral(PromiseFlatString(aDatabase).get(), getter_AddRefs(rdfLiteral));
  mInner->Assert(resource, kSQL_Database, rdfLiteral, PR_TRUE);

  nsresult rv = EnsureAliasesContainer();
  if (NS_FAILED(rv))
    return rv;
  mAliasesContainer->AppendElement(resource);

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::HasAlias(const nsACString& aURI, PRBool* _retval)
{
  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetResource(PromiseFlatCString(aURI).get(), getter_AddRefs(resource));

  nsresult rv = EnsureAliasesContainer();
  if (NS_FAILED(rv))
    return rv;

  PRInt32 aliasIndex;
  mAliasesContainer->IndexOf(resource, &aliasIndex);

  *_retval = aliasIndex != -1;

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetAlias(const nsACString& aURI,
                        nsAString& aName,
                        nsAString& aType,
                        nsAString& aHostname,
                        PRInt32* aPort,
                        nsAString& aDatabase)
{
  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetResource(PromiseFlatCString(aURI).get(), getter_AddRefs(resource));

  nsCOMPtr<nsIRDFNode> rdfNode;
  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;
  const PRUnichar* value;
  
  mInner->GetTarget(resource, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aName.Assign(value);
  }

  mInner->GetTarget(resource, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aType.Assign(value);
  }

  mInner->GetTarget(resource, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aHostname.Assign(value);
  }

  mInner->GetTarget(resource, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfInt = do_QueryInterface(rdfNode);
    rdfInt->GetValue(aPort);
  }

  mInner->GetTarget(resource, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  if (rdfNode) {
    rdfLiteral = do_QueryInterface(rdfNode);
    rdfLiteral->GetValueConst(&value);
    aDatabase.Assign(value);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::UpdateAlias(const nsACString& aURI,
                           const nsAString& aName,
                           const nsAString& aType,
                           const nsAString& aHostname,
                           PRInt32 aPort,
                           const nsAString& aDatabase)
{
  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetResource(PromiseFlatCString(aURI).get(), getter_AddRefs(resource));

  nsCOMPtr<nsIRDFNode> rdfNode;
  nsCOMPtr<nsIRDFLiteral> rdfLiteral;
  nsCOMPtr<nsIRDFInt> rdfInt;

  mInner->GetTarget(resource, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aName).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(resource, kSQL_Name, rdfNode, rdfLiteral);

  mInner->GetTarget(resource, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aType).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(resource, kSQL_Type, rdfNode, rdfLiteral);

  mInner->GetTarget(resource, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aHostname).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(resource, kSQL_Hostname, rdfNode, rdfLiteral);

  mInner->GetTarget(resource, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetIntLiteral(aPort, getter_AddRefs(rdfInt));
  mInner->Change(resource, kSQL_Port, rdfNode, rdfInt);

  mInner->GetTarget(resource, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  gRDFService->GetLiteral(PromiseFlatString(aDatabase).get(), getter_AddRefs(rdfLiteral));
  mInner->Change(resource, kSQL_Database, rdfNode, rdfLiteral);

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::RemoveAlias(const nsACString &aURI)
{
  nsCOMPtr<nsIRDFResource> resource;
  gRDFService->GetResource(PromiseFlatCString(aURI).get(), getter_AddRefs(resource));

  nsCOMPtr<nsIRDFNode> rdfNode;

  mInner->GetTarget(resource, kSQL_Name, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(resource, kSQL_Name, rdfNode);

  mInner->GetTarget(resource, kSQL_Type, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(resource, kSQL_Type, rdfNode);

  mInner->GetTarget(resource, kSQL_Hostname, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(resource, kSQL_Hostname, rdfNode);

  mInner->GetTarget(resource, kSQL_Port, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(resource, kSQL_Port, rdfNode);

  mInner->GetTarget(resource, kSQL_Database, PR_TRUE, getter_AddRefs(rdfNode));
  mInner->Unassert(resource, kSQL_Database, rdfNode);

  nsresult rv = EnsureAliasesContainer();
  if (NS_FAILED(rv))
    return rv;
  mAliasesContainer->RemoveElement(resource, PR_TRUE);

  Flush();

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetConnection(const nsACString &aURI, mozISqlConnection **_retval)
{
  nsCStringKey key(aURI);
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
    nsresult rv = GetNewConnection(aURI, getter_AddRefs(conn));
    if (NS_FAILED(rv))
      return rv;

    weakRef = do_GetWeakReference(conn);

    if (! mConnectionCache)
      mConnectionCache = new nsSupportsHashtable(16);
    mConnectionCache->Put(&key, weakRef);

    NS_ADDREF(*_retval = conn);
  }

  return NS_OK;
}

NS_IMETHODIMP
mozSqlService::GetNewConnection(const nsACString &aURI, mozISqlConnection **_retval)
{
  PRBool hasAlias;
  HasAlias(aURI, &hasAlias);
  if (!hasAlias)
    return NS_ERROR_FAILURE;

  nsresult rv;

  nsAutoString name;
  nsAutoString type;
  nsAutoString hostname;
  PRInt32 port;
  nsAutoString database;
  GetAlias(aURI, name, type, hostname, &port, database);

  nsCAutoString contractID(
    NS_LITERAL_CSTRING("@mozilla.org/sql/connection;1?type=") +
    NS_ConvertUCS2toUTF8(type));

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
