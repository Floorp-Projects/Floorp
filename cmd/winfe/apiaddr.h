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
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#ifndef __APIADDR_H
#define __APIADDR_H

#ifndef __APIAPI_H
    #include "apiapi.h"
#endif
#ifndef __NSGUIDS_H
    #include "nsguids.h"
#endif

#include "abcom.h"


typedef enum {NC_NameComplete, NC_Expand, NC_RFC822, NC_None} NameCompletionEnum;

class IAddressParent
{
public:
    virtual void AddedItem (
        HWND hwnd,
        LONG id,
        int index
        ) = 0;

    virtual int ChangedItem (
        char * pString,
        int index,
		HWND hwnd,
		char ** ppFullName,
		unsigned long* entryID,
		UINT* bitmapID
        ) = 0;

    virtual void DeletedItem (
        HWND hwnd,
        LONG id,
        int index
        ) = 0;

    virtual char * NameCompletion (
        char * pString
        ) = 0;

	virtual void StartNameCompletionSearch(
		void
		)=0;

	virtual void StopNameCompletionSearch(
		void
		)=0;

	virtual void SetProgressBarPercent(
		int32 lPercent
		)=0;

	virtual void SetStatusText(
		const char* pMessage
		)=0;

	virtual CWnd *GetOwnerWindow(
		void
		)=0;

};

typedef IAddressParent * LPADDRESSPARENT;

// Address control API

#define	APICLASS_ADDRESSCONTROL   "AddressControl"

typedef struct {
	ULONG ulHeaderType;
	char * szAddress;
	UINT idBitmap;
	ULONG idEntry;
} NSADDRESSLIST;

typedef NSADDRESSLIST * LPNSADDRESSLIST;

#define WM_CHILDLOSTFOCUS           WM_USER+600
#define	WM_LEAVINGLASTFIELD	        WM_USER+601
#define WM_UPDATEHEADERTYPE         WM_USER+602
#define WM_UPDATEHEADERCONTENTS     WM_USER+603
#define WM_NOTIFYSELECTIONCHANGE    WM_USER+604
#define WM_DISPLAYTYPELIST          WM_USER+605

class CNSAddressList;
class CNSAddressTypeControl;

class IAddressControl 
{
public:
    virtual BOOL AddAddressType (
        char * pszChoice, 
        UINT pidBitmap = 0, 
        BOOL bExpand = TRUE,
        BOOL bHidden = FALSE,
        BOOL bExclusive = FALSE,
        DWORD dwUserData = 0
        ) = 0;

    virtual void SetDefaultBitmapId (
        int id = 0
        ) = 0;

    virtual int GetDefaultBitmapId (
        void
        ) = 0;

    virtual BOOL RemoveSelection (
        int nIndex = -1
        ) = 0;

	virtual int	AppendEntry (
		BOOL expandName = TRUE,
        LPCTSTR szType = 0,
        LPCTSTR szName = 0,
        UINT idBitmap = 0,
        unsigned long idEntry = 0
        ) = 0; 

	virtual int	InsertEntry(
        int nIndex,
		BOOL expandName = TRUE,
        LPCTSTR szType = 0,
        LPCTSTR szName = 0,
        UINT idBitmap = 0,
        unsigned long idEntry = 0
        ) = 0;

	virtual BOOL SetEntry ( 
        int nIndex, 
        LPCTSTR szType = 0,
        LPCTSTR szName = 0,
        UINT idBitmap = 0,
        unsigned long idEntry = 0
        ) = 0;

	virtual BOOL GetEntry ( 
        int nIndex, 
        char ** szType = 0,
        char ** szName = 0,
        UINT * idBitmap = 0,
        unsigned long * idEntry = 0
        ) = 0;

   virtual int GetItemFromPoint(
        LPPOINT point
        ) = 0;

	virtual BOOL DeleteEntry ( 
        int nIndex 
        ) = 0;

	virtual int	FindEntry ( 
        int nStart, 
        LPCTSTR lpszName 
        ) = 0;

	virtual BOOL Create (
        CWnd *pParent, 
        int id = 1000
        ) = 0;

	virtual CListBox * GetAddressTypeComboBox (
        void 
        ) = 0;

	virtual CEdit * GetAddressNameField ( 
        void 
        ) = 0;

	virtual void SetItemName (
        int nIndex, 
        char * text
        ) = 0;

	virtual void SetItemBitmap ( 
        int nIndex, 
        UINT id
        ) = 0;

	virtual void SetItemEntryID ( 
        int nIndex, 
        unsigned long id
        ) = 0;

    virtual void SetControlParent ( 
        LPADDRESSPARENT pIAddressParent
        ) = 0;

    virtual int GetAddressList (
        LPNSADDRESSLIST * ppAdressList
        ) = 0;

    virtual int SetAddressList (
        LPNSADDRESSLIST pAddressList,
        int count
        ) = 0;

    virtual CListBox * GetListBox(
        void
        ) = 0;

    virtual BOOL IsCreated(
        void
        ) = 0;
	virtual int SetSel(
        int nIndex,
		BOOL bSelected
        ) = 0;

	virtual void SetContext(
		MWContext *pContext
		) = 0;

	virtual MWContext *GetContext(
		void
		) = 0;

#define ADDRESS_TYPE_FLAG               UINT
#define ADDRESS_TYPE_FLAG_VALUE         0x1
#define ADDRESS_TYPE_FLAG_HIDDEN        0x2
#define ADDRESS_TYPE_FLAG_EXCLUSIVE     0x4
#define ADDRESS_TYPE_FLAG_BITMAP        0x8
#define ADDRESS_TYPE_FLAG_USER          0x10

    virtual void GetTypeInfo(
        int nIndex, 
        ADDRESS_TYPE_FLAG flag, 
        void ** value
        ) = 0;

    virtual void EnableParsing(
        BOOL bParse = TRUE
        ) = 0;

	virtual BOOL GetEnableParsing(
		void
		)=0;

	virtual void EnableExpansion(
		BOOL bExpand = TRUE
		)=0;

	virtual BOOL GetEnableExpansion(
		void
		)=0;
    virtual void SetCSID (
        int16 csid = 0
        ) = 0;
	virtual void ShowNameCompletionPicker(
		CWnd *pParent = NULL
		)=0;

	virtual void StartNameCompletion(
		int nIndex = -1
		)=0;

	virtual void StopNameCompletion(
		int nIndex = -1,
		BOOL bEraseCookie = TRUE
		)=0;
	virtual void SetEntryHasNameCompletion(
		BOOL bHasNameCompletion = TRUE,
		int nIndex = -1
		)=0;
	virtual BOOL GetEntryHasNameCompletion(
		int nIndex = -1
		)=0;
	virtual void SetNameCompletionCookieInfo(
		AB_NameCompletionCookie *pCookie,
		int nNumResults,
		NameCompletionEnum ncEnum,
		int nIndex = -1)=0;
	virtual void GetNameCompletionCookieInfo(
		AB_NameCompletionCookie **pCookie,
		int *pNumResults,
		int nIndex = -1)=0;

  };

typedef IAddressControl * LPADDRESSCONTROL;

#define ApiAddressControl(v,unk) APIPTRDEF(IID_IAddressControl,IAddressControl,v,unk)

#endif
