/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#include "nsOE5File.h"
#include "nsCRT.h"
#include "OEDebugLog.h"
#include "nsMsgUtils.h"
#include "msgCore.h"
#include "prprf.h"
#include "nsMsgLocalFolderHdrs.h"


#define	kIndexGrowBy		100
#define	kSignatureSize		12
#define	kDontSeek			0xFFFFFFFF

static char *gSig = 
	"\xCF\xAD\x12\xFE\xC5\xFD\x74\x6F\x66\xE3\xD1\x11";


PRBool nsOE5File::VerifyLocalMailFile( nsIFileSpec *pFile)
{
	char		sig[kSignatureSize];
	
	if (!ReadBytes( pFile, sig, 0, kSignatureSize)) {
		return( PR_FALSE);
	}
	
	PRBool	result = PR_TRUE;

	for (int i = 0; (i < kSignatureSize) && result; i++) {
		if (sig[i] != gSig[i]) {
			result = PR_FALSE;
		}
	}
	
	char	storeName[14];
	if (!ReadBytes( pFile, storeName, 0x24C1, 12))
		result = PR_FALSE;

	storeName[12] = 0;
	
	if (nsCRT::strcasecmp( "LocalStore", storeName))
		result = PR_FALSE;
	
	return( result);
}

PRBool nsOE5File::IsLocalMailFile( nsIFileSpec *pFile)
{
	nsresult	rv;
	PRBool		isFile = PR_FALSE;

	rv = pFile->IsFile( &isFile);
	if (NS_FAILED( rv) || !isFile)
		return( PR_FALSE);
	
	rv = pFile->OpenStreamForReading();	
	if (NS_FAILED( rv))
		return( PR_FALSE);
	
	PRBool result = VerifyLocalMailFile( pFile);
	pFile->CloseStream();

	return( result);
}

PRBool nsOE5File::ReadIndex( nsIFileSpec *pFile, PRUint32 **ppIndex, PRUint32 *pSize)
{
  *ppIndex = nsnull;
  *pSize = 0;
  
  char		signature[4];
  if (!ReadBytes( pFile, signature, 0, 4))
    return( PR_FALSE);
  
  for (int i = 0; i < 4; i++) {
    if (signature[i] != gSig[i]) {
      IMPORT_LOG0( "*** Outlook 5.0 dbx file signature doesn't match\n");
      return( PR_FALSE);
    }
  }
  
  PRUint32	offset = 0x00e4;				
  PRUint32	indexStart = 0;
  if (!ReadBytes( pFile, &indexStart, offset, 4)) {
    IMPORT_LOG0( "*** Unable to read offset to index start\n");
    return( PR_FALSE);
  }
  
  PRUint32Array array;
  array.count = 0;
  array.alloc = kIndexGrowBy;
  array.pIndex = new PRUint32[kIndexGrowBy];
  
  PRUint32 next = ReadMsgIndex( pFile, indexStart, &array);
  while (next) {
    next = ReadMsgIndex( pFile, next, &array);
  }
  
  if (array.count) {
    *pSize = array.count;
    *ppIndex = array.pIndex;
    return( PR_TRUE);
  }		
  
  delete [] array.pIndex;
  return( PR_FALSE);
}


PRUint32 nsOE5File::ReadMsgIndex( nsIFileSpec *file, PRUint32 offset, PRUint32Array *pArray)
{
  // Record is:
		// 4 byte marker
		// 4 byte unknown
		// 4 byte nextSubIndex
		// 4 byte (parentIndex?)
		// 2 bytes unknown
		// 1 byte length - # of entries in this record
		// 1 byte unknown
		// 4 byte unknown
		// length records consisting of 3 longs
  //	1 - pointer to record
  //	2 - child index pointer
  //	3 - number of records in child
  
  PRUint32	marker;
  
  if (!ReadBytes( file, &marker, offset, 4))
    return( 0);
  
  if (marker != offset)
    return( 0);
  
  
  PRUint32	vals[3];
  
  if (!ReadBytes( file, vals, offset + 4, 12))
    return( 0);
  
  
  PRUint8	len[4];
  if (!ReadBytes( file, len, offset + 16, 4))
    return( 0);
  
  
  
  PRUint32	cnt = (PRUint32) len[1];
  cnt *= 3;
  PRUint32	*pData = new PRUint32[cnt];
  
  if (!ReadBytes( file, pData, offset + 24, cnt * 4)) {
    delete [] pData;
    return( 0);
  }	
  
  PRUint32	next;
  PRUint32	indexOffset;
  PRUint32 *	pRecord = pData;
  PRUint32 *	pNewIndex;
  
  for (PRUint8 i = 0; i < (PRUint8)len[1]; i++, pRecord += 3) {
    indexOffset = pRecord[0];
    
    if (pArray->count >= pArray->alloc) {
      pNewIndex = new PRUint32[ pArray->alloc + kIndexGrowBy];
      memcpy( pNewIndex, pArray->pIndex, (pArray->alloc * 4));
      (pArray->alloc) += kIndexGrowBy;		
      delete [] pArray->pIndex;
      pArray->pIndex = pNewIndex;
    }
    
    /* 
    We could do some checking here if we wanted - 
    make sure the index is within the file,
    make sure there isn't a duplicate index, etc.
    */
    
    pArray->pIndex[pArray->count] = indexOffset;
    (pArray->count)++;
    
    
    
    next = pRecord[1];
    if (next)
      while ((next = ReadMsgIndex( file, next, pArray)) != 0);
  }
  delete pData;
  
  // return the pointer to the next subIndex
  return( vals[1]);
}

PRBool nsOE5File::IsFromLine( char *pLine, PRUint32 len)
{
   return (len > 5 && (pLine[0] == 'F') && (pLine[1] == 'r') && (pLine[2] == 'o') && (pLine[3] == 'm') && (pLine[4] == ' '));
}

// Anything over 16K will be assumed BAD, BAD, BAD!
#define	kMailboxBufferSize	0x4000
const char *nsOE5File::m_pFromLineSep = "From - Mon Jan 1 00:00:00 1965\x0D\x0A";

nsresult nsOE5File::ImportMailbox( PRUint32 *pBytesDone, PRBool *pAbort, nsString& name, nsIFileSpec *inFile, nsIFileSpec *pDestination, PRUint32 *pCount)
{
  nsresult	rv;
  PRInt32		msgCount = 0;
  if (pCount)
    *pCount = 0;
  
  rv = inFile->OpenStreamForReading();
  if (NS_FAILED( rv)) return( rv);
  rv = pDestination->OpenStreamForWriting();
  if (NS_FAILED( rv)) {
    inFile->CloseStream();
    return( rv);
  }
  
  PRUint32 *	pIndex;
  PRUint32	indexSize;
  
  if (!ReadIndex( inFile, &pIndex, &indexSize)) {
    IMPORT_LOG1( "No messages found in mailbox: %S\n", name.get());
    inFile->CloseStream();
    pDestination->CloseStream();
    return( NS_OK);
  }	
  
  char *  pBuffer = new char[kMailboxBufferSize];
  if (!(*pAbort))
    ConvertIndex( inFile, pBuffer, pIndex, indexSize);
  
  PRUint32  block[4];
  PRInt32   sepLen = (PRInt32) strlen( m_pFromLineSep);
  PRInt32   written;
  
  /*
      Each block is:
      marker - matches file offset
      block length
      text length in block
      pointer to next block. (0 if end)
      
      Each message is made up of a linked list of block data.
      So what we do for each message is:
      1. Read the first block data.
      2. Write out the From message separator if the message doesn't already
      start with one.
      3. If the block of data doesn't end with CRLF then a line is broken into two blocks,
      so save the incomplete line for later process when we read the next block. Then
      write out the block excluding the partial line at the end of the block (if exists).
      4. If there's next block of data then read next data block. Otherwise we're done.
      If we found a partial line in step #3 then find the rest of the line from the
      current block and write out this line separately.
      5. Reset some of the control variables and repeat step #3.
  */
  
  PRUint32	didBytes = 0;
  PRUint32	next, size;
  char *pStart, *pEnd, *partialLineStart;
  nsCAutoString partialLine, tempLine;
  rv = NS_OK;
  
  for (PRUint32 i = 0; (i < indexSize) && !(*pAbort); i++)
  {
    if (! pIndex[i])
      continue;
    
    if (ReadBytes( inFile, block, pIndex[i], 16) && (block[0] == pIndex[i]) &&
      (block[2] < kMailboxBufferSize) && (ReadBytes( inFile, pBuffer, kDontSeek, block[2])))
    {
      // block[2] contains the chars in the buffer (ie, buf content size).
      // block[3] contains offset to the next block of data (0 means no more data).
      size = block[2];
      pStart = pBuffer;
      pEnd = pStart + size;
      
      // write out the from separator.
      if (IsFromLine( pBuffer, size))
      {
        char *pChar = pStart;
        while ((pChar < pEnd) && (*pChar != nsCRT::CR) && (*(pChar+1) != nsCRT::LF))
          pChar++;
        
        if (pChar < pEnd)
        {
          // Get the "From " line so write it out.
          rv = pDestination->Write(pStart, pChar-pStart+2, &written);
          NS_ENSURE_SUCCESS(rv,rv);
          // Now buffer starts from the 2nd line.
          pStart = pChar + 2;
        }
      }
      else
      {
        // Write out the default from line since there is none in the msg.
        rv = pDestination->Write( m_pFromLineSep, sepLen, &written);
        // FIXME: Do I need to check the return value of written???
        if (NS_FAILED( rv))
          break;
      }
      
      char statusLine[50];
      PRUint32 msgFlags = 0; // need to convert from OE flags to mozilla flags
      PR_snprintf(statusLine, sizeof(statusLine), X_MOZILLA_STATUS_FORMAT MSG_LINEBREAK, msgFlags & 0xFFFF);
      rv = pDestination->Write(statusLine, strlen(statusLine), &written);
      NS_ENSURE_SUCCESS(rv,rv);
      PR_snprintf(statusLine, sizeof(statusLine), X_MOZILLA_STATUS2_FORMAT MSG_LINEBREAK, msgFlags & 0xFFFF0000);
      rv = pDestination->Write(statusLine, strlen(statusLine), &written);
      NS_ENSURE_SUCCESS(rv,rv);

      do 
      {
        partialLine.Truncate();
        partialLineStart = pEnd;
        
        // If the buffer doesn't end with CRLF then a line is broken into two blocks,
        // so save the incomplete line for later process when we read the next block.
        if ( (size > 1) && !(*(pEnd - 2) == nsCRT::CR && *(pEnd - 1) == nsCRT::LF) )
        {
          partialLineStart -= 2;
          while ((partialLineStart >= pStart) && (*partialLineStart != nsCRT::CR) && (*(partialLineStart+1) != nsCRT::LF))
            partialLineStart--;
          if (partialLineStart != (pEnd - 2))
            partialLineStart += 2; // skip over CRLF if we find them.
          partialLine.Assign(partialLineStart, pEnd - partialLineStart);
        }
        
        // Now process the block of data which ends with CRLF.
        rv = EscapeFromSpaceLine(pDestination, pStart, partialLineStart);
        if (NS_FAILED(rv))
          break;
        
        didBytes += block[2];
        
        next = block[3];
        if (! next)
        {
          // OK, we're done so flush out the partial line if it's not empty.
          if (partialLine.Length())
            rv = EscapeFromSpaceLine(pDestination, (char *)partialLine.get(), (partialLine.get()+partialLine.Length()));
        }
        else
          if (ReadBytes(inFile, block, next, 16) && (block[0] == next) &&
            (block[2] < kMailboxBufferSize) && (ReadBytes(inFile, pBuffer, kDontSeek, block[2])))
          {
            // See if we have a partial line from previous block. If so then build a complete 
            // line (ie, take the remaining chars from this block) and process this line. Need
            // to adjust where data start and size in this case.
            size = block[2];
            pStart = pBuffer;
            pEnd = pStart + size;
            if (partialLine.Length())
            {
              while ((pStart < pEnd) && (*pStart != nsCRT::CR) && (*(pStart+1) != nsCRT::LF))
                pStart++;
              if (pStart < pEnd)  // if we found a CRLF ..
                pStart += 2;      // .. then copy that too.
              tempLine.Assign(pBuffer, pStart - pBuffer);
              partialLine.Append(tempLine);
              rv = EscapeFromSpaceLine(pDestination, (char *)partialLine.get(), (partialLine.get()+partialLine.Length()));
              if (NS_FAILED(rv))
                break;
              
              // Adjust where data start and size (since some of the data has been processed).
              size -= (pStart - pBuffer);
            }
          }
          else
          {
            IMPORT_LOG2( "Error reading message from %S at 0x%lx\n", name.get(), pIndex[i]);
            rv = pDestination->Write( "\x0D\x0A", 2, &written);
            next = 0;
          }
      } while (next);
      
      // Always end a msg with CRLF. This will make sure that OE msgs without body is
      // correctly recognized as msgs. Otherwise, we'll end up with the following in
      // the msg folder where the 2nd msg starts right after the headers of the 1st msg:
      //
      // From - Jan 1965 00:00:00     <<<--- 1st msg starts here
      // Subject: Test msg
      // . . . (more headers)
      // To: <someone@netscape.com>
      // From - Jan 1965 00:00:00     <<<--- 2nd msg starts here
      // Subject: How are you
      // . . .(more headers)
      //
      // In this case, the 1st msg is not recognized as a msg (it's skipped)
      // when you open the folder.
      rv = pDestination->Write( "\x0D\x0A", 2, &written);
      
      if (NS_FAILED(rv))
        break;
      
      msgCount++;
      if (pCount)
        *pCount = msgCount;
      if (pBytesDone)
        *pBytesDone = didBytes;
    }
    else {
      // Error reading message, should this be logged???
      IMPORT_LOG2( "Error reading message from %S at 0x%lx\n", name.get(), pIndex[i]);
      *pAbort = PR_TRUE;
    }			
  }
        
  delete [] pBuffer;
  inFile->CloseStream();
  pDestination->CloseStream();
  
  if (NS_FAILED(rv))
    *pAbort = PR_TRUE;
  
  return( rv);
}


/*
  A message index record consists of:
  4 byte marker - matches record offset
  4 bytes size - size of data after this header
  2 bytes header length
  (2 bytes - flag offset in msg hdr?) or the following?
  1 bytes - number of attributes
  1 byte changes on this object
  Each attribute is a 4 byte value with the 1st byte being the tag
  and the remaing 3 bytes being data.  The data is either a direct
  offset of an offset within the message index that points to the
  data for the tag.

  Current known tags are:
  0x02 - a time value
  0x04 - text offset pointer, the data is the offset after the attribute
         of a 4 byte pointer to the message text
  0x05 - offset to truncated subject
  0x08 - offste to subject
  0x0D - offset to from
  0x0E - offset to from addresses
  0x13 - offset to to name
  0x45 - offset to to address
  0x80 - msgId
  0x84 - direct text offset, direct pointer to message text

   flags we're interested in:
      kMarked= 0x20;
      kRead = 0x80;
      kHasAttachment = 0x4000;
      kIsReply= 0x80000;

*/
                
void nsOE5File::ConvertIndex( nsIFileSpec *pFile, char *pBuffer, PRUint32 *pIndex, PRUint32 size)
{
  // for each index record, get the actual message offset!  If there is a problem
  // just record the message offset as 0 and the message reading code
  // can log that error information.
  
  PRUint8   recordHead[12];
  PRUint32  marker;
  PRUint32  recordSize;
  PRUint32  numAttrs;
  PRUint32  offset;
  PRUint32  attrIndex;
  PRUint32  attrOffset;
  PRUint8   tag;
  PRUint32  tagData;
  
  for (PRUint32 i = 0; i < size; i++) {
    offset = 0;
    if (ReadBytes( pFile, recordHead, pIndex[i], 12)) {
      memcpy( &marker, recordHead, 4);
      memcpy( &recordSize, recordHead + 4, 4);
      numAttrs = (PRUint32) recordHead[10];
      if ((marker == pIndex[i]) && (recordSize < kMailboxBufferSize) && ((numAttrs * 4) <= recordSize)) {
        if (ReadBytes( pFile, pBuffer, kDontSeek, recordSize)) {
          attrOffset = 0;
          for (attrIndex = 0; attrIndex < numAttrs; attrIndex++, attrOffset += 4) {
            tag = (PRUint8) pBuffer[attrOffset];
            if (tag == (PRUint8) 0x84) {
              tagData = 0;
              memcpy( &tagData, pBuffer + attrOffset + 1, 3);
              offset = tagData;
              break;
            }
            else if (tag == (PRUint8) 0x04) {
              tagData = 0;
              memcpy( &tagData, pBuffer + attrOffset + 1, 3);
              if (((numAttrs * 4) + tagData + 4) <= recordSize)
                memcpy( &offset, pBuffer + (numAttrs * 4) + tagData, 4);
            }
          }
        }
      }
    }
    pIndex[i] = offset;
  }
}


PRBool nsOE5File::ReadBytes( nsIFileSpec *stream, void *pBuffer, PRUint32 offset, PRUint32 bytes)
{
  nsresult	rv;
  
  if (offset != kDontSeek) {
    rv = stream->Seek( offset);
    if (NS_FAILED( rv))
      return( PR_FALSE);
  }
  
  if (!bytes)
    return( PR_TRUE);
  
  PRInt32	cntRead;
  char *	pReadTo = (char *)pBuffer;
  rv = stream->Read( &pReadTo, bytes, &cntRead);	
  if (NS_FAILED( rv) || ((PRUint32)cntRead != bytes))
    return( PR_FALSE);
  return( PR_TRUE);
  
}

