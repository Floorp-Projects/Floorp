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
#ifndef __NABAPI_H__
#define __NABAPI_H__

/********************************************************
 * Structures, Type Definitions & Return Codes
 ********************************************************/
  
#ifndef _UINT_16
typedef unsigned short  UINT_16;
#endif
#ifndef _UINT_32
typedef unsigned long   UINT_32;
#endif
#ifndef _INT_16
typedef short           INT_16;
#endif
#ifndef _INT_32
typedef long            INT_32;
#endif

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE (1)
#endif
#ifndef NULL
#define NULL (0L)
#endif

typedef unsigned char   NABBool;
typedef long            NABError;
typedef short           NABReason;
typedef long            NABAddrBookID;
typedef long            NABUserID;
typedef long            NABFieldMask;
typedef unsigned long   NABUpdateTime;
typedef unsigned long   NABConnectionID;

typedef enum {
  NAB_SUCCESS,        /* The API call succeeded. */
  NAB_FAILURE,        /* This is a generic API failure. */
  NAB_INVALID_CONNID, /* Invalid Connection ID for request */
  NAB_NOT_OPEN,       /* This will be returned when a call is made to the API without NAB_Open() being called first. */
  NAB_NOT_FOUND,      /* The requested information was not found. */
  NAB_PERMISSION_DENIED, /* User does not have write permission to perform the operation */
  NAB_DISK_FULL,      /* Disk full condition encountered attempting to perform the operation */
  NAB_INVALID_NAME,   /* The name specified for the operation is invalid */
  NAB_INVALID_ABOOK,  /* The address book specified is invalid */
  NAB_FILE_NOT_FOUND, /* The disk file for the operation was not found */
  NAB_ALREADY_EXISTS, /* The user entry already exists in the address book being updated */
  NAB_MAXCON_EXCEEDED,/* The maximum number of API connections has been exceeded */
  NAB_MEMORY_FAILURE, /* Memory condition, such as malloc() failures */
  NAB_FILE_ERROR,     /* Error on file creation */
  NAB_INVALID_ENTRY,  /* Invalid entry for operation */
  NAB_MULTIPLE_USERS_FOUND, /* More than one person found with current search criteria */
  NAB_INVALID_SEARCH_ATTRIB /* Invalid search criteria */
} NAB_ERROR_CODE_TYPES;
  
typedef struct   
{
  char     *description; // Description of address book - MUST BE UNIQUE
  char     *fileName;    // The file name of the address book
} NABAddrBookDescType;

// Used to separate return values for the LDIF formatted lines
#define   NAB_CRLF            "\r\n"

/********************************************************
 *  Initialization Calls
 ********************************************************/
/*
 * NAB_Open() will start the connection to the API. It will initialize any necessary 
 * internal variables and return the major and minor version as defined here. This
 * will allow client applications to verify the version they are running against.  
 * 
 * Return Value:  
 *    NAB_SUCCESS - The API was opened successfully and is ready to process other API calls.  
 *    NAB_FAILURE - The API open operation failed.   
 *    NAB_MAXCON_EXCEEDED - The maximum number of connections to the API has been exceeded.
 *
 * Parameters:  
 *    id - the pointer to a connection ID that will be filled out by the open call. This needs 
 *         to be used in other API calls. ZERO is invalid.
 *    majorVerNumber - this is a pointer to a UNIT_16 for the MAJOR version number of the API  
 *    minorVerNumber - this is a pointer to a UNIT_16 for the MINOR version number of the API
 */
NABError  NAB_Open(NABConnectionID *id, UINT_16 *majorVerNumber, UINT_16 *minorVerNumber);  

/*
 * This closes the connection to the API. Any subsequent call to the NABAPI will fail with 
 * an NAB_NOT_OPEN return code.   
 * 
 * Return Value:  
 *    NAB_SUCCESS - The API was opened successfully and is ready to process other API calls.  
 *    NAB_FAILURE - The API open operation failed.   
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 */
NABError      NAB_Close(NABConnectionID id);  

/********************************************************
 * Address Book Information Calls
 ********************************************************/

/*
 * This call will return a list of pointers to the list of available address 
 * books in NABAddrBookDescType format. The memory for this array will be allocated 
 * by the NABAPI and the caller is responsible for freeing the memory with a call 
 * to the NAB_FreeMemory() function call. NOTE: The call to NAB_FreeMemory() must be
 * made for each entry in the returned array and not just a pointer to the array itself. 
 * The number of address books found will also be returned in the abookCount
 * argument to the call. If the returned list is NULL or the abookCount value is 
 * less than or equal to ZERO, no address books were found.  
 * 
 * Return Value:  
 *    NAB_SUCCESS - The operation succeeded and the next entry was found. 
 *    NAB_FAILURE - General failure getting the next address book entry 
 *    NAB_NOT_FOUND - No address books were found
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first. 
 *    NAB_INVALID_CONNID - Invalid connection ID  
 *
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    abookCount - a pointer to a UINT_32 where the API will store the number of 
 *    address books found and returned to the caller
 *    aBooks -  A pointer to an array of NABAddrBookDescType structures. If no entries are 
 *    available, a NULL list is returned to the caller.  
 */
NABError NAB_GetAddressBookList(NABConnectionID id, UINT_32 *abookCount, NABAddrBookDescType *aBooks[]);  

/*
 * This call will return the default address book for the client. If the 
 * returned value is NULL, no default address book was found. The memory 
 * for this return value will be allocated by the ABAPI and the caller is 
 * responsible for freeing the memory with a call to the NAB_FreeMemory() 
 * function call.  
 *
 * Return Value:  
 *    NAB_SUCCESS - The operation succeeded and the next entry was found. 
 *    NAB_FAILURE - General failure getting the next address book entry 
 *    NAB_NOT_FOUND - No address books were found
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first. 
 *    NAB_INVALID_CONNID - Invalid connection ID  
 *
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    aBook - A pointer to an NABAddrBookDescType structure. If no default address 
 *    book is found, NULL returned to the caller.  
 */
NABError NAB_GetDefaultAddressBook(NABConnectionID id, NABAddrBookDescType **aBook);  

/* 
 * This call will create a local/personal address book with the name given.   
 * 
 * Return Value:  
 *    NAB_SUCCESS - The personal address book was created successfully.  
 *    NAB_FAILURE - General failure to create the address book  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to create an address book  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to create address book  
 *    NAB_INVALID_NAME - The name for the address book is invalid or already exists   
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBookName - the description name for the address book to be created  
 *    addrBook - a pointer to the addrBook entry that will be populated by the API for the newly created API - if 
 *               the create call fails, this pointer is set to NULL -
 *    [memory allocated by API and must be freed by a call to NAB_FreeMemory()]
 */
NABError NAB_CreatePersonalAddressBook(NABConnectionID id, char *addrBookName, NABAddrBookDescType **addrBook); 

/********************************************************
 * Memory Handling Calls
 ********************************************************/

/*
 * This call frees memory allocated by the NABAPI.   
 *
 * Return Value:  
 *    NAB_SUCCESS - The memory pointed to by ptr was freed successfully.  
 *    NAB_FAILURE - The ptr parameter was NULL or was unable to be free'd  
 *
 * Parameters:  
 *    ptr - this is a pointer to the memory allocated by the NABAPI. 
 */
NABError NAB_FreeMemory(void *ptr);   

/********************************************************
 * Utility Calls
 ********************************************************/

/*
 * This will save the address book specified by the addrBook parameter into a 
 * table formatted, HTML file. The fields to be saved should be passed into 
 * the call via the ldifFields parameter.  
 *
 * Return Value:  
 *    NAB_SUCCESS - The address book was saved to an HTML file.  
 *    NAB_FAILURE - General failure to create save the address book  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API 
 *                   without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to create 
 *                            an address book  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to create address book  
 *    NAB_INVALID_NAME - The name for the address book is invalid or already exists   
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *    
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular 
 *               address book for the operation  
 *    fileName - the name for the output HTML file on disk  
 */
NABError NAB_SaveAddressBookToHTML(NABConnectionID id, NABAddrBookDescType *addrBook, 
                                   char *fileName);  

/*
 * This call will import an LDIF formatted file from a disk file to the address 
 * book specified by the addrBook parameter.  
 *
 * Return Value:
 *    NAB_SUCCESS - The import operation succeeded.  
 *    NAB_FAILURE - General failure to during the import operation  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without 
 *                   NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to the address 
 *                            book or to read the import disk file  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to import the address book  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *    NAB_FILE_NOT_FOUND  - The disk file was not found  
 *    
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address 
 *               book for the operation  
 *    fileName - the disk file name for the import operation 
 *    deleteFileWhenFinished - if this is true, the temp file will be deleted upon exit
 */
NABError NAB_ImportLDIFFile(NABConnectionID id, NABAddrBookDescType *addrBook, 
                            char *fileName, NABBool deleteFileWhenFinished);  

/*
 * This call will export the contents of an address book specified by the 
 * addrBook parameter to an LDIF formatted disk file.  
 *
 * Return Value:  
 *    NAB_SUCCESS - The export operation succeeded.  
 *    NAB_FAILURE - General failure to during the export operation  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API 
 *                   without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission for the output 
 *                            disk file  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to export the 
 *                    address book  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 * 
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address book 
 *               for the operation  
 *    fileName - the disk file name for the export operation
 */
NABError NAB_ExportLDIFFile(NABConnectionID id, NABAddrBookDescType *addrBook, char *fileName);  

/*
 * This is a convenience call that will format an LDIF string for use with calls such 
 * as NAB_InsertAddressBookEntry() and NAB_UpdateAddressBookEntry(). The memory for 
 * the line is allocated by the NAB_FormatLDIFLine() and must be freed by the caller
 * with the NAB_FreeMemory() call.
 * 
 * Return Value: 
 *   NULL - the call failed for one of the following reasons:
 *
 *          - There were not valid parameters passed into the call
 *          - The memory allocation failed
 *
 *   char * - a pointer to the allocated string
 *
 * Parameters: 
 *   firstName (sn) - persons first name
 *   lastName (givenname) -  last name  
 *   generalNotes (description) - general notes for entry
 *   city (locality) - City 
 *   state (st) - State 
 *   email (mail) - Email Address 
 *   title (title) - Job Title or position
 *   addrLine1 (postOfficeBox) - Address line #1 
 *   addrLine2 (streetaddress) - Address line #2 
 *   zipCode (postalcode) - Zip Code 
 *   country (countryname) - Country Name  
 *   businessPhone (telephonenumber) - Business Telephone Number 
 *   faxPhone (facsimiletelephonenumber) - FAX Number 
 *   homePhone (homephone) - Home Phone Number 
 *   organization (o) - Organization or company name 
 *   nickname (xmozillanickname) - Communicator Nickname 
 *   useHTML (xmozillausehtmlmail) - Preference for HTML composition
 */
char *NAB_FormatLDIFLine(char *firstName, 
                         char *lastName, 
                         char *generalNotes, 
                         char *city, 
                         char *state, 
                         char *email, 
                         char *title, 
                         char *addrLine1, 
                         char *addrLine2, 
                         char *zipCode, 
                         char *country, 
                         char *businessPhone, 
                         char *faxPhone, 
                         char *homePhone, 
                         char *organization, 
                         char *nickname, 
                         char *useHTML); 

/********************************************************
 * Individual Entry Calls
 ********************************************************/
                                              
/*
 * This call will get the User ID for the first person in the specified address 
 * book and it will also return the attributes for the user in the ldifEntry field. The memory for
 * this field must be freed by a call to NAB_FreeMemory(). The addrBook argument is 
 * passed back via the calls that return NABAddrBookDescType structures.
 * The call will also return the time this entry was last updated via the NABUpdateTime 
 * paramenter. The memory for this structure should be allocated by the
 * calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND 
 * if no entries are found. If a user is found, ID for the user returned is
 * stored in the userID argument.  
 *
 * Return Value:  
 *    NAB_SUCCESS - The operation succeeded and an entry was found.  
 *    NAB_FAILURE - General failure getting the first address book entry  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without 
 *                   NAB_Open() being called first.  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 * 
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular 
 *               address book for the operation  
 *    userID - a pointer to a NABUserID value that will be filled out by the API with the ID 
 *             for the found user  
 *    ldifEntry - the ldif formatted information for the found user [memory allocated by API 
 *                and must be freed by a call to NAB_FreeMemory()]  
 *    time - a pointer to the the NABUpdateTime value for the time of last update for the individual entry 
 */
NABError NAB_GetFirstAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, NABUserID *userID, 
                                      char **ldifEntry, NABUpdateTime *updTime);  
        
/* 
 * This call will get the next address book entry from the specified address book and it will also 
 * return the attributes for the user in the ldifEntry field. The memory for
 * this field must be freed by a call to NAB_FreeMemory(). The addrBook argument is passed back 
 * via the calls that return NABAddrBookDescType structures. The call will also return the time this 
 * entry was last updated via the NABUpdateTime paramenter. The memory for this structure should be allocated by the
 * calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND if the 
 * final entry has already been returned. If a user is found, ID for the user returned is stored 
 * in the userID argument.  
 * 
 * Return Value:  
 *    NAB_SUCCESS - The operation succeeded and the next entry was found.  
 *    NAB_FAILURE - General failure getting the next address book entry  
 *    NAB_NOT_FOUND - The final entry was already returned  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_INVALID_ABOOK - NAB_GetFirstAddressBookEntry() probably wasn't called  
 *
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    userID - a pointer to a NABUserID value that will be filled out by the API with the ID for the found user  
 *    ldifEntry - the ldif formatted information for the found user [memory allocated by API and must 
 *                be freed by a call to NAB_FreeMemory()]  
 *    time - a pointer to the the NABUpdateTime value for the time of last update for the individual entry
 */ 
NABError NAB_GetNextAddressBookEntry(NABConnectionID id, NABUserID *userID, 
                                     char **ldifEntry, NABUpdateTime *updTime);  

/* 
 * This call will perform a query on the specified ldif attribute pair. The memory for the ldifEntry 
 * will be allocated by the API and must be freed by a call to NAB_FreeMemory(). The addrBook argument 
 * is passed back via the calls that return NABAddrBookDescType structures. The call will also return the time
 * this entry was last updated via the NABUpdateTime paramenter. The memory for this structure should be 
 * allocated by the calling application. It will return NAB_SUCCESS if an entry was found or NAB_NOT_FOUND 
 * if the search failed.  
 * 
 * Return Value:  
 *    NAB_SUCCESS - The operation succeeded and the requested entry was found.  
 *    NAB_FAILURE - General failure performing the query  
 *    NAB_NOT_FOUND - The requested entry was not found  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address book for the operation  
 *    ldifSearchAttribute - the attribute/value pairing for the search operation  
 *    ldifEntry - the ldif formatted information for the user entry found [memory allocated by API and 
 *                must be freed by a call to NAB_FreeMemory()]  
 *    time - a pointer to the the NABUpdateTime value for the time of last update for the individual entry 
 */
NABError NAB_FindAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, 
                         NABUserID *userID, char *ldifSearchAttribute, 
                         char **ldifEntry, NABUpdateTime *updTime);
    
/* 
 * This call will insert an individual address book entry (in LDIF format) into the (addrBookID) address 
 * book. The addrBook argument is passed back via the calls that return NABAddrBookDescType structures. 
 * If this user already exists in the specified address book, the operation will fail.  
 * 
 * Return Value:  
 *    NAB_SUCCESS - The insert operation succeeded.  
 *    NAB_FAILURE - General failure to during the insert operation  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to the address book to perform the insertion operation  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to insert the entry into the address book  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *    NAB_ALREADY_EXISTS - The user entry already exists in the address book  
 * 
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address book for the operation  
 *    ldifEntry - the entry to be inserted into the identified address book  
 *    userID - a pointer to a NABUserID value that will be filled out by the API with the ID for the new user
 */
NABError NAB_InsertAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, char *ldifEntry, NABUserID *userID);

/* This call will update the specified address book entry in the specified address book with the 
 * information passed in via the LDIF formatted entry. Attributes that exists will be replaced by the 
 * new entry and new attributes will be added to the user record. The addrBook argument is passed back 
 * via the calls that return NABAddrBookDescType structures.  If the entry is not found, the call will fail.  
 *  
 * Return Value:  
 *    NAB_SUCCESS - The update operation succeeded.  
 *    NAB_FAILURE - General failure to during the update operation  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to the address book to perform the update operation  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to update the entry into the address book  
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *    NAB_NOT_FOUND - The user entry to update does not exist in the address book  
 * 
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address book for the operation  
 *    userID - the specific user ID for the entry to be updated  
 *    ldifEntry - the attribute pairs to be updated in the identified address book 
 */
NABError NAB_UpdateAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, NABUserID userID, 
                                    char *ldifEntry);  

/*
 * This call will delete the specified address book entry in the specified address book. The addrBook 
 * argument is passed back via the calls that return NABAddrBookDescType structures.  If the entry is 
 * not found, the call will fail.   
 * 
 * Return Value:  
 *    NAB_SUCCESS - The delete operation succeeded.  
 *    NAB_FAILURE - General failure to during the delete operation  
 *    NAB_NOT_OPEN - This will be returned when a call is made to the API without NAB_Open() being called first.  
 *    NAB_PERMISSION_DENIED - User does not have write permission to the address book to perform the delete operation  
 *    NAB_DISK_FULL - Disk full condition encountered attempting to delete the entry into the address book   
 *    NAB_INVALID_ABOOK - Address book specified is invalid   
 *    NAB_NOT_FOUND - The user entry to delete does not exist in the address book  
 * 
 * Parameters:  
 *    id - the connection ID returned by the NAB_Open() call
 *    addrBook - a pointer to a NABAddrBookDescType structure for the particular address book for the operation  
 *    userID - the specific user ID for the entry to be updated
 */
NABError NAB_DeleteAddressBookEntry(NABConnectionID id, NABAddrBookDescType *addrBook, NABUserID userID);  
  

#endif // ifdef __NABAPI_H__
