#ifndef mozSqlService_h
#define mozSqlService_h

#include "nsHashtable.h"
#include "nsIRDFDataSource.h"
#include "nsIRDFRemoteDataSource.h"
#include "nsIRDFService.h"
#include "nsIRDFContainerUtils.h"
#include "mozISqlService.h"

#define MOZ_SQLSERVICE_CLASSNAME "SQL service"
#define MOZ_SQLSERVICE_CID \
{0x1ceb35b7, 0xdaa8, 0x4ce4, {0xac, 0x67, 0x12, 0x5f, 0xb1, 0x7c, 0xb0, 0x19}}
#define MOZ_SQLSERVICE_CONTRACTID "@mozilla.org/sql/service;1"
#define MOZ_SQLDATASOURCE_CONTRACTID "@mozilla.org/rdf/datasource;1?name=sql"

class mozSqlService : public mozISqlService,
                      public nsIRDFDataSource,
                      public nsIRDFRemoteDataSource
{
  public:
    mozSqlService();
    virtual ~mozSqlService();
    nsresult Init();

    NS_DECL_ISUPPORTS
    NS_DECL_MOZISQLSERVICE
    NS_DECL_NSIRDFDATASOURCE
    NS_DECL_NSIRDFREMOTEDATASOURCE

  protected:
    nsresult EnsureAliasesContainer();

  private:
    static nsIRDFService*               gRDFService;
    static nsIRDFContainerUtils*        gRDFContainerUtils;

    static nsIRDFResource*              kSQL_AliasesRoot;
    static nsIRDFResource*              kSQL_Name;
    static nsIRDFResource*              kSQL_Type;
    static nsIRDFResource*              kSQL_Hostname;
    static nsIRDFResource*              kSQL_Port;
    static nsIRDFResource*              kSQL_Database;
	
    nsString                            mErrorMessage;
    nsCOMPtr<nsIRDFDataSource>          mInner;
    nsCOMPtr<nsIRDFContainer>           mAliasesContainer;
    nsSupportsHashtable*                mConnectionCache;

};

#endif /* mozSqlService_h */
