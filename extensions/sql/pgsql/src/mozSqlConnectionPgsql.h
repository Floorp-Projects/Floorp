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

#ifndef mozSqlConnectionPgsql_h
#define mozSqlConnectionPgsql_h                                                  

#include "libpq-fe.h"
#include "mozSqlConnection.h"
#include "mozISqlConnectionPgsql.h"

#define MOZ_SQLCONNECTIONPGSQL_CLASSNAME "PosgreSQL SQL Connection"
#define MOZ_SQLCONNECTIONPGSQL_CID \
{0x0cf1eefe, 0x611d, 0x48fa, {0xae, 0x27, 0x0a, 0x6f, 0x40, 0xd6, 0xa3, 0x3e }}
#define MOZ_SQLCONNECTIONPGSQL_CONTRACTID "@mozilla.org/sql/connection;1?type=pgsql"

#define SERVER_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

class mozSqlConnectionPgsql : public mozSqlConnection,
                              public mozISqlConnectionPgsql
{
  public:
    mozSqlConnectionPgsql();
    virtual ~mozSqlConnectionPgsql();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(const nsAString& aHost, PRInt32 aPort,
                    const nsAString& aDatabase, const nsAString& aUsername,
		    const nsAString& aPassword);

    NS_IMETHOD GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable, mozISqlResult** _retval);

    NS_DECL_MOZISQLCONNECTIONPGSQL

  protected:
    nsresult Setup();

    virtual nsresult RealExec(const nsAString& aQuery,
                              mozISqlResult** aResult, PRInt32* aAffectedRows);

    virtual nsresult CancelExec();

    virtual nsresult GetIDName(nsAString& aIDName);

  private:
    PGconn*             mConnection;
    PRInt32             mVersion;
};

#endif // mozSqlConnectionPgsql_h
