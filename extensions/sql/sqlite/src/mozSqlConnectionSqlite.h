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
 *   Valia Vaneeva <fattie@altlinux.ru>
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

#ifndef mozSqlConnectionSqlite_h
#define mozSqlConnectionSqlite_h                                                  

#include "sqlite3.h"
#include "mozSqlConnection.h"
#include "mozISqlConnectionSqlite.h"

#define MOZ_SQLCONNECTIONSQLITE_CLASSNAME "SQLite SQL Connection"
#define MOZ_SQLCONNECTIONSQLITE_CID \
{0xb00f42b4, 0x8902, 0x428c, {0xab, 0x13, 0x9d, 0x06, 0x6f, 0xf1, 0x5a, 0xed }}

#define MOZ_SQLCONNECTIONSQLITE_CONTRACTID "@mozilla.org/sql/connection;1?type=sqlite"

#define SERVER_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))

class mozSqlConnectionSqlite : public mozSqlConnection,
                               public mozISqlConnectionSqlite
{
  public:
    mozSqlConnectionSqlite();
    virtual ~mozSqlConnectionSqlite();

    NS_DECL_ISUPPORTS

    NS_IMETHOD Init(const nsAString& aHost, PRInt32 aPort,
                    const nsAString& aDatabase, const nsAString& aUsername,
                    const nsAString& aPassword);

    NS_IMETHOD GetPrimaryKeys(const nsAString& aSchema, const nsAString& aTable,
                              mozISqlResult** _retval);

    NS_DECL_MOZISQLCONNECTIONSQLITE

  protected:
    nsresult Setup();

    virtual nsresult RealExec(const nsAString& aQuery, mozISqlResult** aResult,
                              PRInt32* aAffectedRows);

    virtual nsresult CancelExec();

    virtual nsresult GetIDName(nsAString& aIDName);

  private:
    sqlite3*            mConnection;
    PRInt32             mVersion;
    PRBool              mWritable;
};

#endif // mozSqlConnectionSqlite_h
