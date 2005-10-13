/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mark Banner <bugzilla@standard8.demon.co.uk>
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
#include "nsIAddrDatabase.h"
#include "nsString.h"
#include "nsAbLDIFService.h"
#include "nsIFile.h"
#include "nsILineInputStream.h"
#include "nsNetUtil.h"
#include "nsISeekableStream.h"
#include "nsVoidArray.h"
#include "mdb.h"
#include "plstr.h"
#include "prmem.h"
#include "prprf.h"
#include "nsCRT.h"

NS_IMPL_ISUPPORTS1(nsAbLDIFService, nsIAbLDIFService)

// If we get a line longer than 32K it's just toooooo bad!
#define kTextAddressBufferSz    (64 * 1024)

nsAbLDIFService::nsAbLDIFService()
{
  mStoreLocAsHome = PR_FALSE;
  mLFCount = 0;
  mCRCount = 0;
}

nsAbLDIFService::~nsAbLDIFService()
{
}

#define RIGHT2            0x03
#define RIGHT4            0x0f
#define CONTINUED_LINE_MARKER    '\001'

// XXX TODO fix me
// use the NSPR base64 library.  see plbase64.h
// see bug #145367
static unsigned char b642nib[0x80] = {
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0xff, 0xff, 0x3e, 0xff, 0xff, 0xff, 0x3f,
    0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x3a, 0x3b,
    0x3c, 0x3d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
    0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d, 0x0e,
    0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16,
    0x17, 0x18, 0x19, 0xff, 0xff, 0xff, 0xff, 0xff,
    0xff, 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x1f, 0x20,
    0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28,
    0x29, 0x2a, 0x2b, 0x2c, 0x2d, 0x2e, 0x2f, 0x30,
    0x31, 0x32, 0x33, 0xff, 0xff, 0xff, 0xff, 0xff
};

NS_IMETHODIMP nsAbLDIFService::ImportLDIFFile(nsIAddrDatabase *aDb, nsIFile *aSrc, PRBool aStoreLocAsHome, PRUint32 *aProgress)
{
  NS_ENSURE_ARG_POINTER(aSrc);
  NS_ENSURE_ARG_POINTER(aDb);
  mDatabase = aDb;

  mStoreLocAsHome = aStoreLocAsHome;

  char buf[1024];
  char* pBuf = &buf[0];
  PRInt32 startPos = 0;
  PRUint32 len = 0;
  nsVoidArray listPosArray;   // where each list/group starts in ldif file
  nsVoidArray listSizeArray;  // size of the list/group info
  PRInt32 savedStartPos = 0;
  PRInt32 filePos = 0;
  PRUint32 bytesLeft = 0;

  nsCOMPtr<nsIInputStream> inputStream;
  nsresult rv = NS_NewLocalFileInputStream(getter_AddRefs(inputStream), aSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  // Initialize the parser for a run...
  mLdifLine.Truncate();

  while (NS_SUCCEEDED(inputStream->Available(&bytesLeft)) && bytesLeft > 0)
  {
    if (NS_SUCCEEDED(inputStream->Read(pBuf, sizeof(buf), &len)) && len > 0)
    {
      startPos = 0;

      while (NS_SUCCEEDED(GetLdifStringRecord(buf, len, startPos)))
      {
        if (mLdifLine.Find("groupOfNames") == kNotFound)
          AddLdifRowToDatabase(PR_FALSE);
        else
        {
          //keep file position for mailing list
          listPosArray.AppendElement((void*)savedStartPos);
          listSizeArray.AppendElement((void*)(filePos + startPos-savedStartPos));
          ClearLdifRecordBuffer();
        }
        savedStartPos = filePos + startPos;
      }
      filePos += len;
      if (aProgress)
        *aProgress = (PRUint32)filePos;
    }
  }
  //last row
  if (!mLdifLine.IsEmpty() && mLdifLine.Find("groupOfNames") == kNotFound)
    AddLdifRowToDatabase(PR_FALSE); 

  // mail Lists
  PRInt32 i, pos;
  PRUint32 size;
  PRInt32 listTotal = listPosArray.Count();
  char *listBuf;
  ClearLdifRecordBuffer();  // make sure the buffer is clean

  nsCOMPtr<nsISeekableStream> seekableStream = do_QueryInterface(inputStream, &rv);
  NS_ENSURE_SUCCESS(rv, rv);

  for (i = 0; i < listTotal; i++)
  {
    pos  = NS_PTR_TO_INT32(listPosArray.ElementAt(i));
    size = NS_PTR_TO_INT32(listSizeArray.ElementAt(i));
    if (NS_SUCCEEDED(seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, pos)))
    {
      // Allocate enough space for the lists/groups as the size varies.
      listBuf = (char *) PR_Malloc(size);
      if (!listBuf)
        continue;
      if (NS_SUCCEEDED(inputStream->Read(listBuf, size, &len)) && len > 0)
      {
        startPos = 0;

        while (NS_SUCCEEDED(GetLdifStringRecord(listBuf, len, startPos)))
        {
          if (mLdifLine.Find("groupOfNames") != kNotFound)
          {
            AddLdifRowToDatabase(PR_TRUE);
            if (NS_SUCCEEDED(seekableStream->Seek(nsISeekableStream::NS_SEEK_SET, 0)))
              break;
          }
        }
      }
    PR_FREEIF(listBuf);
    }
  }

  rv = inputStream->Close();
  NS_ENSURE_SUCCESS(rv, rv);

  // Finally commit everything to the database and return.
  return aDb->Commit(nsAddrDBCommitType::kLargeCommit);
}

/*
 * str_parse_line - takes a line of the form "type:[:] value" and splits it
 * into components "type" and "value".  if a double colon separates type from
 * value, then value is encoded in base 64, and parse_line un-decodes it
 * (in place) before returning.
 * in LDIF, non-ASCII data is treated as base64 encoded UTF-8 
 */

nsresult nsAbLDIFService::str_parse_line(char *line, char **type, char **value, int *vlen) const
{
  char    *p, *s, *d, *byte, *stop;
  char    nib;
  int    i, b64;

  /* skip any leading space */
  while ( NS_IS_SPACE( *line ) ) {
    line++;
  }
  *type = line;

  for ( s = line; *s && *s != ':'; s++ )
    ;    /* NULL */
  if ( *s == '\0' ) {
    return NS_ERROR_FAILURE;
  }

  /* trim any space between type and : */
  for ( p = s - 1; p > line && nsCRT::IsAsciiSpace( *p ); p-- ) {
    *p = '\0';
  }
  *s++ = '\0';

  /* check for double : - indicates base 64 encoded value */
  if ( *s == ':' ) {
    s++;
    b64 = 1;
  /* single : - normally encoded value */
  } else {
    b64 = 0;
  }

  /* skip space between : and value */
  while ( NS_IS_SPACE( *s ) ) {
    s++;
  }

  /* if no value is present, error out */
  if ( *s == '\0' ) {
    return NS_ERROR_FAILURE;
  }

  /* check for continued line markers that should be deleted */
  for ( p = s, d = s; *p; p++ ) {
    if ( *p != CONTINUED_LINE_MARKER )
      *d++ = *p;
  }
  *d = '\0';

  *value = s;
  if ( b64 ) {
    stop = PL_strchr( s, '\0' );
    byte = s;
    for ( p = s, *vlen = 0; p < stop; p += 4, *vlen += 3 ) {
      for ( i = 0; i < 3; i++ ) {
        if ( p[i] != '=' && (p[i] & 0x80 ||
             b642nib[ p[i] & 0x7f ] > 0x3f) ) {
          return NS_ERROR_FAILURE;
        }
      }

      /* first digit */
      nib = b642nib[ p[0] & 0x7f ];
      byte[0] = nib << 2;
      /* second digit */
      nib = b642nib[ p[1] & 0x7f ];
      byte[0] |= nib >> 4;
      byte[1] = (nib & RIGHT4) << 4;
      /* third digit */
      if ( p[2] == '=' ) {
        *vlen += 1;
        break;
      }
      nib = b642nib[ p[2] & 0x7f ];
      byte[1] |= nib >> 2;
      byte[2] = (nib & RIGHT2) << 6;
      /* fourth digit */
      if ( p[3] == '=' ) {
        *vlen += 2;
        break;
      }
      nib = b642nib[ p[3] & 0x7f ];
      byte[2] |= nib;

      byte += 3;
    }
    s[ *vlen ] = '\0';
  } else {
    *vlen = (int) (d - s);
  }
  return NS_OK;
}

/*
 * str_getline - return the next "line" (minus newline) of input from a
 * string buffer of lines separated by newlines, terminated by \n\n
 * or \0.  this routine handles continued lines, bundling them into
 * a single big line before returning.  if a line begins with a white
 * space character, it is a continuation of the previous line. the white
 * space character (nb: only one char), and preceeding newline are changed
 * into CONTINUED_LINE_MARKER chars, to be deleted later by the
 * str_parse_line() routine above.
 *
 * it takes a pointer to a pointer to the buffer on the first call,
 * which it updates and must be supplied on subsequent calls.
 */

char* nsAbLDIFService::str_getline(char **next) const
{
  char    *lineStr;
  char    c;

  if ( *next == nsnull || **next == '\n' || **next == '\0' ) {
    return( nsnull);
  }

  lineStr = *next;
  while ( (*next = PL_strchr( *next, '\n' )) != NULL ) {
    c = *(*next + 1);
    if ( NS_IS_SPACE ( c ) && c != '\n' ) {
      **next = CONTINUED_LINE_MARKER;
      *(*next+1) = CONTINUED_LINE_MARKER;
    } else {
      *(*next)++ = '\0';
      break;
    }
  }

  return( lineStr );
}

nsresult nsAbLDIFService::GetLdifStringRecord(char* buf, PRInt32 len, PRInt32& stopPos)
{
  for (; stopPos < len; stopPos++) 
  {
    char c = buf[stopPos];

    if (c == 0xA)
    {
      mLFCount++;
    }
    else if (c == 0xD)
    {
      mCRCount++;
    }
    else
    {
      if (mLFCount == 0 && mCRCount == 0)
        mLdifLine.Append(c);
      else if (( mLFCount > 1) || ( mCRCount > 2 && mLFCount ) ||
               ( !mLFCount && mCRCount > 1 ))
      {
        return NS_OK;
      }
      else if ((mLFCount == 1 || mCRCount == 1))
      {
        mLdifLine.Append('\n');
        mLdifLine.Append(c);
        mLFCount = 0;
        mCRCount = 0;
      }
    }
  }

  if ((stopPos == len) && (mLFCount > 1) || (mCRCount > 2 && mLFCount) ||
      (!mLFCount && mCRCount > 1))
    return NS_OK;

  return NS_ERROR_FAILURE;
}

void nsAbLDIFService::AddLdifRowToDatabase(PRBool bIsList)
{
  // If no data to process then reset CR/LF counters and return.
  if (mLdifLine.IsEmpty())
  {
    mLFCount = 0;
    mCRCount = 0;
    return;
  }

  nsCOMPtr <nsIMdbRow> newRow;
  if (mDatabase)
  {
    if (bIsList)
      mDatabase->GetNewListRow(getter_AddRefs(newRow)); 
    else
      mDatabase->GetNewRow(getter_AddRefs(newRow)); 

    if (!newRow)
      return;
  }
  else
    return;

  char* cursor = ToNewCString(mLdifLine); 
  char* saveCursor = cursor;  /* keep for deleting */ 
  char* line = 0; 
  char* typeSlot = 0; 
  char* valueSlot = 0; 
  int length = 0;  // the length  of an ldif attribute
  while ( (line = str_getline(&cursor)) != nsnull)
  {
    if ( str_parse_line(line, &typeSlot, &valueSlot, &length) == 0) {
      AddLdifColToDatabase(newRow, typeSlot, valueSlot, bIsList);
    }
    else
      continue; // parse error: continue with next loop iteration
  }
  nsMemory::Free(saveCursor);
  mDatabase->AddCardRowToDB(newRow);    

  if (bIsList)
    mDatabase->AddListDirNode(newRow);
        
  // Clear buffer for next record
  ClearLdifRecordBuffer();
}

void nsAbLDIFService::AddLdifColToDatabase(nsIMdbRow* newRow, char* typeSlot, char* valueSlot, PRBool bIsList)
{
  nsCAutoString colType(typeSlot);
  nsCAutoString column(valueSlot);

  // 4.x exports attributes like "givenname", 
  // mozilla does "givenName" to be compliant with RFC 2798
  ToLowerCase(colType);

  mdb_u1 firstByte = (mdb_u1)(colType.get())[0];
  switch ( firstByte )
  {
  case 'b':
    if (colType.EqualsLiteral("birthyear"))
      mDatabase->AddBirthYear(newRow, column.get());
    break; // 'b'

  case 'c':
    if (colType.EqualsLiteral("cn") || colType.EqualsLiteral("commonname"))
    {
      if (bIsList)
        mDatabase->AddListName(newRow, column.get());
      else
        mDatabase->AddDisplayName(newRow, column.get());
    }
    else if (colType.EqualsLiteral("c") || colType.EqualsLiteral("countryname"))
    {
      if (mStoreLocAsHome )
        mDatabase->AddHomeCountry(newRow, column.get());
      else
        mDatabase->AddWorkCountry(newRow, column.get());
    }

    else if (colType.EqualsLiteral("cellphone") )
      mDatabase->AddCellularNumber(newRow, column.get());

    else if (colType.EqualsLiteral("carphone"))
      mDatabase->AddCellularNumber(newRow, column.get());
        
    else if (colType.EqualsLiteral("custom1"))
      mDatabase->AddCustom1(newRow, column.get());
        
    else if (colType.EqualsLiteral("custom2"))
      mDatabase->AddCustom2(newRow, column.get());
        
    else if (colType.EqualsLiteral("custom3"))
      mDatabase->AddCustom3(newRow, column.get());
        
    else if (colType.EqualsLiteral("custom4"))
      mDatabase->AddCustom4(newRow, column.get());
        
    else if (colType.EqualsLiteral("company"))
      mDatabase->AddCompany(newRow, column.get());
    break; // 'c'

  case 'd':
    if (colType.EqualsLiteral("description"))
    {
      if (bIsList)
        mDatabase->AddListDescription(newRow, column.get());
      else
        mDatabase->AddNotes(newRow, column.get());
    }

    else if (colType.EqualsLiteral("department"))
      mDatabase->AddDepartment(newRow, column.get());

    else if (colType.EqualsLiteral("displayname"))
    {
      if (bIsList)
        mDatabase->AddListName(newRow, column.get());
      else
        mDatabase->AddDisplayName(newRow, column.get());
    }
    break; // 'd'

  case 'f':

    if (colType.EqualsLiteral("fax") ||
        colType.EqualsLiteral("facsimiletelephonenumber"))
      mDatabase->AddFaxNumber(newRow, column.get());
    break; // 'f'

  case 'g':
    if (colType.EqualsLiteral("givenname"))
      mDatabase->AddFirstName(newRow, column.get());

    break; // 'g'

  case 'h':
    if (colType.EqualsLiteral("homephone"))
      mDatabase->AddHomePhone(newRow, column.get());

    else if (colType.EqualsLiteral("homestreet"))
      mDatabase->AddHomeAddress(newRow, column.get());

    else if (colType.EqualsLiteral("homeurl"))
      mDatabase->AddWebPage2(newRow, column.get());
    break; // 'h'

  case 'l':
    if (colType.EqualsLiteral("l") || colType.EqualsLiteral("locality"))
    {
      if (mStoreLocAsHome)
        mDatabase->AddHomeCity(newRow, column.get());
      else
        mDatabase->AddWorkCity(newRow, column.get());
    }

    break; // 'l'

  case 'm':
    if (colType.EqualsLiteral("mail"))
      mDatabase->AddPrimaryEmail(newRow, column.get());

    else if (colType.EqualsLiteral("member") && bIsList)
      mDatabase->AddLdifListMember(newRow, column.get());

    else if (colType.EqualsLiteral("mobile"))
      mDatabase->AddCellularNumber(newRow, column.get());

    else if (colType.EqualsLiteral("mozilla_aimscreenname"))
      mDatabase->AddAimScreenName(newRow, column.get());

    else if (colType.EqualsLiteral("mozillacustom1"))
      mDatabase->AddCustom1(newRow, column.get());
        
    else if (colType.EqualsLiteral("mozillacustom2"))
      mDatabase->AddCustom2(newRow, column.get());
        
    else if (colType.EqualsLiteral("mozillacustom3"))
      mDatabase->AddCustom3(newRow, column.get());
        
    else if (colType.EqualsLiteral("mozillacustom4"))
      mDatabase->AddCustom4(newRow, column.get());

    else if (colType.EqualsLiteral("mozilladefaultemail"))
      mDatabase->AddDefaultEmail(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomecountryname"))
      mDatabase->AddHomeCountry(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomelocalityname"))
      mDatabase->AddHomeCity(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomestate"))
      mDatabase->AddHomeState(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomestreet2"))
      mDatabase->AddHomeAddress2(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomepostalcode"))
      mDatabase->AddHomeZipCode(newRow, column.get());

    else if (colType.EqualsLiteral("mozillahomeurl"))
      mDatabase->AddWebPage2(newRow, column.get());

    else if (colType.EqualsLiteral("mozillanickname"))
    {
      if (bIsList)
        mDatabase->AddListNickName(newRow, column.get());
      else
        mDatabase->AddNickName(newRow, column.get());
    }

    else if (colType.EqualsLiteral("mozillasecondemail"))
      mDatabase->Add2ndEmail(newRow, column.get());

    else if (colType.EqualsLiteral("mozillausehtmlmail"))
    {
      ToLowerCase(column);
      if (kNotFound != column.Find("true"))
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::html);
      else if (kNotFound != column.Find("false"))
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::plaintext);
      else
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::unknown);
    }

    else if (colType.EqualsLiteral("mozillaworkstreet2"))
      mDatabase->AddWorkAddress2(newRow, column.get());

    else if (colType.EqualsLiteral("mozillaworkurl"))
      mDatabase->AddWebPage1(newRow, column.get());

    break; // 'm'

  case 'n':
    if (colType.EqualsLiteral("notes"))
      mDatabase->AddNotes(newRow, column.get());

    else if (colType.EqualsLiteral("nscpaimscreenname") || 
             colType.EqualsLiteral("nsaimid"))
      mDatabase->AddAimScreenName(newRow, column.get());

    break; // 'n'

  case 'o':
    if (colType.EqualsLiteral("objectclass"))
      break;

    else if (colType.EqualsLiteral("ou") || colType.EqualsLiteral("orgunit"))
      mDatabase->AddDepartment(newRow, column.get());

    else if (colType.EqualsLiteral("o")) // organization
      mDatabase->AddCompany(newRow, column.get());

    break; // 'o'

  case 'p':
    if (colType.EqualsLiteral("postalcode"))
    {
      if (mStoreLocAsHome)
        mDatabase->AddHomeZipCode(newRow, column.get());
      else
        mDatabase->AddWorkZipCode(newRow, column.get());
    }

    else if (colType.EqualsLiteral("postofficebox"))
    {
      nsCAutoString workAddr1, workAddr2;
      SplitCRLFAddressField(column, workAddr1, workAddr2);
      mDatabase->AddWorkAddress(newRow, workAddr1.get());
      mDatabase->AddWorkAddress2(newRow, workAddr2.get());
    }
    else if (colType.EqualsLiteral("pager") || colType.EqualsLiteral("pagerphone"))
      mDatabase->AddPagerNumber(newRow, column.get());

    break; // 'p'

  case 'r':
    if (colType.EqualsLiteral("region"))
    {
      if (mStoreLocAsHome)
        mDatabase->AddWorkState(newRow, column.get());
      else
        mDatabase->AddWorkState(newRow, column.get());
    }

    break; // 'r'

  case 's':
    if (colType.EqualsLiteral("sn") || colType.EqualsLiteral("surname"))
      mDatabase->AddLastName(newRow, column.get());

    else if (colType.EqualsLiteral("street"))
      mDatabase->AddWorkAddress(newRow, column.get());

    else if (colType.EqualsLiteral("streetaddress"))
    {
      nsCAutoString addr1, addr2;
      SplitCRLFAddressField(column, addr1, addr2);
      if (mStoreLocAsHome)
      {
        mDatabase->AddHomeAddress(newRow, addr1.get());
        mDatabase->AddHomeAddress2(newRow, addr2.get());
      }
      else
      {
        mDatabase->AddWorkAddress(newRow, addr1.get());
        mDatabase->AddWorkAddress2(newRow, addr2.get());
      }
    }
    else if (colType.EqualsLiteral("st"))
    {
    if (mStoreLocAsHome)
      mDatabase->AddHomeState(newRow, column.get());
    else
      mDatabase->AddWorkState(newRow, column.get());
    }
        
    break; // 's'

  case 't':
    if (colType.EqualsLiteral("title"))
      mDatabase->AddJobTitle(newRow, column.get());

    else if (colType.EqualsLiteral("telephonenumber") )
    {
      mDatabase->AddWorkPhone(newRow, column.get());
    }

    break; // 't'

  case 'u':

    if (colType.EqualsLiteral("uniquemember") && bIsList)
      mDatabase->AddLdifListMember(newRow, column.get());

    break; // 'u'

  case 'w':
    if (colType.EqualsLiteral("workurl"))
      mDatabase->AddWebPage1(newRow, column.get());

    break; // 'w'

  case 'x':
    if (colType.EqualsLiteral("xmozillanickname"))
    {
      if (bIsList)
        mDatabase->AddListNickName(newRow, column.get());
      else
        mDatabase->AddNickName(newRow, column.get());
    }

    else if (colType.EqualsLiteral("xmozillausehtmlmail"))
    {
      ToLowerCase(column);
      if (kNotFound != column.Find("true"))
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::html);
      else if (kNotFound != column.Find("false"))
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::plaintext);
      else
        mDatabase->AddPreferMailFormat(newRow, nsIAbPreferMailFormat::unknown);
    }

    break; // 'x'

  case 'z':
    if (colType.EqualsLiteral("zip")) // alias for postalcode
    {
      if (mStoreLocAsHome)
        mDatabase->AddHomeZipCode(newRow, column.get());
      else
        mDatabase->AddWorkZipCode(newRow, column.get());
    }

    break; // 'z'

  default:
    break; // default
  }
}

void nsAbLDIFService::ClearLdifRecordBuffer()
{
  if (!mLdifLine.IsEmpty())
  {
    mLdifLine.Truncate();
    mLFCount = 0;
    mCRCount = 0;
  }
}

// Some common ldif fields, it an ldif file has NONE of these entries
// then it is most likely NOT an ldif file!
static const char *const sLDIFFields[] = {
    "objectclass",
    "sn",
    "dn",
    "cn",
    "givenName",
    "mail",
    nsnull
};
#define kMaxLDIFLen        14

// Count total number of legal ldif fields and records in the first 100 lines of the 
// file and if the average legal ldif field is 3 or higher than it's a valid ldif file.
NS_IMETHODIMP nsAbLDIFService::IsLDIFFile(nsIFile *pSrc, PRBool *_retval)
{
  NS_ENSURE_ARG_POINTER(pSrc);
  NS_ENSURE_ARG_POINTER(_retval);

  *_retval = PR_FALSE;

  nsresult rv = NS_OK;

  nsCOMPtr<nsIInputStream> fileStream;
  rv = NS_NewLocalFileInputStream(getter_AddRefs(fileStream), pSrc);
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsILineInputStream> lineInputStream(do_QueryInterface(fileStream, &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  PRInt32 lineLen = 0;
  PRInt32 lineCount = 0;
  PRInt32 ldifFields = 0;  // total number of legal ldif fields.
  char field[kMaxLDIFLen];
  PRInt32 fLen = 0;
  const char *pChar;
  PRInt32 recCount = 0;  // total number of records.
  PRInt32 i;
  PRBool gotLDIF = PR_FALSE;
  PRBool more = PR_TRUE;
  nsXPIDLCString line;

  while (more && NS_SUCCEEDED(rv) && (lineCount < 100))
  {
    rv = lineInputStream->ReadLine(line, &more);

    if (NS_SUCCEEDED(rv) && more)
    {
      pChar = line.get();
      lineLen = line.Length();
      if (!lineLen && gotLDIF)
      {
        recCount++;
        gotLDIF = PR_FALSE;
      }
                   
      if (lineLen && (*pChar != ' ') && (*pChar != 9))
      {
        fLen = 0;

        while (lineLen && (fLen < (kMaxLDIFLen - 1)) && (*pChar != ':'))
        {
          field[fLen] = *pChar;
          pChar++;
          fLen++;
          lineLen--;
        }
                
        field[fLen] = 0;

        if (lineLen && (*pChar == ':') && (fLen < (kMaxLDIFLen - 1)))
        {
          // see if this is an ldif field (case insensitive)?
          i = 0;
          while (sLDIFFields[i])
          {
            if (!nsCRT::strcasecmp( sLDIFFields[i], field))
            {
              ldifFields++;
              gotLDIF = PR_TRUE;
              break;
            }
            i++;
          }
        }
      }
    }
    lineCount++;
  }

  // If we just saw ldif address, increment recCount.
  if (gotLDIF)
    recCount++;

  rv = fileStream->Close();

  if (recCount > 1)
    ldifFields /= recCount;

  // If the average field number >= 3 then it's a good ldif file.
  if (ldifFields >= 3)
  {
    *_retval = PR_TRUE;
  }

  return rv;
}

void nsAbLDIFService::SplitCRLFAddressField(nsCString &inputAddress, nsCString &outputLine1, nsCString &outputLine2) const
{
  PRInt32 crlfPos = inputAddress.Find("\r\n");
  if (crlfPos != kNotFound)
  {
    inputAddress.Left(outputLine1, crlfPos);
    inputAddress.Right(outputLine2, inputAddress.Length() - (crlfPos + 2));
  }
  else
    outputLine1.Assign(inputAddress);
}

