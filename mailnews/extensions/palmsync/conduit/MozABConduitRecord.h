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
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 *
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *       Rajiv Dayal <rdayal@netscape.com>
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

#ifndef __MOZABCONDUIT_RECORD_H_
#define __MOZABCONDUIT_RECORD_H_

#include <syncmgr.h>
#include "CPString.h"
#include "CPalmRec.h"
#include <PGenErr.h>
#include <tables.h>

#include "IPalmSync.h"

// these identify the types of phone numbers in Palm record
#define PHONE_LABEL_WORK        0
#define PHONE_LABEL_HOME        1
#define PHONE_LABEL_FAX         2
#define PHONE_LABEL_OTHER       3
#define PHONE_LABEL_EMAIL       4
#define PHONE_LABEL_MAIN        5
#define PHONE_LABEL_PAGER       6
#define PHONE_LABEL_MOBILE      7

// these are flags which says if a field in Palm record exist or not
struct AddrDBRecordFlags {
    unsigned name               :1;
    unsigned firstName          :1;
    unsigned company            :1;
    unsigned phone1             :1;
    unsigned phone2             :1;
    unsigned phone3             :1;
    unsigned phone4             :1;
    unsigned phone5             :1;
    unsigned address            :1;
    unsigned city               :1;
    unsigned state              :1;
    unsigned zipCode            :1;
    unsigned country            :1;
    unsigned title              :1;
    unsigned custom1            :1;
    unsigned custom2            :1;
    unsigned custom3            :1;
    unsigned custom4            :1;
    unsigned note               :1;
    unsigned reserved           :13;
};

// these are the flag that identify the type of phone #s in Palm record
struct AddrOptionsType {
    unsigned phone1             :4;
    unsigned phone2             :4;
    unsigned phone3             :4;
    unsigned phone4             :4;
    unsigned phone5             :4;
    unsigned displayPhone       :4;
    unsigned reserved           :8;
};

// class implementation that represents the Mozilla AB Record type 
// used along with Palm record type
class CMozABConduitRecord {
    
public:
    CMozABConduitRecord(void);
    CMozABConduitRecord(CPalmRecord &rec);
    CMozABConduitRecord(nsABCOMCardStruct &rec);
    virtual ~CMozABConduitRecord();

public:
    // Mozilla AB card data structure member
    nsABCOMCardStruct m_nsCard;

    // the following data members are for Palm Record
    DWORD m_dwRecordID;
    DWORD m_dwStatus;
    DWORD m_dwPosition;
    CPString m_csName;
    CPString m_csFirst;
    CPString m_csDisplayName;
    CPString m_csTitle;
    CPString m_csCompany;
    DWORD m_dwPhone1LabelID;
    CPString m_csPhone1;
    DWORD m_dwPhone2LabelID;
    CPString m_csPhone2;
    DWORD m_dwPhone3LabelID;
    CPString m_csPhone3;
    DWORD m_dwPhone4LabelID;
    CPString m_csPhone4;
    DWORD m_dwPhone5LabelID;
    CPString m_csPhone5;
    CPString m_csAddress;
    CPString m_csCity;
    CPString m_csState;
    CPString m_csZipCode;
    CPString m_csCountry;
    CPString m_csNote;
    DWORD m_dwPrivate;
    DWORD m_dwCategoryID;
    CPString m_csCustom1;
    CPString m_csCustom2;
    CPString m_csCustom3;
    CPString m_csCustom4;
    DWORD m_dwDisplayPhone; 

protected:

    // internal utility functions
    long AddCRs(TCHAR* pSrc, TCHAR* pDest, int len, int iBufSize);
    long AddCRs(TCHAR* pSrc, TCHAR* pDest, int len);
    int  StripCRs(TCHAR* pDest, const TCHAR* pSrc, int len);

    void AssignPhoneToMozData(DWORD labelID, LPTSTR buf);

public:
    // rests the data in this record object   
    void Reset(void);
    // compares with another record object
    eRecCompare Compare(const CMozABConduitRecord &rec);

    // converts from Palm record type to self(Mozilla AB) type
    long ConvertFromGeneric(CPalmRecord &rec);
    // converts from self(Mozilla AB) type to Palm record type
    long ConvertToGeneric(CPalmRecord &rec);

    static void CleanUpABCOMCardStruct(nsABCOMCardStruct * card);

};

#endif
