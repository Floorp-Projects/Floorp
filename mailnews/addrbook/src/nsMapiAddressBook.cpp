/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc.  Portions created by Sun are
 * Copyright (C) 2001 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Created by Cyrille Moureaux <Cyrille.Moureaux@sun.com>
 */
#define INITGUID
#define USES_IID_IMAPIProp
#define USES_IID_IMAPIContainer
#define USES_IID_IABContainer
#define USES_IID_IMAPITable
#define USES_IID_IDistList

#include "nsMapiAddressBook.h"
#include "nsAbUtils.h"
#include "nsAutoLock.h"

#include "nslog.h"

NS_IMPL_LOG(nsMapiAddressBookLog)

#define PRINTF NS_LOG_PRINTF(nsMapiAddressBookLog)
#define FLUSH  NS_LOG_FLUSH(nsMapiAddressBookLog)

// Small utility to ensure release of all MAPI interfaces
template <class tInterface> struct nsMapiInterfaceWrapper
{
    tInterface mInterface ;

    nsMapiInterfaceWrapper(void) : mInterface(NULL) {}
    ~nsMapiInterfaceWrapper(void) {
        if (mInterface != NULL) { mInterface->Release() ; }
    }
    operator LPUNKNOWN *(void) { return NS_REINTERPRET_CAST(LPUNKNOWN *, &mInterface) ; }
    tInterface operator -> (void) const { return mInterface ; }
    operator tInterface *(void) { return &mInterface ; }
} ;

static HINSTANCE gMapiLibHandle = NULL ;
static LPMAPIINITIALIZE gMAPIInitialize = NULL ;
static LPMAPIUNINITIALIZE gMAPIUninitialize = NULL ;
static LPMAPIALLOCATEBUFFER gMAPIAllocateBuffer = NULL ;
static LPMAPIFREEBUFFER gMAPIFreeBuffer = NULL ;
static LPMAPILOGONEX gMAPILogonEx = NULL ;

// There seems to be a deadlock/auto-destruction issue
// in MAPI when multiple threads perform init/release 
// operations at the same time. So I've put a mutex
// around both the initialize process and the destruction
// one. I just hope the rest of the calls don't need the 
// same protection (MAPI is supposed to be thread-safe).
static PRLock *gMutex = NULL ;

static BOOL LoadMAPIEntryPoints(void)
{
    if (gMapiLibHandle != NULL) { return TRUE ; }
    HINSTANCE libraryHandle = LoadLibrary("MAPI32.DLL") ;

    if (libraryHandle == NULL) { return FALSE ; }
    FARPROC entryPoint = GetProcAddress(libraryHandle, "MAPIGetNetscapeVersion") ;

    if (entryPoint != NULL) {
        FreeLibrary(libraryHandle) ;
        libraryHandle = LoadLibrary("MAPI32BAK.DLL") ;
        if (libraryHandle == NULL) { return FALSE ; }
    }
    gMapiLibHandle = libraryHandle ;
    gMAPIInitialize = NS_REINTERPRET_CAST(LPMAPIINITIALIZE, 
        GetProcAddress(gMapiLibHandle, "MAPIInitialize")) ;
    if (gMAPIInitialize == NULL) { return FALSE ; }
    gMAPIUninitialize = NS_REINTERPRET_CAST(LPMAPIUNINITIALIZE, 
        GetProcAddress(gMapiLibHandle, "MAPIUninitialize")) ;
    if (gMAPIUninitialize == NULL) { return FALSE ; }
    gMAPIAllocateBuffer = NS_REINTERPRET_CAST(LPMAPIALLOCATEBUFFER, 
        GetProcAddress(gMapiLibHandle, "MAPIAllocateBuffer")) ;
    if (gMAPIAllocateBuffer == NULL) { return FALSE ; }
    gMAPIFreeBuffer = NS_REINTERPRET_CAST(LPMAPIFREEBUFFER, 
        GetProcAddress(gMapiLibHandle, "MAPIFreeBuffer")) ;
    if (gMAPIFreeBuffer == NULL) { return FALSE ; }
    gMAPILogonEx = NS_REINTERPRET_CAST(LPMAPILOGONEX, 
        GetProcAddress(gMapiLibHandle, "MAPILogonEx")) ;
    if (gMAPILogonEx == NULL) { return FALSE ; }
    gMutex = PR_NewLock() ;
    if (gMutex == NULL) { return FALSE ; }
    return TRUE ;
}

static void myFreeProws(LPSRowSet aRowset)
{
    if (aRowset == NULL) { return ; }
    ULONG i = 0 ; 

    for (i = 0 ; i < aRowset->cRows ; ++ i) {
        gMAPIFreeBuffer(aRowset->aRow [i].lpProps) ;
    }
    gMAPIFreeBuffer(aRowset) ;
}

static void assignEntryID(LPENTRYID& aTarget, LPENTRYID aSource, ULONG aByteCount)
{
    if (aTarget != NULL) {
        delete [] (NS_REINTERPRET_CAST (LPBYTE, aTarget)) ;
        aTarget = NULL ;
    }
    if (aSource != NULL) {
        aTarget = NS_REINTERPRET_CAST(LPENTRYID, new BYTE [aByteCount]) ;
        memcpy(aTarget, aSource, aByteCount) ;
    }
}

ULONG nsMapiAddressBook::mEntryCounter = 0 ;

MOZ_DECL_CTOR_COUNTER(nsMapiEntry)

nsMapiEntry::nsMapiEntry(void)
: mByteCount(0), mEntryId(NULL)
{
    MOZ_COUNT_CTOR(nsMapiEntry) ;
}

nsMapiEntry::nsMapiEntry(ULONG aByteCount, LPENTRYID aEntryId)
: mByteCount(0), mEntryId(NULL)
{
    Assign(aByteCount, aEntryId) ;
    MOZ_COUNT_CTOR(nsMapiEntry) ;
}

nsMapiEntry::~nsMapiEntry(void)
{
    Assign(0, NULL) ;
    MOZ_COUNT_DTOR(nsMapiEntry) ;
}

void nsMapiEntry::Assign(ULONG aByteCount, LPENTRYID aEntryId)
{
    assignEntryID(mEntryId, aEntryId, aByteCount) ;
    mByteCount = aByteCount ;
}

void nsMapiEntry::Dump(void) const
{
    PRINTF("%d\n", mByteCount) ;
    for (ULONG i = 0 ; i < mByteCount ; ++ i) {
        PRINTF("%02X", (NS_REINTERPRET_CAST(unsigned char *, mEntryId)) [i]) ;
    }
    PRINTF("\n") ;
}

MOZ_DECL_CTOR_COUNTER(nsMapiEntryArray)

nsMapiEntryArray::nsMapiEntryArray(void)
: mEntries(NULL), mNbEntries(0)
{
    MOZ_COUNT_CTOR(nsMapiEntryArray) ;
}

nsMapiEntryArray::~nsMapiEntryArray(void)
{
    if (mEntries) { delete [] mEntries ; }
    MOZ_COUNT_DTOR(nsMapiEntryArray) ;
}

void nsMapiEntryArray::CleanUp(void)
{
    if (mEntries != NULL) { 
        delete [] mEntries ;
        mEntries = NULL ;
        mNbEntries = 0 ;
    }
}

MOZ_DECL_CTOR_COUNTER(nsMapiAddressBook)

nsMapiAddressBook::nsMapiAddressBook(void)
: mSession(NULL), mAddressBook(NULL), mLogonDone(FALSE), 
mInitialized(FALSE), mLastError(S_OK)
{
    MOZ_COUNT_CTOR(nsMapiAddressBook) ;
}

nsMapiAddressBook::~nsMapiAddressBook(void)
{
    nsAutoLock guard(gMutex) ;
    if (mAddressBook != NULL) { mAddressBook->Release() ; }
    if (mSession != NULL) {
        if (mLogonDone) { mSession->Logoff(NULL, 0, 0) ; }
        mSession->Release() ;
    }
    if (mInitialized) { gMAPIUninitialize() ; }
    MOZ_COUNT_DTOR(nsMapiAddressBook) ;
}

BOOL nsMapiAddressBook::GetFolders(nsMapiEntryArray& aFolders)
{
    aFolders.CleanUp() ;
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPABCONT> rootFolder ;
    nsMapiInterfaceWrapper<LPMAPITABLE> folders ;
    ULONG objType = 0 ;
    ULONG rowCount = 0 ;
    SRestriction restriction ;
    SPropTagArray folderColumns ;

    mLastError = mAddressBook->OpenEntry(0, NULL, NULL, 0, &objType, 
                                         rootFolder) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF("Cannot open root %08x.\n", mLastError) ;
        return FALSE ; 
    }
    mLastError = rootFolder->GetHierarchyTable(0, folders) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot get hierarchy %08x.\n", mLastError) ;
        return FALSE ; 
    }
    // We only take into account modifiable containers, 
    // otherwise, we end up with all the directory services...
    restriction.rt = RES_BITMASK ;
    restriction.res.resBitMask.ulPropTag = PR_CONTAINER_FLAGS ;
    restriction.res.resBitMask.relBMR = BMR_NEZ ;
    restriction.res.resBitMask.ulMask = AB_MODIFIABLE ;
    mLastError = folders->Restrict(&restriction, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot restrict table %08x.\n", mLastError) ;
    }
    folderColumns.cValues = 1 ;
    folderColumns.aulPropTag [0] = PR_ENTRYID ;
    mLastError = folders->SetColumns(&folderColumns, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot set columns %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = folders->GetRowCount(0, &rowCount) ;
    if (HR_SUCCEEDED(mLastError)) {
        aFolders.mEntries = new nsMapiEntry [rowCount] ;
        aFolders.mNbEntries = 0 ;
        do {
            LPSRowSet rowSet = NULL ;

            rowCount = 0 ;
            mLastError = folders->QueryRows(1, 0, &rowSet) ;
            if (HR_SUCCEEDED(mLastError)) {
                rowCount = rowSet->cRows ;
                if (rowCount > 0) {
                    nsMapiEntry& current = aFolders.mEntries [aFolders.mNbEntries ++] ;
                    SPropValue& currentValue = rowSet->aRow->lpProps [0] ;
                    
                    current.Assign(currentValue.Value.bin.cb,
                                   NS_REINTERPRET_CAST(LPENTRYID, currentValue.Value.bin.lpb)) ;
                }
                myFreeProws(rowSet) ;
            }
            else {
                PRINTF("Cannot query rows %08x.\n", mLastError) ;
            }
        } while (rowCount > 0) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsMapiAddressBook::GetCards(const nsMapiEntry& aParent, LPSRestriction aRestriction,
                               nsMapiEntryArray& aCards)
{
    aCards.CleanUp() ;
    if (!Initialize()) { return FALSE ; }
    return GetContents(aParent, aRestriction, &aCards.mEntries, aCards.mNbEntries, 0) ;
}
 
BOOL nsMapiAddressBook::GetNodes(const nsMapiEntry& aParent, nsMapiEntryArray& aNodes)
{ 
    aNodes.CleanUp() ;
    if (!Initialize()) { return FALSE ; }
    return GetContents(aParent, NULL, &aNodes.mEntries, aNodes.mNbEntries, MAPI_DISTLIST) ;
}

BOOL nsMapiAddressBook::GetCardsCount(const nsMapiEntry& aParent, ULONG& aNbCards) 
{
    aNbCards = 0 ;
    if (!Initialize()) { return FALSE ; }
    return GetContents(aParent, NULL, NULL, aNbCards, 0) ;
}

BOOL nsMapiAddressBook::GetPropertyString(const nsMapiEntry& aObject,
                                        ULONG aPropertyTag, 
                                        nsCString& aName)
{
    aName.Truncate() ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL) {
        if (PROP_TYPE(values->ulPropTag) == PT_STRING8) {
            aName = values->Value.lpszA ;
        }
        else if (PROP_TYPE(values->ulPropTag) == PT_UNICODE) {
            aName.AssignWithConversion(values->Value.lpszW) ;
        }
    }
    gMAPIFreeBuffer(values) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetPropertyUString(const nsMapiEntry& aObject, 
                                         ULONG aPropertyTag,
                                         nsString& aName)
{
    aName.Truncate() ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL) {
        if (PROP_TYPE(values->ulPropTag) == PT_UNICODE) {
            aName = values->Value.lpszW ;
        }
        else if (PROP_TYPE(values->ulPropTag) == PT_STRING8) {
            aName.AssignWithConversion(values->Value.lpszA) ;
        }
    }
    gMAPIFreeBuffer(values) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertyTags,
                                           ULONG aNbProperties, nsStringArray& aNames)
{
    aNames.Clear() ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, aPropertyTags, aNbProperties, values, valueCount)) { 
        return FALSE ; 
    }
    if (valueCount == aNbProperties && values != NULL) {
        ULONG i = 0 ;

        for (i = 0 ; i < valueCount ; ++ i) {
            if (values [i].ulPropTag == aPropertyTags [i]) {
                if (PROP_TYPE(values [i].ulPropTag) == PT_STRING8) {
                    nsAutoString temp ;

                    temp.AssignWithConversion (values [i].Value.lpszA) ;
                    aNames.AppendString(temp) ;
                }
                else if (PROP_TYPE(values [i].ulPropTag) == PT_UNICODE) {
                    aNames.AppendString(nsAutoString (values [i].Value.lpszW)) ;
                }
                else {
                    aNames.AppendString(nsAutoString((const PRUnichar *) "")) ;
                }
            }
            else {
                aNames.AppendString(nsAutoString((const PRUnichar *) "")) ;
            }
        }
        gMAPIFreeBuffer(values) ;
    }
    return TRUE ;
}

BOOL nsMapiAddressBook::GetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                      WORD& aYear, WORD& aMonth, WORD& aDay)
{
    aYear = 0 ; 
    aMonth = 0 ;
    aDay = 0 ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_SYSTIME) {
        SYSTEMTIME readableTime ;

        if (FileTimeToSystemTime(&values->Value.ft, &readableTime)) {
            aYear = readableTime.wYear ;
            aMonth = readableTime.wMonth ;
            aDay = readableTime.wDay ;
        }
    }
    gMAPIFreeBuffer(values) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetPropertyLong(const nsMapiEntry& aObject, 
                                      ULONG aPropertyTag, 
                                      ULONG& aValue)
{
    aValue = 0 ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_LONG) {
        aValue = values->Value.ul ;
    }
    gMAPIFreeBuffer(values) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetPropertyBin(const nsMapiEntry& aObject, ULONG aPropertyTag, nsMapiEntry& aValue)
{
    aValue.Assign(0, NULL) ;
    LPSPropValue values = NULL ;
    ULONG valueCount = 0 ;

    if (!GetMAPIProperties(aObject, &aPropertyTag, 1, values, valueCount)) { return FALSE ; }
    if (valueCount == 1 && values != NULL && PROP_TYPE(values->ulPropTag) == PT_BINARY) {
        aValue.Assign(values->Value.bin.cb, 
                      NS_REINTERPRET_CAST(LPENTRYID, values->Value.bin.lpb)) ;
    }
    gMAPIFreeBuffer(values) ;
    return TRUE ;
}

static char UnsignedToChar(unsigned char aUnsigned)
{
    if (aUnsigned < 0xA) { return '0' + aUnsigned ; }
    return 'A' + aUnsigned - 0xA ;
}

static char kBase64Encoding [] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-." ;
static const int kARank = 0 ;
static const int kaRank = 26 ;
static const int k0Rank = 52 ;
static const unsigned char kMinusRank = 62 ;
static const unsigned char kDotRank = 63 ;

static void UnsignedToBase64(unsigned char *& aUnsigned, 
                             ULONG aNbUnsigned, nsCString& aString)
{
    if (aNbUnsigned > 0) {
        unsigned char remain0 = (*aUnsigned & 0x03) << 4 ;

        aString.Append(kBase64Encoding [(*aUnsigned >> 2) & 0x3F]) ;
        ++ aUnsigned ;
        if (aNbUnsigned > 1) {
            unsigned char remain1 = (*aUnsigned & 0x0F) << 2 ;

            aString.Append(kBase64Encoding [remain0 | ((*aUnsigned >> 4) & 0x0F)]) ;
            ++ aUnsigned ;
            if (aNbUnsigned > 2) {
                aString.Append(kBase64Encoding [remain1 | ((*aUnsigned >> 6) & 0x03)]) ;
                aString.Append(kBase64Encoding [*aUnsigned & 0x3F]) ;
                ++ aUnsigned ;
            }
            else {
                aString.Append(kBase64Encoding [remain1]) ;
            }
        }
        else {
            aString.Append(kBase64Encoding [remain0]) ;
        }
    }
}

BOOL nsMapiAddressBook::EntryToString(const nsMapiEntry& aEntry, nsCString& aString) const
{
    aString.Truncate() ;
    ULONG i = 0 ;
    unsigned char *currentSource = NS_REINTERPRET_CAST(unsigned char *, aEntry.mEntryId) ;

    for (i = aEntry.mByteCount ; i >= 3 ; i -= 3) {
        UnsignedToBase64(currentSource, 3, aString) ;
    }
    UnsignedToBase64(currentSource, i, aString) ;
    return HR_SUCCEEDED(mLastError) ;
}

static unsigned char CharToUnsigned(char aChar)
{
    if (aChar >= '0' && aChar <= '9') { return NS_STATIC_CAST(unsigned char, aChar) - '0' ; }
    return NS_STATIC_CAST(unsigned char, aChar) - 'A' + 0xA ;
}

// This function must return the rank in kBase64Encoding of the 
// character provided.
static unsigned char Base64To6Bits(char aBase64)
{
    if (aBase64 >= 'A' && aBase64 <= 'Z') { 
        return NS_STATIC_CAST(unsigned char, aBase64 - 'A' + kARank) ; 
    }
    if (aBase64 >= 'a' && aBase64 <= 'z') { 
        return NS_STATIC_CAST(unsigned char, aBase64 - 'a' + kaRank) ; 
    }
    if (aBase64 >= '0' && aBase64 <= '9') {
        return NS_STATIC_CAST(unsigned char, aBase64 - '0' + k0Rank) ;
    }
    if (aBase64 == '-') { return kMinusRank ; }
    if (aBase64 == '.') { return kDotRank ; }
    return 0 ;
}

static void Base64ToUnsigned(const char *& aBase64, PRUint32 aNbBase64, 
                             unsigned char *&aUnsigned)
{
    // By design of the encoding, we must have at least two characters to use
    if (aNbBase64 > 1) {
        unsigned char first6Bits = Base64To6Bits(*aBase64 ++) ;
        unsigned char second6Bits = Base64To6Bits(*aBase64 ++) ;

        *aUnsigned = first6Bits << 2 ;
        *aUnsigned |= second6Bits >> 4 ;
        ++ aUnsigned ;
        if (aNbBase64 > 2) {
            unsigned char third6Bits = Base64To6Bits(*aBase64 ++) ;

            *aUnsigned = second6Bits << 4 ;
            *aUnsigned |= third6Bits >> 2 ;
            ++ aUnsigned ;
            if (aNbBase64 > 3) {
                unsigned char fourth6Bits = Base64To6Bits(*aBase64 ++) ;

                *aUnsigned = third6Bits << 6 ;
                *aUnsigned |= fourth6Bits ;
                ++ aUnsigned ;
            }
        }
    }
}

BOOL nsMapiAddressBook::StringToEntry(const nsCString& aString, nsMapiEntry& aEntry) const
{
    aEntry.Assign(0, NULL) ;
    ULONG byteCount = aString.Length() / 4 * 3 ;

    if (aString.Length() & 0x03 != 0) {
        byteCount += (aString.Length() & 0x03) - 1 ;
    }
    const char *currentSource = aString.get() ;
    unsigned char *currentTarget = new unsigned char [byteCount] ;
    PRUint32 i = 0 ;

    aEntry.mByteCount = byteCount ;
    aEntry.mEntryId = NS_REINTERPRET_CAST(LPENTRYID, currentTarget) ;
    for (i = aString.Length() ; i >= 4 ; i -= 4) {
        Base64ToUnsigned(currentSource, 4, currentTarget) ;
    }
    Base64ToUnsigned(currentSource, i, currentTarget) ;
    return HR_SUCCEEDED(mLastError) ;
}

// This function, supposedly indicating whether a particular entry was
// in a particular container, doesn't seem to work very well (has
// a tendancy to return TRUE even if we're talking to different containers...).
BOOL nsMapiAddressBook::TestOpenEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry)
{
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPMAPICONTAINER> container ;
    nsMapiInterfaceWrapper<LPMAPIPROP> subObject ;
    ULONG objType = 0 ;
    
    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IMAPIContainer, 0, &objType, 
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot open container %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = container->OpenEntry(aEntry.mByteCount, aEntry.mEntryId,
                                      NULL, 0, &objType, subObject) ;
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsMapiAddressBook::DeleteEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry)
{
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;
    SBinary entry ;
    SBinaryArray entryArray ;

    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType, 
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot open container %08x.\n", mLastError) ;
        return FALSE ;
    }
    entry.cb = aEntry.mByteCount ;
    entry.lpb = NS_REINTERPRET_CAST(LPBYTE, aEntry.mEntryId) ;
    entryArray.cValues = 1 ;
    entryArray.lpbin = &entry ;
    mLastError = container->DeleteEntries(&entryArray, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot delete entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    return TRUE ;
}

BOOL nsMapiAddressBook::SetPropertyUString(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                         const PRUnichar *aValue)
{
    SPropValue value ;
    nsCAutoString alternativeValue ;

    value.ulPropTag = aPropertyTag ;
    if (PROP_TYPE(aPropertyTag) == PT_UNICODE) {
        value.Value.lpszW = NS_CONST_CAST(WORD *, aValue) ;
    }
    else if (PROP_TYPE(aPropertyTag) == PT_STRING8) {
        alternativeValue.AssignWithConversion(aValue) ;
        value.Value.lpszA = NS_CONST_CAST(char *, alternativeValue.get()) ;
    }
    else {
        PRINTF("Property %08x is not a string.\n", aPropertyTag) ;
        return TRUE ;
    }
    return SetMAPIProperties(aObject, 1, &value) ;
}

BOOL nsMapiAddressBook::SetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag,
                                           ULONG aNbProperties, nsXPIDLString *aValues) 
{
    LPSPropValue values = new SPropValue [aNbProperties] ;
    ULONG i = 0 ;
    ULONG currentValue = 0 ;
    nsCAutoString alternativeValue ;
    BOOL retCode = TRUE ;

    for (i = 0 ; i < aNbProperties ; ++ i) {
        values [currentValue].ulPropTag = aPropertiesTag [i] ;
        if (PROP_TYPE(aPropertiesTag [i]) == PT_UNICODE) {
            values [currentValue ++].Value.lpszW = NS_CONST_CAST(WORD *, aValues [i].get()) ;
        }
        else if (PROP_TYPE(aPropertiesTag [i]) == PT_STRING8) {
            alternativeValue.AssignWithConversion(aValues [i].get()) ;
            values [currentValue ++].Value.lpszA = nsCRT::strdup(alternativeValue.get()) ;
        }
    }
    retCode = SetMAPIProperties(aObject, currentValue, values) ;
    for (i = 0 ; i < currentValue ; ++ i) {
        if (PROP_TYPE(aPropertiesTag [i]) == PT_STRING8) {
            nsCRT::free(values [i].Value.lpszA) ;
        }
    }
    delete [] values ;
    return retCode ;
}

BOOL nsMapiAddressBook::SetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                                      WORD aYear, WORD aMonth, WORD aDay)
{
    SPropValue value ;
    
    value.ulPropTag = aPropertyTag ;
    if (PROP_TYPE(aPropertyTag) == PT_SYSTIME) {
        SYSTEMTIME readableTime ;

        readableTime.wYear = aYear ;
        readableTime.wMonth = aMonth ;
        readableTime.wDay = aDay ;
        readableTime.wDayOfWeek = 0 ;
        readableTime.wHour = 0 ;
        readableTime.wMinute = 0 ;
        readableTime.wSecond = 0 ;
        readableTime.wMilliseconds = 0 ;
        if (SystemTimeToFileTime(&readableTime, &value.Value.ft)) {
            return SetMAPIProperties(aObject, 1, &value) ;
        }
        return TRUE ;
    }
    return FALSE ;
}

BOOL nsMapiAddressBook::CreateEntry(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry)
{
    //PRINTF("IN CreateEntry.\n") ;
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF("Cannot open container %08x.\n", mLastError) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;

    property.cValues = 1 ;
    property.aulPropTag [0] = PR_DEF_CREATE_MAILUSER ;
    mLastError = container->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF("Cannot obtain template %08x.\n", mLastError) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(value->Value.bin.cb, 
                                        NS_REINTERPRET_CAST(LPENTRYID, value->Value.bin.lpb),
                                        CREATE_CHECK_DUP_LOOSE,
                                        newEntry) ;
    gMAPIFreeBuffer(value) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot create new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    SPropValue displayName ;
    LPSPropProblemArray problems = NULL ;
    nsAutoString tempName ;

    displayName.ulPropTag = PR_DISPLAY_NAME_W ;
    tempName.AssignWithConversion("__MailUser__") ;
    tempName.AppendInt(mEntryCounter ++) ;
    displayName.Value.lpszW = NS_CONST_CAST(WORD *, tempName.get()) ;
    mLastError = newEntry->SetProps(1, &displayName, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot set temporary name %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot commit new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF("Cannot get entry id %08x.\n", mLastError) ;
        return FALSE ;
    }
    aNewEntry.Assign(value->Value.bin.cb, NS_REINTERPRET_CAST(LPENTRYID, value->Value.bin.lpb)) ;
    gMAPIFreeBuffer(value) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::CreateDistList(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry)
{
    //PRINTF("IN CreateDistList.\n") ;
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot open container %08x.\n", mLastError) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;

    property.cValues = 1 ;
    property.aulPropTag [0] = PR_DEF_CREATE_DL ;
    mLastError = container->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF("Cannot obtain template %08x.\n", mLastError) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(value->Value.bin.cb, 
                                        NS_REINTERPRET_CAST(LPENTRYID, value->Value.bin.lpb),
                                        CREATE_CHECK_DUP_LOOSE,
                                        newEntry) ;
    gMAPIFreeBuffer(value) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot create new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    SPropValue displayName ;
    LPSPropProblemArray problems = NULL ;
    nsAutoString tempName ;

    displayName.ulPropTag = PR_DISPLAY_NAME_W ;
    tempName.AssignWithConversion("__MailList__") ;
    tempName.AppendInt(mEntryCounter ++) ;
    displayName.Value.lpszW = NS_CONST_CAST(WORD *, tempName.get()) ;
    mLastError = newEntry->SetProps(1, &displayName, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot set temporary name %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot commit new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF("Cannot get entry id %08x.\n", mLastError) ;
        return FALSE ;
    }
    aNewEntry.Assign(value->Value.bin.cb, 
                     NS_REINTERPRET_CAST(LPENTRYID, value->Value.bin.lpb)) ;
    gMAPIFreeBuffer(value) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::CopyEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aSource,
                                nsMapiEntry& aTarget)
{
    //PRINTF("IN CopyEntry.\n") ;
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPABCONT> container ;
    ULONG objType = 0 ;

    mLastError = mAddressBook->OpenEntry(aContainer.mByteCount, aContainer.mEntryId,
                                         &IID_IABContainer, MAPI_MODIFY, &objType,
                                         container) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF("Cannot open container %08x.\n", mLastError) ;
        return FALSE ;
    }
    nsMapiInterfaceWrapper<LPMAPIPROP> newEntry ;

    mLastError = container->CreateEntry(aSource.mByteCount, aSource.mEntryId,
                                        CREATE_CHECK_DUP_LOOSE, newEntry) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot create new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = newEntry->SaveChanges(KEEP_OPEN_READONLY) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot commit new entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    SPropTagArray property ;
    LPSPropValue value = NULL ;
    ULONG valueCount = 0 ;
    
    property.cValues = 1 ;
    property.aulPropTag [0] = PR_ENTRYID ;
    mLastError = newEntry->GetProps(&property, 0, &valueCount, &value) ;
    if (HR_FAILED(mLastError) || valueCount != 1) {
        PRINTF("Cannot get entry id %08x.\n", mLastError) ;
        return FALSE ;
    }
    aTarget.Assign(value->Value.bin.cb, 
                   NS_REINTERPRET_CAST(LPENTRYID, value->Value.bin.lpb)) ;
    gMAPIFreeBuffer(value) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetDefaultContainer(nsMapiEntry& aContainer)
{
    if (!Initialize()) { return FALSE ; }
    LPENTRYID entryId = NULL ; 
    ULONG byteCount = 0 ;

    mLastError = mAddressBook->GetPAB(&byteCount, &entryId) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot get PAB %08x.\n", mLastError) ;
        return FALSE ;
    }
    aContainer.Assign(byteCount, entryId) ;
    gMAPIFreeBuffer(entryId) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::Initialize(void)
{
    if (mAddressBook != NULL) { return TRUE ; }
    if (!LoadMAPIEntryPoints()) {
        PRINTF("Cannot load library.\n") ;
        return FALSE ;
    }
    nsAutoLock guard(gMutex) ;
    MAPIINIT_0 mapiInit = { MAPI_INIT_VERSION, MAPI_MULTITHREAD_NOTIFICATIONS } ;

    mLastError = gMAPIInitialize(&mapiInit) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot initialize MAPI %08x.\n", mLastError) ;
        return FALSE ; 
    }
    mInitialized = TRUE ;
    mLastError = gMAPILogonEx(0, NULL, NULL,
                              MAPI_NO_MAIL | 
                              MAPI_USE_DEFAULT | 
                              MAPI_EXTENDED | 
                              MAPI_NEW_SESSION,
                              &mSession) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot logon to MAPI %08x.\n", mLastError) ;
        return FALSE ; 
    }
    mLogonDone = TRUE ;
    mLastError = mSession->OpenAddressBook(0, NULL, 0, &mAddressBook) ;
    return HR_SUCCEEDED(mLastError) ;
}

enum
{
    ContentsColumnEntryId = 0,
    ContentsColumnObjectType,
    ContentsColumnsSize
} ;

static const SizedSPropTagArray(ContentsColumnsSize, ContentsColumns) =
{
    ContentsColumnsSize,
    {
        PR_ENTRYID,
        PR_OBJECT_TYPE
    }
} ;

BOOL nsMapiAddressBook::GetContents(const nsMapiEntry& aParent, LPSRestriction aRestriction,
                                    nsMapiEntry **aList, ULONG& aNbElements, ULONG aMapiType)
{
    if (aList != NULL) { *aList = NULL ; }
    aNbElements = 0 ;
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPMAPICONTAINER> parent ;
    nsMapiInterfaceWrapper<LPMAPITABLE> contents ;
    ULONG objType = 0 ;
    ULONG rowCount = 0 ;

    mLastError = mAddressBook->OpenEntry(aParent.mByteCount, aParent.mEntryId, 
                                         &IID_IMAPIContainer, 0, &objType, 
                                         parent) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot open parent %08x.\n", mLastError) ;
        return FALSE ; 
    }
    mLastError = parent->GetContentsTable(0, contents) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot get contents %08x.\n", mLastError) ;
        return FALSE ; 
    }
    if (aRestriction != NULL) {
        mLastError = contents->Restrict(aRestriction, 0) ;
        if (HR_FAILED(mLastError)) {
            PRINTF("Cannot set restriction %08x.\n", mLastError) ;
            return FALSE ;
        }
    }
    mLastError = contents->SetColumns((LPSPropTagArray) &ContentsColumns, 0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot set columns %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = contents->GetRowCount(0, &rowCount) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot get result count %08x.\n", mLastError) ;
        return FALSE ;
    }
    if (aList != NULL) { *aList = new nsMapiEntry [rowCount] ; }
    aNbElements = 0 ;
    do {
        LPSRowSet rowSet = NULL ;
        
        rowCount = 0 ;
        mLastError = contents->QueryRows(1, 0, &rowSet) ;
        if (HR_FAILED(mLastError)) {
            PRINTF("Cannot query rows %08x.\n", mLastError) ;
            return FALSE ;
        }
        rowCount = rowSet->cRows ;
        if (rowCount > 0 &&
            (aMapiType == 0 ||
            rowSet->aRow->lpProps[ContentsColumnObjectType].Value.ul == aMapiType)) {
            if (aList != NULL) {
                nsMapiEntry& current = (*aList) [aNbElements ++] ;
                SPropValue& currentValue = rowSet->aRow->lpProps[ContentsColumnEntryId] ;
                
                current.Assign(currentValue.Value.bin.cb,
                    NS_REINTERPRET_CAST(LPENTRYID, currentValue.Value.bin.lpb)) ;
            }
            else { ++ aNbElements ; }
        }
        myFreeProws(rowSet) ;
    } while (rowCount > 0) ;
    return TRUE ;
}

BOOL nsMapiAddressBook::GetMAPIProperties(const nsMapiEntry& aObject, const ULONG *aPropertyTags, 
                                        ULONG aNbProperties, LPSPropValue& aValue, 
                                        ULONG& aValueCount)
{
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPMAPIPROP> object ;
    ULONG objType = 0 ;
    LPSPropTagArray properties = NULL ;
    ULONG i = 0 ;
    
    mLastError = mAddressBook->OpenEntry(aObject.mByteCount, aObject.mEntryId,
                                         &IID_IMAPIProp, 0, &objType, 
                                         object) ;
    if (HR_FAILED(mLastError)) { 
        PRINTF("Cannot open entry %08x.\n", mLastError) ;
        return FALSE ; 
    }
    gMAPIAllocateBuffer(CbNewSPropTagArray(aNbProperties), 
                        NS_REINTERPRET_CAST(void **, &properties)) ;
    properties->cValues = aNbProperties ;
    for (i = 0 ; i < aNbProperties ; ++ i) {
        properties->aulPropTag [i] = aPropertyTags [i] ;
    }
    mLastError = object->GetProps(properties, 0, &aValueCount, &aValue) ;
    gMAPIFreeBuffer(properties) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot get props %08x.\n", mLastError) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}

BOOL nsMapiAddressBook::SetMAPIProperties(const nsMapiEntry& aObject, ULONG aNbProperties, 
                                        const LPSPropValue& aValues)
{
    if (!Initialize()) { return FALSE ; }
    nsMapiInterfaceWrapper<LPMAPIPROP> object ;
    ULONG objType = 0 ;
    LPSPropProblemArray problems = NULL ;

    mLastError = mAddressBook->OpenEntry(aObject.mByteCount, aObject.mEntryId,
                                         &IID_IMAPIProp, MAPI_MODIFY, &objType, 
                                         object) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot open entry %08x.\n", mLastError) ;
        return FALSE ;
    }
    mLastError = object->SetProps(aNbProperties, aValues, &problems) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot update the object %08x.\n", mLastError) ;
        return FALSE ;
    }
    if (problems != NULL) {
        for (ULONG i = 0 ; i < problems->cProblem ; ++ i) {
            PRINTF("Problem %d: index %d code %08x.\n", i, 
                problems->aProblem [i].ulIndex, 
                problems->aProblem [i].scode) ;
        }
    }
    mLastError = object->SaveChanges(0) ;
    if (HR_FAILED(mLastError)) {
        PRINTF("Cannot commit changes %08x.\n", mLastError) ;
    }
    return HR_SUCCEEDED(mLastError) ;
}





