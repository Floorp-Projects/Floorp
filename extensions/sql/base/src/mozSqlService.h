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
    static nsIRDFResource*              kSQL_Priority;
	
    nsString                            mErrorMessage;
    nsCOMPtr<nsIRDFDataSource>          mInner;
    nsCOMPtr<nsIRDFContainer>           mAliasesContainer;
    nsSupportsHashtable*                mConnectionCache;

};

#endif /* mozSqlService_h */
