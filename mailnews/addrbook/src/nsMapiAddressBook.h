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
#ifndef nsMapiAddressBook_h___
#define nsMapiAddressBook_h___

#include <windows.h>
#include <mapix.h>

#include "nsString.h"
#include "nsVoidArray.h"
#include "nsXPIDLString.h"

struct nsMapiEntry
{
    ULONG     mByteCount ;
    LPENTRYID mEntryId ;
    
    nsMapiEntry(void) ;
    ~nsMapiEntry(void) ;
    nsMapiEntry(ULONG aByteCount, LPENTRYID aEntryId) ;
    
    void Assign(ULONG aByteCount, LPENTRYID aEntryId) ;
    void Dump(void) const ;
} ;

struct nsMapiEntryArray 
{
    nsMapiEntry *mEntries ;
    ULONG      mNbEntries ;
    
    nsMapiEntryArray(void) ;
    ~nsMapiEntryArray(void) ;
    
    const nsMapiEntry& operator [] (int aIndex) const { return mEntries [aIndex] ; }
    void CleanUp(void) ;
} ;

class nsMapiAddressBook
{
public :
    nsMapiAddressBook(void) ;
    ~nsMapiAddressBook(void) ;
    
    // Get the top address books
    BOOL GetFolders(nsMapiEntryArray& aFolders) ;
    // Get a list of entries for cards/mailing lists in a folder/mailing list
    BOOL GetCards(const nsMapiEntry& aParent, LPSRestriction aRestriction, 
        nsMapiEntryArray& aCards) ;
    // Get a list of mailing lists in a folder
    BOOL GetNodes(const nsMapiEntry& aParent, nsMapiEntryArray& aNodes) ;
    // Get the number of cards/mailing lists in a folder/mailing list
    BOOL GetCardsCount(const nsMapiEntry& aParent, ULONG& aNbCards) ;
    // Access last MAPI error
    HRESULT LastError(void) const { return mLastError ; }
    // Get the value of a MAPI property of type string
    BOOL GetPropertyString(const nsMapiEntry& aObject, ULONG aPropertyTag, nsCString& aValue) ;
    // Same as previous, but string is returned as unicode
    BOOL GetPropertyUString(const nsMapiEntry& aObject, ULONG aPropertyTag, nsString& aValue) ;
    // Get multiple string MAPI properties in one call.
    BOOL GetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag, 
                              ULONG aNbProperties, nsStringArray& aValues) ;
    // Get the value of a MAPI property of type SYSTIME
    BOOL GetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                         WORD& aYear, WORD& aMonth, WORD& aDay) ;
    // Get the value of a MAPI property of type LONG
    BOOL GetPropertyLong(const nsMapiEntry& aObject, ULONG aPropertyTag, ULONG& aValue) ;
    // Get the value of a MAPI property of type BIN
    BOOL GetPropertyBin(const nsMapiEntry& aObject, ULONG aPropertyTag, nsMapiEntry& aValue) ;
    // Convert MAPI entries into strings and back
    BOOL StringToEntry(const nsCString& aString, nsMapiEntry& aEntry) const ;
    BOOL EntryToString(const nsMapiEntry& aEntry, nsCString& aString) const ;
    // Tests if a container contains an entry
    BOOL TestOpenEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry) ;
    // Delete an entry in the address book
    BOOL DeleteEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aEntry) ;
    // Set the value of a MAPI property of type string in unicode
    BOOL SetPropertyUString (const nsMapiEntry& aObject, ULONG aPropertyTag, 
                             const PRUnichar *aValue) ;
    // Same as previous, but with a bunch of properties in one call
    BOOL SetPropertiesUString(const nsMapiEntry& aObject, const ULONG *aPropertiesTag,
        ULONG aNbProperties, nsXPIDLString *aValues) ;
    // Set the value of a MAPI property of type SYSTIME
    BOOL SetPropertyDate(const nsMapiEntry& aObject, ULONG aPropertyTag, 
                         WORD aYear, WORD aMonth, WORD aDay) ;
    // Create entry in the address book
    BOOL CreateEntry(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry) ;
    // Create a distribution list in the address book
    BOOL CreateDistList(const nsMapiEntry& aParent, nsMapiEntry& aNewEntry) ;
    // Copy an existing entry in the address book
    BOOL CopyEntry(const nsMapiEntry& aContainer, const nsMapiEntry& aSource, nsMapiEntry& aTarget) ;
    // Get a default address book container
    BOOL GetDefaultContainer(nsMapiEntry& aContainer) ;
    
protected :
    
private :
    LPMAPISESSION mSession ;
    LPADRBOOK mAddressBook ;
    BOOL mLogonDone ;
    BOOL mInitialized ;
    HRESULT mLastError ;
    static ULONG mEntryCounter ;
    
    // Load the MAPI environment
    BOOL Initialize(void) ;
    // Retrieve the contents of a container, with an optional restriction
    BOOL GetContents(const nsMapiEntry& aParent, LPSRestriction aRestriction, 
                     nsMapiEntry **aList, ULONG &aNbElements, ULONG aMapiType) ;
    // Retrieve the values of a set of properties on a MAPI object
    BOOL GetMAPIProperties(const nsMapiEntry& aObject, const ULONG *aPropertyTags, 
                           ULONG aNbProperties,
                           LPSPropValue& aValues, ULONG& aValueCount) ;
    // Set the values of a set of properties on a MAPI object
    BOOL SetMAPIProperties(const nsMapiEntry& aObject, ULONG aNbProperties, 
                           const LPSPropValue& aValues) ;
    
} ;

#endif // nsMapiAddressBook_h___

