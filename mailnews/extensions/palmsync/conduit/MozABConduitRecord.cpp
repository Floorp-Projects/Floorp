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

#include <windows.h>
#include <string.h>
#include <stdio.h>
#include <memory.h>
#include <sys/stat.h>
#include <TCHAR.H>

#include "MozABConduitRecord.h"
#include "MozABPCManager.h"
#include "MozABConduitSync.h"

#define  STR_CRETURN  "\r" 
#define  STR_NEWLINE  "\n" 

#define  CH_CRETURN  0x0D 
#define  CH_NEWLINE  0x0A 


// constructor
CMozABConduitRecord::CMozABConduitRecord(void)
{
    Reset();
}

// construct using Palm record
CMozABConduitRecord::CMozABConduitRecord(CPalmRecord &rec)
{
    ConvertFromGeneric(rec);
}

// construct using Mozilla record
CMozABConduitRecord::CMozABConduitRecord(nsABCOMCardStruct &rec)
{
    Reset();

    memcpy(&m_nsCard, &rec, sizeof(nsABCOMCardStruct));

    m_dwRecordID = m_nsCard.dwRecordId;
    m_dwStatus = m_nsCard.dwStatus;
    m_dwCategoryID = m_nsCard.dwCategoryId;

    m_dwPhone1LabelID = PHONE_LABEL_WORK;
    m_dwPhone2LabelID = PHONE_LABEL_HOME;
    m_dwPhone3LabelID = PHONE_LABEL_FAX;
    m_dwPhone4LabelID = PHONE_LABEL_MOBILE;
    m_dwPhone5LabelID = PHONE_LABEL_EMAIL;
    
    m_csName = rec.lastName;
    m_csFirst = rec.firstName;
    m_csTitle = rec.jobTitle;
    m_csCompany = rec.company;

    m_csPhone1 = rec.workPhone;
    m_csPhone2 = rec.homePhone;
    m_csPhone3 = rec.faxNumber;
    m_csPhone4 = rec.cellularNumber;
    m_csPhone5 = rec.primaryEmail;

    m_csAddress = (MozABPCManager::gUseHomeAddress)
                    ? rec.homeAddress : rec.workAddress;

    m_csCity = (MozABPCManager::gUseHomeAddress) ? rec.homeCity : rec.workCity;
    m_csState = (MozABPCManager::gUseHomeAddress) ? rec.homeState : rec.workState;
    m_csZipCode = (MozABPCManager::gUseHomeAddress) ? rec.homeZipCode : rec.workZipCode;
    m_csCountry = (MozABPCManager::gUseHomeAddress) ? rec.homeCountry : rec.workCountry;
//    CONDUIT_LOG3(gFD, "\nCMozABConduitRecord::CMozABConduitRecord(nsABCOMCardStruct &rec) gUseHomeAddress = %s card home address = %s card work address = %s\n", 
//      (MozABPCManager::gUseHomeAddress) ? "true" : "false", (char *) rec.homeAddress, (char *) rec.workAddress);
    m_dwDisplayPhone = rec.preferredPhoneNum;

    m_csNote = rec.notes;

    m_csCustom1 = rec.custom1;
    m_csCustom2 = rec.custom2;
    m_csCustom3 = rec.custom3;
    m_csCustom4 = rec.custom4;
}

// destructor
CMozABConduitRecord::~CMozABConduitRecord()
{

}

// reset the internal data members
void CMozABConduitRecord::Reset(void)
{
    memset(&m_nsCard, 0, sizeof(nsABCOMCardStruct));

    m_dwRecordID = 0;
    m_dwStatus = 0;
    m_dwPosition = 0;
    m_dwPhone1LabelID = 0;
    m_dwPhone2LabelID = 0;
    m_dwPhone3LabelID = 0;
    m_dwPhone4LabelID = 0;
    m_dwPhone5LabelID = 0;
    m_dwPrivate = 0;
    m_dwCategoryID = 0;
    m_dwDisplayPhone = 1; // work 
    m_csName.Empty();
    m_csFirst.Empty();
    m_csTitle.Empty();
    m_csCompany.Empty();
    m_csPhone1.Empty();
    m_csPhone2.Empty();
    m_csPhone3.Empty();
    m_csPhone4.Empty();
    m_csCustom1.Empty();
    m_csCustom2.Empty();
    m_csCustom3.Empty();
    m_csCustom4.Empty();
    m_csPhone5.Empty();
    m_csAddress.Empty();
    m_csCity.Empty();
    m_csState.Empty();
    m_csZipCode.Empty();
    m_csCountry.Empty();
    m_csNote.Empty();
}

// this method compares this object with another MozAB Conduit record
eRecCompare CMozABConduitRecord::Compare(const CMozABConduitRecord &rec)
{
    if ( (m_dwPhone1LabelID != rec.m_dwPhone1LabelID) ||
         (m_dwPhone2LabelID != rec.m_dwPhone2LabelID) ||
         (m_dwPhone3LabelID != rec.m_dwPhone3LabelID ) ||
         (m_dwPhone4LabelID != rec.m_dwPhone4LabelID ) ||
         (m_dwPhone5LabelID != rec.m_dwPhone5LabelID ) ||
         // comment this out until we can match 4.x's displayPhone algorithm
//         (m_dwDisplayPhone !=  rec.m_dwDisplayPhone ) ||
         (m_csName !=  rec.m_csName ) ||
         (m_csFirst !=  rec.m_csFirst ) ||
         (m_csTitle !=  rec.m_csTitle ) ||
         (m_csCompany !=  rec.m_csCompany ) ||
         (m_csPhone1 !=  rec.m_csPhone1 ) ||
         (m_csPhone2 !=  rec.m_csPhone2 ) ||
         (m_csPhone3 !=  rec.m_csPhone3 ) ||
         (m_csPhone4 !=  rec.m_csPhone4 ) ||
         (m_csCustom1 !=  rec.m_csCustom1 ) ||
         (m_csCustom2 !=  rec.m_csCustom2 ) ||
         (m_csCustom3 !=  rec.m_csCustom3 ) ||
         (m_csCustom4 !=  rec.m_csCustom4 ) ||
         (m_csPhone5 !=  rec.m_csPhone5 ) ||
         (m_csAddress !=  rec.m_csAddress ) ||
         (m_csCity !=  rec.m_csCity ) ||
         (m_csState !=  rec.m_csState ) ||
         (m_csZipCode !=  rec.m_csZipCode ) ||
         (m_csCountry !=  rec.m_csCountry ) ||
         (m_csNote !=  rec.m_csNote ))
         return eData;
    if (m_dwPrivate != rec.m_dwPrivate ) 
        return ePrivateFlag;
    if (m_dwCategoryID != rec.m_dwCategoryID ) 
        return eCategory;

    return eEqual;
}

#define COPY_FROM_GENERIC(m, d)  {  iLen = _tcslen((char *)pBuff);  \
                                    CopyFromHHBuffer((char *)pBuff, m.GetBuffer(iLen * 2), iLen); \
                                    m.ReleaseBuffer();              \
                                    d = m.GetBuffer(0);             \
                                    pBuff += iLen + 1;              \
                                 }                                  \

#define COPY_PHONE_FROM_GENERIC(m, d){  iLen = _tcslen((char *)pBuff);              \
                                        CopyFromHHBuffer((char *)pBuff, m.GetBuffer(iLen * 2), iLen); \
                                        m.ReleaseBuffer();                          \
                                        AssignPhoneToMozData(d, m.GetBuffer(0));    \
                                        pBuff += iLen + 1;                          \
                                     }                                              \


// this method converts CPalmRecord to MozAB Conduit record 
long CMozABConduitRecord::ConvertFromGeneric(CPalmRecord &rec)
{
    long                retval = 0;
    BYTE                *pBuff;
    AddrDBRecordFlags   flags;
    AddrOptionsType     options;
    int iLen;

    Reset();

    if (rec.GetRawDataSize() == (sizeof(DWORD) + sizeof(DWORD) + sizeof(unsigned char))){
        // then this is a deleted record because it has no data
        return GEN_ERR_EMPTY_RECORD;
    }

    // palm specific data
    m_dwRecordID = rec.GetID();
    m_dwCategoryID = rec.GetCategory();
    m_dwPrivate    = rec.IsPrivate();
    m_dwStatus = rec.GetStatus();

    m_nsCard.dwRecordId = m_dwRecordID;
    m_nsCard.dwCategoryId = m_dwCategoryID;
    m_nsCard.dwStatus = m_dwStatus;

    // clear all values in the Address Record
    m_dwPhone1LabelID = PHONE_LABEL_WORK;
    m_dwPhone2LabelID = PHONE_LABEL_HOME;
    m_dwPhone3LabelID = PHONE_LABEL_FAX;
    m_dwPhone4LabelID = PHONE_LABEL_MOBILE;
    m_dwPhone5LabelID = PHONE_LABEL_EMAIL;
    m_dwDisplayPhone = m_nsCard.preferredPhoneNum;

    DWORD dwRawSize = rec.GetRawDataSize();
    if (!dwRawSize) {
        // invalid record
        return 0;
    }

    // get the data for moz card
    pBuff = rec.GetRawData();

    // AddrOptionsType
    *(DWORD *)&options = CPalmRecord::SwapDWordToIntel(*((DWORD*)pBuff));
    m_dwDisplayPhone  = (long)options.displayPhone + 1;
    m_dwPhone1LabelID = (DWORD)options.phone1;
    m_dwPhone2LabelID = (DWORD)options.phone2;
    m_dwPhone3LabelID = (DWORD)options.phone3;
    m_dwPhone4LabelID = (DWORD)options.phone4;
    m_dwPhone5LabelID = (DWORD)options.phone5;

    pBuff += sizeof(DWORD);

    // The flag bits are used to indicate which fields exist
    *(DWORD *)&flags = CPalmRecord::SwapDWordToIntel(*((DWORD*)pBuff));
    pBuff += sizeof(DWORD) + sizeof(unsigned char);

    // Name
    if (flags.name) COPY_FROM_GENERIC(m_csName, m_nsCard.lastName)
    // FirstName
    if (flags.firstName) COPY_FROM_GENERIC(m_csFirst, m_nsCard.firstName)
    // DisplayName
    m_csDisplayName = m_nsCard.firstName;
    m_csDisplayName += " ";
    m_csDisplayName += m_nsCard.lastName;
    m_nsCard.displayName = m_csDisplayName.GetBuffer(0);
    // Company
    if (flags.company) COPY_FROM_GENERIC(m_csCompany, m_nsCard.company)
    // Phones
    if (flags.phone1) COPY_PHONE_FROM_GENERIC(m_csPhone1, m_dwPhone1LabelID)
    if (flags.phone2) COPY_PHONE_FROM_GENERIC(m_csPhone2, m_dwPhone2LabelID)
    if (flags.phone3) COPY_PHONE_FROM_GENERIC(m_csPhone3, m_dwPhone3LabelID)
    if (flags.phone4) COPY_PHONE_FROM_GENERIC(m_csPhone4, m_dwPhone4LabelID)
    if (flags.phone5) COPY_PHONE_FROM_GENERIC(m_csPhone5, m_dwPhone5LabelID)
    // Address
    if (flags.address) 
    {
      if (MozABPCManager::gUseHomeAddress)
        COPY_FROM_GENERIC(m_csAddress, m_nsCard.homeAddress)
      else
         COPY_FROM_GENERIC(m_csAddress, m_nsCard.workAddress)

//      CONDUIT_LOG4(gFD, "\nConvertFromGeneric gUseHomeAddress = %s card home address = %s card work address = %s result = %s\n", 
//        (MozABPCManager::gUseHomeAddress) ? "true" : "false", (char *) m_nsCard.homeAddress, (char *) m_nsCard.workAddress, (char *) m_csAddress);
    }
    // City
    if (flags.city) 
    {
      if (MozABPCManager::gUseHomeAddress)
        COPY_FROM_GENERIC(m_csCity, m_nsCard.homeCity)
      else
        COPY_FROM_GENERIC(m_csCity, m_nsCard.workCity)
    }
    // State
    if (flags.state) 
    {
      if (MozABPCManager::gUseHomeAddress)
        COPY_FROM_GENERIC(m_csState, m_nsCard.homeState)
      else
        COPY_FROM_GENERIC(m_csState, m_nsCard.workState)
    }
    // ZipCode
    if (flags.zipCode) 
    {
      if (MozABPCManager::gUseHomeAddress)
        COPY_FROM_GENERIC(m_csZipCode, m_nsCard.homeZipCode)
      else
        COPY_FROM_GENERIC(m_csZipCode, m_nsCard.workZipCode)
    }
    // Country
    if (flags.country)
    {
      if (MozABPCManager::gUseHomeAddress)
        COPY_FROM_GENERIC(m_csCountry, m_nsCard.homeCountry)
      else
        COPY_FROM_GENERIC(m_csCountry, m_nsCard.workCountry)
    }
    // Title
    if (flags.title) COPY_FROM_GENERIC(m_csTitle, m_nsCard.jobTitle)
    // Customs
    if (flags.custom1) COPY_FROM_GENERIC(m_csCustom1, m_nsCard.custom1)
    if (flags.custom2) COPY_FROM_GENERIC(m_csCustom2, m_nsCard.custom2)
    if (flags.custom3) COPY_FROM_GENERIC(m_csCustom3, m_nsCard.custom3)
    if (flags.custom4) COPY_FROM_GENERIC(m_csCustom4, m_nsCard.custom4)
    // Note
    if (flags.note) COPY_FROM_GENERIC(m_csNote, m_nsCard.notes)

    return retval;
}

// this method assigns the phone #s to the Moz card data from Palm Record Phone buffer
void CMozABConduitRecord::AssignPhoneToMozData(DWORD labelID, LPTSTR buf)
{
    switch(labelID) {
    case PHONE_LABEL_WORK:
        m_nsCard.workPhone = buf;
        break;
    case PHONE_LABEL_HOME:
        m_nsCard.homePhone = buf;
        break;
    case PHONE_LABEL_FAX:
        m_nsCard.faxNumber = buf;
        break;
    case PHONE_LABEL_OTHER:
        break;
    case PHONE_LABEL_EMAIL:
        m_nsCard.primaryEmail = buf;
        break;
    case PHONE_LABEL_MAIN:
        break;
    case PHONE_LABEL_PAGER:
        m_nsCard.pagerNumber = buf;
        break;
    case PHONE_LABEL_MOBILE:
        m_nsCard.cellularNumber = buf;
        break;
    }
}


#define COPY_TO_GENERIC(m, f)      iLen = m.length();           \
                                   if (iLen != 0)               \
                                   {                            \
                                        f = 1;                  \
                                        destLen = StripCRs(pBuff, m.c_str(), iLen);  \
                                        pBuff += destLen;       \
                                        dwRecSize += destLen;   \
                                    }                           \


// This method converts records from the Moz type to the CPalmRecord record type.
long CMozABConduitRecord::ConvertToGeneric(CPalmRecord &rec)
{
    long                retval = 0;
    char                *pBuff, *pFlags, *pCompany, *pFirst;
    AddrDBRecordFlags   flags;
    AddrOptionsType     options;
    int                 destLen;
    BYTE                szRawData[MAX_RECORD_SIZE];
    DWORD               dwRecSize = 0;
    int                 iLen=0;


    rec.SetID(m_dwRecordID);

    rec.SetCategory(m_dwCategoryID);

    rec.SetPrivate(m_dwPrivate);

  	rec.SetAttribs(m_dwStatus);

    pBuff = (char*)szRawData;

    // AddrOptionsType
    *(DWORD *)&options = 0;
    options.displayPhone = m_dwDisplayPhone - 1;
    options.phone1 = LOWORD(m_dwPhone1LabelID);
    options.phone2 = LOWORD(m_dwPhone2LabelID);
    options.phone3 = LOWORD(m_dwPhone3LabelID);
    options.phone4 = LOWORD(m_dwPhone4LabelID);
    options.phone5 = LOWORD(m_dwPhone5LabelID);
    *((DWORD *)pBuff) = CPalmRecord::SwapDWordToMotor(*(DWORD *)&options);

    pBuff += sizeof(DWORD);
    dwRecSize += sizeof(DWORD);

    // The flag bits are used to indicate which fields exist
    pFlags = pBuff;
    *(DWORD *)&flags = 0;
    pBuff += sizeof(DWORD);
    dwRecSize += sizeof(DWORD);
    
    // Company Field Offset - Initialize to zero
    pCompany = pBuff;
    *((unsigned char *)pCompany) = 0;
    pBuff += sizeof(unsigned char);
    dwRecSize += sizeof(unsigned char);
    
    // First Field
    pFirst = pBuff;

    // Name
    COPY_TO_GENERIC(m_csName, flags.name)
    // FirstName
    COPY_TO_GENERIC(m_csFirst, flags.firstName)
    // Company
    COPY_TO_GENERIC(m_csCompany, flags.company)
    // Phone1
    COPY_TO_GENERIC(m_csPhone1, flags.phone1)
    // Phone2
    COPY_TO_GENERIC(m_csPhone2, flags.phone2)
    // Phone3
    COPY_TO_GENERIC(m_csPhone3, flags.phone3)
    // Phone4
    COPY_TO_GENERIC(m_csPhone4, flags.phone4)
    // Phone5
    COPY_TO_GENERIC(m_csPhone5, flags.phone5)
    // Address
    COPY_TO_GENERIC(m_csAddress, flags.address)
    // City
    COPY_TO_GENERIC(m_csCity, flags.city)
    // State
    COPY_TO_GENERIC(m_csState, flags.state)
    // ZipCode
    COPY_TO_GENERIC(m_csZipCode, flags.zipCode)
    // Country
    COPY_TO_GENERIC(m_csCountry, flags.country)
    // Title
    COPY_TO_GENERIC(m_csTitle, flags.title)
    // Custom1
    COPY_TO_GENERIC(m_csCustom1, flags.custom1)
    // Custom2
    COPY_TO_GENERIC(m_csCustom2, flags.custom2)
    // Custom3
    COPY_TO_GENERIC(m_csCustom3, flags.custom3)
    // Custom4
    COPY_TO_GENERIC(m_csCustom4, flags.custom4)
    // Note
    COPY_TO_GENERIC(m_csNote, flags.note)

    // Store the AddrDBRecordFlags
    *((DWORD *)pFlags) = CPalmRecord::SwapDWordToMotor(*(DWORD *)&flags);

    
    if (dwRecSize == (sizeof(DWORD) + sizeof(DWORD) + sizeof(unsigned char))){
        // then this is a deleted record because it has no data
        rec.SetDeleted();
    }

    retval = rec.SetRawData(dwRecSize, szRawData);

    return(retval);
}

// this function copies data from the handheld into the passed destination,
// and optionally adds carriage returns to handheld data
long CMozABConduitRecord::CopyFromHHBuffer(TCHAR* pSrc, TCHAR* pDest, int len)
{
    long retval = GEN_ERR_INVALID_POINTER;
    int off=0;

    TCHAR* pCurr;

    if (pDest) {

        // Only add Cr's if newlines are present..
        pCurr = _tcspbrk(pSrc, STR_NEWLINE);
        if(pCurr) 
        {
            while (off < len && *pSrc)
            {
                if (*pSrc == CH_NEWLINE)
                {
                    *pDest = CH_CRETURN;
                    pDest++;
                }
                *pDest = *pSrc;

                pDest++ ; pSrc++;
                off++;
            }
            *pDest = 0;
        }
        else 
        {
            strncpy(pDest, pSrc, len);
            pDest[len] = 0;
        }
        retval = 0;
    }
    return(retval);
}

// this function strips extra carraige returns from PC data
int CMozABConduitRecord::StripCRs(TCHAR* pDest, const TCHAR* pSrc, int len)
{
    int  retval = 0;
    int  off    = 0;
    TCHAR* pCurr; 
    TCHAR* pStart = pDest;

    // See if any cr's are present in the first place.
    pCurr = _tcspbrk(pSrc, STR_CRETURN);
    if (pCurr)
    {
        while (off < len && *pSrc)
        {
            if (*pSrc == CH_CRETURN)
                pSrc++;

            *pDest = *pSrc;

            off++;
            pDest++ ; pSrc++;
        }
        *pDest = 0;

        retval = strlen(pStart) + 1;
    }
    else
    {
        strncpy(pDest, pSrc, len);
        *(pDest + len) = '\0';
        retval = len + 1;
    }    

    return(retval);
}

void CMozABConduitRecord::CleanUpABCOMCardStruct(nsABCOMCardStruct * card)
{
    if(!card)
        return;

    CoTaskMemFree(card->firstName);
    CoTaskMemFree(card->lastName);
    CoTaskMemFree(card->displayName);
    CoTaskMemFree(card->nickName);
    CoTaskMemFree(card->primaryEmail);
    CoTaskMemFree(card->secondEmail);
    CoTaskMemFree(card->workPhone);
    CoTaskMemFree(card->homePhone);
    CoTaskMemFree(card->faxNumber);
    CoTaskMemFree(card->pagerNumber);
    CoTaskMemFree(card->cellularNumber);
    CoTaskMemFree(card->homeAddress);
    CoTaskMemFree(card->homeAddress2);
    CoTaskMemFree(card->homeCity);
    CoTaskMemFree(card->homeState);
    CoTaskMemFree(card->homeZipCode);
    CoTaskMemFree(card->homeCountry);
    CoTaskMemFree(card->workAddress);
    CoTaskMemFree(card->workAddress2);
    CoTaskMemFree(card->workCity);
    CoTaskMemFree(card->workState);
    CoTaskMemFree(card->workZipCode);
    CoTaskMemFree(card->workCountry);
    CoTaskMemFree(card->jobTitle);
    CoTaskMemFree(card->department);
    CoTaskMemFree(card->company);
    CoTaskMemFree(card->webPage1);
    CoTaskMemFree(card->webPage2);
    CoTaskMemFree(card->birthYear);
    CoTaskMemFree(card->birthMonth);
    CoTaskMemFree(card->birthDay);
    CoTaskMemFree(card->custom1);
    CoTaskMemFree(card->custom2);
    CoTaskMemFree(card->custom3);
    CoTaskMemFree(card->custom4);
    CoTaskMemFree(card->notes);
    CoTaskMemFree(card->mailListURI);
}
