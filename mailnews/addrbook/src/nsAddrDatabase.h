/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1999 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _nsAddrDatabase_H_
#define _nsAddrDatabase_H_

#include "nsIAddrDatabase.h"
#include "mdb.h"
#include "nsVoidArray.h"
#include "nsString.h"
#include "nsFileSpec.h"
#include "nsIAddrDBListener.h"
#include "nsISupportsArray.h"

typedef enum 
{
	AB_NotifyInserted,
	AB_NotifyDeleted,
	AB_NotifyPropertyChanged,

	AB_NotifyAll,		   /* contents of the have totally changed. Listener must totally
							  forget anything they knew about the object. */
	/* pane notifications (i.e. not tied to a particular entry */
	AB_NotifyScramble,     /* same contents, but the view indices have all changed 
						      i.e the object was sorted on a different attribute */
	AB_NotifyLDAPTotalContentChanged,
	AB_NotifyNewTopIndex,
	AB_NotifyStartSearching,
	AB_NotifyStopSearching

} AB_NOTIFY_CODE;

enum nsAddrDBCommitType {
  kSmallCommit,
  kLargeCommit,
  kSessionCommit,
  kCompressCommit
};

class nsAddrDatabase : public nsIAddrDatabase 
{
public:
	NS_DECL_ISUPPORTS

	//////////////////////////////////////////////////////////////////////////////
	// nsIAddrDBAnnouncer methods:

	NS_IMETHOD AddListener(nsIAddrDBListener *listener);
	NS_IMETHOD RemoveListener(nsIAddrDBListener *listener);
	NS_IMETHOD NotifyCardAttribChange(PRUint32 abCode, nsIAddrDBListener *instigator);
	NS_IMETHOD NotifyCardEntryChange(PRUint32 abCode, nsIAbCard *card, nsIAddrDBListener *instigator);
	NS_IMETHOD NotifyAnnouncerGoingAway();

	//////////////////////////////////////////////////////////////////////////////
	// nsIAddrDatabase methods:

	NS_IMETHOD GetDbPath(nsFileSpec * *aDbPath);
	NS_IMETHOD SetDbPath(nsFileSpec * aDbPath);
	NS_IMETHOD Open(nsFileSpec * folderName, PRBool create, nsIAddrDatabase **pCardDB, PRBool upgrading);
	NS_IMETHOD Close(PRBool forceCommit);
	NS_IMETHOD OpenMDB(nsFileSpec *dbName, PRBool create);
	NS_IMETHOD CloseMDB(PRBool commit);
	NS_IMETHOD OpenAnonymousDB(nsIAddrDatabase **pCardDB);
	NS_IMETHOD CloseAnonymousDB(PRBool forceCommit);
	NS_IMETHOD Commit(PRUint32 commitType);
	NS_IMETHOD ForceClosed();

	NS_IMETHOD CreateNewCardAndAddToDB(nsIAbCard *newCard, PRBool notify);
	NS_IMETHOD EnumerateCards(nsIAbDirectory *directory, nsIEnumerator **result);
	NS_IMETHOD EnumerateMailingLists(nsIAbDirectory *directory, nsIEnumerator **result);
	NS_IMETHOD DeleteCard(nsIAbCard *newCard, PRBool notify);
	NS_IMETHOD EditCard(nsIAbCard *card, PRBool notify);
	NS_IMETHOD ContainsCard(nsIAbCard *card, PRBool *hasCard);

	NS_IMETHOD GetCardForEmailAddress(nsIAbDirectory *directory, const char *emailAddress, nsIAbCard **card);

	NS_IMETHOD SetAnonymousStringAttribute(const char *attrname, const char *value);
	NS_IMETHOD GetAnonymousStringAttribute(const char *attrname, char** value);
	NS_IMETHOD SetAnonymousIntAttribute(const char *attrname, PRUint32 value);
	NS_IMETHOD GetAnonymousIntAttribute(const char *attrname, PRUint32* value);
	NS_IMETHOD SetAnonymousBoolAttribute(const char *attrname, PRBool value);
	NS_IMETHOD GetAnonymousBoolAttribute(const char *attrname, PRBool* value);
	NS_IMETHOD AddAnonymousAttributesToDB();
	NS_IMETHOD RemoveAnonymousAttributesFromDB();
	NS_IMETHOD EditAnonymousAttributesInDB();

	NS_IMETHOD AddAnonymousAttributesFromCard(nsIAbCard *card);
	NS_IMETHOD RemoveAnonymousAttributesFromCard(nsIAbCard *card);
	NS_IMETHOD EditAnonymousAttributesFromCard(nsIAbCard *card);

	//////////////////////////////////////////////////////////////////////////////
	// nsAddrDatabase methods:

	nsAddrDatabase();
	virtual ~nsAddrDatabase();

	nsIMdbFactory	*GetMDBFactory();
	nsIMdbEnv		*GetEnv() {return m_mdbEnv;}
	nsIMdbStore		*GetStore() {return m_mdbStore;}
	PRUint32		GetCurVersion();
	nsIMdbTableRowCursor *GetTableRowCursor();
	nsIMdbTable		*GetPabTable() {return m_mdbPabTable;}
	nsIMdbTable		*GetAnonymousTable() {return m_mdbAnonymousTable;}

	static nsAddrDatabase*	FindInCache(nsFileSpec *dbName);

	static void		CleanupCache();

	nsresult CreateABCard(nsIMdbRow* cardRow, nsIAbCard **result);

protected:

    static void		AddToCache(nsAddrDatabase* pAddrDB) 
						{GetDBCache()->AppendElement(pAddrDB);}
	static void		RemoveFromCache(nsAddrDatabase* pAddrDB);
	static PRInt32	FindInCache(nsAddrDatabase* pAddrDB);
	PRBool			MatchDbName(nsFileSpec *dbName);	// returns TRUE if they match

#if defined(XP_PC) || defined(XP_MAC)	// this should go away when we can provide our own file stream to MDB/Mork
	static void		UnixToNative(char*& ioPath);
#endif
#if defined(XP_MAC)
	static void		NativeToUnix(char*& ioPath);
#endif


	void YarnToUInt32(struct mdbYarn *yarn, PRUint32 *pResult);
	void GetStringYarn(char* str, struct mdbYarn* strYarn);
	void GetIntYarn(PRUint32 nValue, struct mdbYarn* intYarn);
	mdb_err AddStringColumn(nsIMdbRow* cardRow, mdb_column inColumn, char* str);
	mdb_err AddIntColumn(nsIMdbRow* cardRow, mdb_column inColumn, PRUint32 nValue);
	nsresult GetStringColumn(nsIMdbRow *cardRow, mdb_token outToken, nsString& str);
	nsresult GetIntColumn(nsIMdbRow *cardRow, mdb_token outToken, 
							PRUint32* pValue, PRUint32 defaultValue);
	nsresult GetBoolColumn(nsIMdbRow *cardRow, mdb_token outToken, PRBool* pValue);
	nsresult GetCardFromDB(nsIAbCard *newCard, nsIMdbRow* cardRow);
	nsresult GetAnonymousAttributesFromDB();
	nsresult AddAttributeColumnsToRow(nsIAbCard *card, nsIMdbRow *cardRow);
	nsresult RemoveAnonymousList(nsVoidArray* pArray);
	nsresult SetAnonymousAttribute(nsVoidArray** pAttrAray, 
							nsVoidArray** pValueArray, void *attrname, void *value);
	nsresult DoAnonymousAttributesTransaction(AB_NOTIFY_CODE code);
	nsresult DoStringAnonymousTransaction(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code);
	nsresult DoIntAnonymousTransaction(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code);
	nsresult DoBoolAnonymousTransaction(nsVoidArray* pAttributes, nsVoidArray* pValues, AB_NOTIFY_CODE code);
	void GetAnonymousAttributesFromCard(nsIAbCard* card);
	nsresult FindAttributeRow(nsIMdbTable* pTable, mdb_token columnToken, nsIMdbRow** row);

	static nsVoidArray/*<nsAddrDatabase>*/* GetDBCache();
	static nsVoidArray/*<nsAddrDatabase>*/* m_dbCache;

	// mdb bookkeeping stuff
	nsresult			InitExistingDB();
	nsresult			InitNewDB();
	nsresult			InitMDBInfo();
	nsresult			InitPabTable();
	nsresult			InitAnonymousTable();

	nsIMdbEnv		    *m_mdbEnv;	// to be used in all the db calls.
	nsIMdbStore	 	    *m_mdbStore;
	nsIMdbTable		    *m_mdbPabTable;
	nsIMdbRow			*m_mdbRow;	// singleton row in table;
	nsFileSpec		    m_dbName;
	PRBool				m_mdbTokensInitialized;
    nsVoidArray/*<nsIAddrDBListener>*/ *m_ChangeListeners;

	nsIMdbTable		    *m_mdbAnonymousTable;
	mdb_kind			m_AnonymousTableKind;
	nsVoidArray*		m_pAnonymousStrAttributes;
	nsVoidArray*		m_pAnonymousStrValues;
	nsVoidArray*		m_pAnonymousIntAttributes;
	nsVoidArray*		m_pAnonymousIntValues;
	nsVoidArray*		m_pAnonymousBoolAttributes;
	nsVoidArray*		m_pAnonymousBoolValues;

	mdb_kind			m_PabTableKind;
 	mdb_kind			m_HistoryTableKind;
	mdb_kind			m_MailListTableKind;
	mdb_kind			m_CategoryTableKind;

	mdb_scope			m_CardRowScopeToken;

	mdb_token			m_FirstNameColumnToken;
	mdb_token			m_LastNameColumnToken;
	mdb_token			m_DisplayNameColumnToken;
	mdb_token			m_NickNameColumnToken;
	mdb_token			m_PriEmailColumnToken;
	mdb_token			m_2ndEmailColumnToken;
	mdb_token			m_WorkPhoneColumnToken;
	mdb_token			m_HomePhoneColumnToken;
	mdb_token			m_FaxColumnToken;
	mdb_token			m_PagerColumnToken;
	mdb_token			m_CellularColumnToken;
	mdb_token			m_HomeAddressColumnToken;
	mdb_token			m_HomeAddress2ColumnToken;
	mdb_token			m_HomeCityColumnToken;
	mdb_token			m_HomeStateColumnToken;
	mdb_token			m_HomeZipCodeColumnToken;
	mdb_token			m_HomeCountryColumnToken;
	mdb_token			m_WorkAddressColumnToken;
	mdb_token			m_WorkAddress2ColumnToken;
	mdb_token			m_WorkCityColumnToken;
	mdb_token			m_WorkStateColumnToken;
	mdb_token			m_WorkZipCodeColumnToken;
	mdb_token			m_WorkCountryColumnToken;
	mdb_token			m_JobTitleColumnToken;
	mdb_token			m_DepartmentColumnToken;
	mdb_token			m_CompanyColumnToken;
	mdb_token			m_WebPage1ColumnToken;
	mdb_token			m_WebPage2ColumnToken;
	mdb_token			m_BirthYearColumnToken;
	mdb_token			m_BirthMonthColumnToken;
	mdb_token			m_BirthDayColumnToken;
	mdb_token			m_Custom1ColumnToken;
	mdb_token			m_Custom2ColumnToken;
	mdb_token			m_Custom3ColumnToken;
	mdb_token			m_Custom4ColumnToken;
	mdb_token			m_NotesColumnToken;
	mdb_token			m_LastModDateColumnToken;

	mdb_token			m_PlainTextColumnToken;

	mdb_token			m_AddressCharSetColumnToken;

	nsIAbDirectory*		m_dbDirectory;
};

#endif
