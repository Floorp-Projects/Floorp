/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsTextAddress.h"
#include "nsIAddrDatabase.h"
#include "nsNativeCharsetUtils.h"

#include "TextDebugLog.h"

#define kWhitespace    " \t\b\r\n"

// If we get a line longer than 32K it's just toooooo bad!
#define kTextAddressBufferSz    (64 * 1024)

nsTextAddress::nsTextAddress()
{
    m_database = nsnull;
    m_fieldMap = nsnull;
    m_LFCount = 0;
    m_CRCount = 0;
}

nsTextAddress::~nsTextAddress()
{
    NS_IF_RELEASE( m_database);
    NS_IF_RELEASE( m_fieldMap);
}

nsresult nsTextAddress::ImportAddresses( PRBool *pAbort, const PRUnichar *pName, nsIFileSpec *pSrc, nsIAddrDatabase *pDb, nsIImportFieldMap *fieldMap, nsString& errors, PRUint32 *pProgress)
{
    // Open the source file for reading, read each line and process it!
    NS_IF_RELEASE( m_database);
    NS_IF_RELEASE( m_fieldMap);
    m_database = pDb;
    m_fieldMap = fieldMap;
    NS_ADDREF( m_fieldMap);
    NS_ADDREF( m_database);

    nsresult rv = pSrc->OpenStreamForReading();
    if (NS_FAILED( rv)) {
        IMPORT_LOG0( "*** Error opening address file for reading\n");
        return( rv);
    }
    
    char *pLine = new char[kTextAddressBufferSz];
    PRBool    eof = PR_FALSE;
    rv = pSrc->Eof( &eof);
    if (NS_FAILED( rv)) {
        IMPORT_LOG0( "*** Error checking address file for eof\n");
        pSrc->CloseStream();
        return( rv);
    }
    
    PRInt32    loc;
    PRInt32    lineLen = 0;
    while (!(*pAbort) && !eof && NS_SUCCEEDED( rv)) {
        rv = pSrc->Tell( &loc);
        if (NS_SUCCEEDED( rv) && pProgress)
            *pProgress = (PRUint32)loc;
        rv = ReadRecord( pSrc, pLine, kTextAddressBufferSz, m_delim, &lineLen);
        if (NS_SUCCEEDED( rv)) {
            rv = ProcessLine( pLine, strlen( pLine), errors);
            if (NS_FAILED( rv)) {
                IMPORT_LOG0( "*** Error processing text record.\n");
            }
            else
                rv = pSrc->Eof( &eof);
        }
    }
    
    rv = pSrc->CloseStream();
    
    delete [] pLine;
    
    if (!eof) {
        IMPORT_LOG0( "*** Error reading the address book, didn't reach the end\n");
        return( NS_ERROR_FAILURE);
    }
    
    rv = pDb->Commit(nsAddrDBCommitType::kLargeCommit);
    return rv;
}



nsresult nsTextAddress::ReadRecord( nsIFileSpec *pSrc, char *pLine, PRInt32 bufferSz, char delim, PRInt32 *pLineLen)
{
    PRBool        wasTruncated;
    PRBool        isEof;
    char *        pRead;
    PRInt32        lineLen = 0;
    nsresult    rv;
    do {
        if (lineLen) {
            if ((lineLen + 2) < bufferSz) {
                memcpy( pLine + lineLen, "\x0D\x0A", 2);
                lineLen += 2;
                pLine[lineLen] = 0;
            }
        }
        wasTruncated = PR_FALSE;
        pRead = pLine;
        pRead += lineLen;
        pSrc->Eof(&isEof);
        if (isEof)
          // If we get an EOF here, then the line isn't complete
          // so we must have an incorrect file.
          rv = NS_ERROR_FAILURE;
        else
        {
          rv = pSrc->ReadLine( &pRead, bufferSz - lineLen, &wasTruncated);
          if (wasTruncated) {
            pLine[bufferSz - 1] = 0;
            IMPORT_LOG0( "Unable to read line from file, buffer too small\n");
            rv = NS_ERROR_FAILURE;
          }
          else if (NS_SUCCEEDED( rv)) {
            lineLen = strlen( pLine);
          }
        }
    } while (NS_SUCCEEDED( rv) && !IsLineComplete( pLine, lineLen));

    *pLineLen = lineLen;
    return( rv);
}


nsresult nsTextAddress::ReadRecordNumber( nsIFileSpec *pSrc, char *pLine, PRInt32 bufferSz, char delim, PRInt32 *pLineLen, PRInt32 rNum)
{
    PRInt32    rIndex = 0;
    nsresult rv = pSrc->Seek( 0);
    if (NS_FAILED( rv))
        return( rv);
    
    PRBool    eof = PR_FALSE;

    while (!eof && (rIndex <= rNum)) {
        if (NS_FAILED( rv = ReadRecord( pSrc, pLine, bufferSz, delim, pLineLen)))
            return( rv);
        if (rIndex == rNum)
            return( NS_OK);
        rIndex++;
        rv = pSrc->Eof( &eof);
        if (NS_FAILED( rv))
            return( rv);
    }

    return( NS_ERROR_FAILURE);
}




/*
    Find out if the given line appears to be a complete record or
    if it needs more data because the line ends with a quoted value non-terminated
*/
PRBool nsTextAddress::IsLineComplete( const char *pLine, PRInt32 len)
{
    PRBool    quoted = PR_FALSE;

    while (len) {
      if ((*pLine == '"')) {
        quoted = !quoted;
      }
      pLine++;
      len--;
    }

    return !quoted;
}

PRInt32 nsTextAddress::CountFields( const char *pLine, PRInt32 maxLen, char delim)
{
    const char *pChar = pLine;
    PRInt32        len = 0;
    PRInt32        count = 0;
    char        tab = 9;

    if (delim == tab)
        tab = 0;

    while (len < maxLen) {
        while (((*pChar == ' ') || (*pChar == tab)) && (len < maxLen)) {
            pChar++;
            len++;
        }
        if ((len < maxLen) && (*pChar == '"')) {
            pChar++;
            len++;
            while ((len < maxLen) && (*pChar != '"')) {
                len++;
                pChar++;
                if (((len + 1) < maxLen) && (*pChar == '"') && (*(pChar + 1) == '"')) {
                    len += 2;
                    pChar += 2;
                }
            }
            if (len < maxLen) {
                pChar++;
                len++;
            }
        }        
        while ((len < maxLen) && (*pChar != delim)) {
            len++;
            pChar++;
        }
        
        count++;
        pChar++;
        len++;
    }

    return( count);
}

PRBool nsTextAddress::GetField( const char *pLine, PRInt32 maxLen, PRInt32 index, nsCString& field, char delim)
{
    PRBool result = PR_FALSE;
    const char *pChar = pLine;
    PRInt32        len = 0;
    char        tab = 9;

    field.Truncate();

    if (delim == tab)
        tab = 0;

    while (index && (len < maxLen)) {
        while (((*pChar == ' ') || (*pChar == tab)) && (len < maxLen)) {
            pChar++;
            len++;
        }
        if (len >= maxLen)
            break;
        if (*pChar == '"') {
            len = -1;
            do {
                len++;
                pChar++;
                if (((len + 1) < maxLen) && (*pChar == '"') && (*(pChar + 1) == '"')) {
                    len += 2;
                    pChar += 2;
                }
            } while ((len < maxLen) && (*pChar != '"'));
            if (len < maxLen) {
                pChar++;
                len++;
            }
        }
        if (len >= maxLen)
            break;
        
        while ((len < maxLen) && (*pChar != delim)) {
            len++;
            pChar++;
        }
        
        if (len >= maxLen)
            break;

        index--;
        pChar++;
        len++;
    }

    if (len >= maxLen) {
        return( result);
    }

    result = PR_TRUE;

    while ((len < maxLen) && ((*pChar == ' ') || (*pChar == tab))) {
        len++;
        pChar++;
    }

    const char *pStart = pChar;
    PRInt32        fLen = 0;
    PRBool        quoted = PR_FALSE;
    if (*pChar == '"') {
        pStart++;
        fLen = -1;
        do {
            pChar++;
            len++;
            fLen++;
            if (((len + 1) < maxLen) && (*pChar == '"') && (*(pChar + 1) == '"')) {
                quoted = PR_TRUE;
                len += 2;
                pChar += 2;
                fLen += 2;
            }
        } while ((len < maxLen) && (*pChar != '"'));
    }
    else {
        while ((len < maxLen) && (*pChar != delim)) {
            pChar++;
            len++;
            fLen++;
        }
    }

    if (!fLen) {
        return( result);
    }

    field.Append( pStart, fLen);
    field.Trim( kWhitespace);

    if (quoted) {
        field.ReplaceSubstring( "\"\"", "\"");
    }

    return( result);
}


void nsTextAddress::SanitizeSingleLine( nsCString& val)
{
    val.ReplaceSubstring( "\x0D\x0A", ", ");
    val.ReplaceChar( 13, ' ');
    val.ReplaceChar( 10, ' ');
}


nsresult nsTextAddress::DetermineDelim( nsIFileSpec *pSrc)
{
    nsresult rv = pSrc->OpenStreamForReading();
    if (NS_FAILED( rv)) {
        IMPORT_LOG0( "*** Error opening address file for reading\n");
        return( rv);
    }
    
    char *pLine = new char[kTextAddressBufferSz];
    PRBool    eof = PR_FALSE;
    rv = pSrc->Eof( &eof);
    if (NS_FAILED( rv)) {
        IMPORT_LOG0( "*** Error checking address file for eof\n");
        pSrc->CloseStream();
        return( rv);
    }
    
    PRBool    wasTruncated = PR_FALSE;
    PRInt32    lineLen = 0;
    PRInt32    lineCount = 0;
    PRInt32    tabCount = 0;
    PRInt32    commaCount = 0;
    PRInt32    tabLines = 0;
    PRInt32    commaLines = 0;

    while (!eof && NS_SUCCEEDED( rv) && (lineCount < 100)) {
        wasTruncated = PR_FALSE;
        rv = pSrc->ReadLine( &pLine, kTextAddressBufferSz, &wasTruncated);
        if (wasTruncated)
            pLine[kTextAddressBufferSz - 1] = 0;
        if (NS_SUCCEEDED( rv)) {
            lineLen = strlen( pLine);
            tabCount = CountFields( pLine, lineLen, 9);
            commaCount = CountFields( pLine, lineLen, ',');
            if (tabCount > commaCount)
                tabLines++;
            else if (commaCount)
                commaLines++;
            rv = pSrc->Eof( &eof);
        }
        lineCount++;
    }
    
    rv = pSrc->CloseStream();
    
    delete [] pLine;
    
    if (tabLines > commaLines)
        m_delim = 9;
    else
        m_delim = ',';

    return( NS_OK);
}


/*
    This is where the real work happens!
    Go through the field map and set the data in a new database row
*/
nsresult nsTextAddress::ProcessLine( const char *pLine, PRInt32 len, nsString& errors)
{
    if (!m_fieldMap) {
        IMPORT_LOG0( "*** Error, text import needs a field map\n");
        return( NS_ERROR_FAILURE);
    }

    nsresult rv;
    
    // Wait until we get our first non-empty field, then create a new row,
    // fill in the data, then add the row to the database.
        

    nsIMdbRow *    newRow = nsnull;
    nsString    uVal;
    nsCString    fieldVal;
    PRInt32        fieldNum;
    PRInt32        numFields = 0;
    PRBool        active;
    rv = m_fieldMap->GetMapSize( &numFields);
    for (PRInt32 i = 0; (i < numFields) && NS_SUCCEEDED( rv); i++) {
        active = PR_FALSE;
        rv = m_fieldMap->GetFieldMap( i, &fieldNum);
        if (NS_SUCCEEDED( rv))
            rv = m_fieldMap->GetFieldActive( i, &active);
        if (NS_SUCCEEDED( rv) && active) {
            if (GetField( pLine, len, i, fieldVal, m_delim)) {
                if (!fieldVal.IsEmpty()) {
                    if (!newRow) {
                        rv = m_database->GetNewRow( &newRow);
                        if (NS_FAILED( rv)) {
                            IMPORT_LOG0( "*** Error getting new address database row\n");
                        }
                    }
                    if (newRow) {
                        NS_CopyNativeToUnicode( fieldVal, uVal);
                        rv = m_fieldMap->SetFieldValue( m_database, newRow, fieldNum, uVal.get());
                    }
                }
            }
            else
                break;
            
        }
        else {
            if (active) {
                IMPORT_LOG1( "*** Error getting field map for index %ld\n", (long) i);
            }
        }

    }
    
    if (NS_SUCCEEDED( rv)) {
        if (newRow) {
            rv = m_database->AddCardRowToDB( newRow);
            // Release newRow????
        }
    }
    else {
        // Release newRow??
    }

    return( rv);
}

