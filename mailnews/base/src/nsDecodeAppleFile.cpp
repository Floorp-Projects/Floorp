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
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Jean-Francois Ducarroz <ducarroz@netscape.com> 
 */
#ifdef XP_MAC

#include "nsDecodeAppleFile.h"

ProcessAppleDoubleResourceFork::ProcessAppleDoubleResourceFork()
{
  m_rfRefNum = -1;
  m_bNeedToDecodeHeaders = PR_TRUE;
}

ProcessAppleDoubleResourceFork::~ProcessAppleDoubleResourceFork()
{
  if (m_rfRefNum != -1)
    FSClose(m_rfRefNum);
}

OSErr ProcessAppleDoubleResourceFork::Initialize(nsFileSpec& file)
{
  m_fsFileSpec = file.GetFSSpec();
  return FSpOpenRF(&m_fsFileSpec, fsWrPerm, &m_rfRefNum);
}

OSErr ProcessAppleDoubleResourceFork::Write(unsigned char *buffer, PRUint32 bufferSize, PRUint32* writeCount)
{
  /* WARNING: to simplify my life, I presume that I should get all appledouble headers in the first block,
              else I would have to implement a buffer */

  OSErr anErr = noErr;  
  PRUint32 headerSize = 0;

  if (m_bNeedToDecodeHeaders)
  {
    anErr = Decodeheaders(buffer, bufferSize, &headerSize);
    if (anErr != noErr)
      return (anErr);
    m_bNeedToDecodeHeaders = PR_FALSE;
  }

  SInt32 howMuch = bufferSize - headerSize;
  anErr = FSWrite(m_rfRefNum, &howMuch, (const void *)&buffer[headerSize]);
  *writeCount = howMuch + headerSize;

  return noErr;
}

OSErr ProcessAppleDoubleResourceFork::Decodeheaders(unsigned char *buffer, PRUint32 bufferSize, PRUint32* processCount)
{
  ap_header head;
  ap_entry   entries[NUM_ENTRIES + 1];
  
  HFileInfo *fpb;
  CInfoPBRec cipbr;
  
  PRUint32 chunkSize;
  PRUint32 i;
  PRUint32 j;

  unsigned char *buffPtr = buffer;
  PRUint32 remainingCount = bufferSize;

  /*
  **   Read & verify header 
  */
  chunkSize = sizeof(ap_header);
  if (remainingCount < chunkSize)
    return errADNotEnoughData;

  nsCRT::memcpy(&head, buffPtr, chunkSize);
  if(ntohl(head.magic) != APPLEDOUBLE_MAGIC)
    return errADNotSupported;
  
  if (ntohl(head.version) != VERSION) 
    return errADBadVersion;

  head.entries = ntohs(head.entries);

  buffPtr += chunkSize;
  remainingCount -= chunkSize;
  
  /* read entries */
  chunkSize = sizeof(ap_entry);
  
  for (i = 0, j = 0; i < head.entries; i++) 
  {
    if (remainingCount < chunkSize)
      return errADNotEnoughData;

    ap_entry * anEntry = (ap_entry*)buffPtr;
    
    /*
    **  only care about these entries...
    */
    if (j < NUM_ENTRIES) 
      switch (ntohl(anEntry->id)) 
      {
        case ENT_NAME     :
        case ENT_FINFO    :
        case ENT_DATES    :
        case ENT_COMMENT  :
        case ENT_RFORK    :
        case ENT_DFORK    :
          nsCRT::memcpy(&entries[j], buffPtr, chunkSize);
          /*
          **  correct the byte order now.
          */
          entries[j].id     = ntohl(entries[j].id);
          entries[j].offset = ntohl(entries[j].offset);
          entries[j].length = ntohl(entries[j].length);
          
          j ++;
          break;
      }
      
    buffPtr += chunkSize;
    remainingCount -= chunkSize;
  }
  
  fpb = (HFileInfo *) &cipbr;
  fpb->ioVRefNum = m_fsFileSpec.vRefNum;
  fpb->ioDirID   = m_fsFileSpec.parID;
  fpb->ioNamePtr = m_fsFileSpec.name;
  fpb->ioFDirIndex = 0;
  PBGetCatInfoSync(&cipbr);

  /* get finder info */
  for (i = 0; i < j && entries[i].id != ENT_FINFO; ++i)
    ;
  if (i < j)
  {
    if (entries[i].offset + entries[i].length > bufferSize)
       return errADNotEnoughData;

    nsCRT::memcpy(&fpb->ioFlFndrInfo, &buffer[entries[i].offset], sizeof (FInfo));
    nsCRT::memcpy(&fpb->ioFlXFndrInfo, &buffer[entries[i].offset + sizeof (FInfo)], sizeof (FXInfo));
    fpb->ioFlFndrInfo.fdFlags &= 0xfc00; /* clear flags maintained by finder */
  }

  /*
  **   get file date info 
  */
  for (i = 0; i < j && entries[i].id != ENT_DATES; ++i)
    ;
  if (i < j) 
  {
    if (entries[i].offset + entries[i].length > bufferSize)
       return errADNotEnoughData;

    ap_dates *dates = (ap_dates *)&buffer[entries[i].offset];

    fpb->ioFlCrDat = dates->create - CONVERT_TIME;
    fpb->ioFlMdDat = dates->modify - CONVERT_TIME;
    fpb->ioFlBkDat = dates->backup - CONVERT_TIME;
  }
    
  /*
  **   update info 
  */
  fpb->ioDirID = fpb->ioFlParID;
  PBSetCatInfoSync(&cipbr);


  /*
  **   get comment & save it 
  */
  IOParam vinfo;
  GetVolParmsInfoBuffer vp;
  DTPBRec dtp;

  for (i = 0; i < j && entries[i].id != ENT_COMMENT; ++i)
    ;
  if (i < j && entries[i].length != 0) 
  {
    if (entries[i].offset + entries[i].length > bufferSize)
       return errADNotEnoughData;

    nsCRT::zero((void *) &vinfo, sizeof (vinfo));
    vinfo.ioVRefNum = fpb->ioVRefNum;
    vinfo.ioBuffer  = (Ptr) &vp;
    vinfo.ioReqCount = sizeof (vp);
    if (PBHGetVolParmsSync((HParmBlkPtr) &vinfo) == noErr &&
      ((vp.vMAttrib >> bHasDesktopMgr) & 1)) 
    {
      nsCRT::zero((void *) &dtp, sizeof (dtp));
      dtp.ioVRefNum = fpb->ioVRefNum;
      if (PBDTGetPath(&dtp) == noErr) 
      {
        if (entries[i].length > 255) 
          entries[i].length = 255;
          
        dtp.ioDTBuffer = (Ptr) &buffer[entries[i].offset];
        dtp.ioNamePtr  = fpb->ioNamePtr;
        dtp.ioDirID    = fpb->ioDirID;
        dtp.ioDTReqCount = entries[i].length;
        if (PBDTSetCommentSync(&dtp) == noErr) 
        {
          PBDTFlushSync(&dtp);
        }
      }
    }
  }
    
  /* find beginning of resource fork */
  for (i = 0; i < j && entries[i].id != ENT_RFORK; ++i)
      ;
  if (i < j) 
    *processCount = entries[i].offset;

  return noErr;
}

#endif