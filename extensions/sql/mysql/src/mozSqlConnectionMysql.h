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
 * The Initial Developer of the Original Code is Neil Deakin
 * Portions created by the Initial Developer are Copyright (C) 2004
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

#ifndef mozSqlConnectionMysql_h
#define mozSqlConnectionMysql_h                                                  

#include "mysql.h"

#include "mozSqlConnection.h"
#include "mozISqlConnectionMysql.h"

#define MOZ_SQLCONNECTIONMYSQL_CLASSNAME "Mysql SQL Connection"

#define MOZ_SQLCONNECTIONMYSQL_CID \
{0x582dcad4, 0xc789, 0x11d8, {0x89, 0xf4, 0xd0, 0x5c, 0x55, 0x80, 0x13, 0xcc }}

#define MOZ_SQLCONNECTIONMYSQL_CONTRACTID "@mozilla.org/sql/connection;1?type=mysql"

class mozSqlConnectionMysql : public mozSqlConnection,
                              public mozISqlConnectionMysql
{
  public:
    mozSqlConnectionMysql();
    virtual ~mozSqlConnectionMysql();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(const nsAString& aHost, PRInt32 aPort,
                    const nsAString& aDatabase, const nsAString& aUsername,
		    const nsAString& aPassword);

    NS_IMETHODIMP GetServerVersion(nsAString &aServerVersion);

    NS_IMETHOD GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable, mozISqlResult** _retval);

    NS_DECL_MOZISQLCONNECTIONMYSQL

    MYSQL *mConnection;

  protected:

    virtual nsresult RealExec(const nsAString& aQuery,
                              mozISqlResult** aResult, PRInt32* aAffectedRows);

    virtual nsresult CancelExec();

    virtual nsresult GetIDName(nsAString& aIDName);

  private:
    // MysqlConnection* mConnection;
};

#endif // mozSqlConnectionMysql_h
