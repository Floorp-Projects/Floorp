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

#ifndef mozSqlResultSqlite_h
#define mozSqlResultSqlite_h                                                  

#include "sqlite3.h"
#include "mozSqlResult.h"
#include "mozISqlResultSqlite.h"

class mozSqlResultSqlite : public mozSqlResult,
                          public mozISqlResultSqlite
{
  public:
    mozSqlResultSqlite(mozISqlConnection* aConnection, const nsAString& aQuery);
    void SetResult(char** aResult, PRInt32 nrow, PRInt32 ncolumn,
                   PRBool aWritable);
    virtual ~mozSqlResultSqlite();

    NS_DECL_ISUPPORTS

    NS_DECL_MOZISQLRESULTSQLITE 

  protected:
    PRInt32 GetColType(PRInt32 aColumnIndex);

    virtual nsresult BuildColumnInfo();
    virtual nsresult BuildRows();
    virtual void ClearNativeResult();

    virtual nsresult CanInsert(PRBool* _retval);
    virtual nsresult CanUpdate(PRBool* _retval);
    virtual nsresult CanDelete(PRBool* _retval);

  private:
    char**   	 mResult;
    PRInt32      mColumnCount;
    PRInt32	 mRowCount;
    PRInt32	 mWritable;
};

#endif // mozSqlResultSqlite_h
