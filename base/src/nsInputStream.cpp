/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
#include "nsIInputStream.h"
#include "prprf.h"
#include <fcntl.h>
#ifdef XP_PC
#include <io.h>
#else
#include <unistd.h>
#endif
#include <stdlib.h>

// File input stream class
class FileInputStream : public nsIInputStream {
public:
  FileInputStream(PRInt32 aFD);

  NS_DECL_ISUPPORTS
  virtual PRInt32 Read(PRInt32* aErrorCode,
                       char* aBuf,
                       PRInt32 aOffset,
                       PRInt32 aCount);
  virtual void Close();

protected:
  ~FileInputStream();

  PRInt32 mFD;
};

FileInputStream::FileInputStream(PRInt32 aFD)
{
  NS_INIT_REFCNT();
  mFD = aFD;
}

NS_DEFINE_IID(kIInputStreamIID, NS_IINPUTSTREAM_IID);
NS_IMPL_ISUPPORTS(FileInputStream,kIInputStreamIID);

FileInputStream::~FileInputStream()
{
  Close();
}

PRInt32 FileInputStream::Read(PRInt32* aErrorCode,
                              char* aBuf,
                              PRInt32 aOffset,
                              PRInt32 aCount)
{
  NS_PRECONDITION(aOffset >= 0, "illegal offset");
  NS_PRECONDITION(aCount >= 0, "illegal count");
  NS_PRECONDITION(mFD != -1, "closed");
  if ((aOffset < 0) || (aCount < 0)) {
    *aErrorCode = NS_INPUTSTREAM_ILLEGAL_ARGS;
    return -1;
  }
  if (mFD == -1) {
    *aErrorCode = NS_INPUTSTREAM_CLOSED;
    return -1;
  }
  PRInt32 nb = PRInt32( ::read(mFD, aBuf + aOffset, aCount) );
  if (nb <= 0) {
    if (nb < 0) {
      *aErrorCode = NS_INPUTSTREAM_OSERROR;
      return -1;
    }
    *aErrorCode = NS_INPUTSTREAM_EOF;
  }
  return nb;
}

void FileInputStream::Close()
{
  if (-1 != mFD) {
    ::close(mFD);
    mFD = -1;
  }
}

NS_BASE nsresult NS_OpenFile(nsIInputStream** aInstancePtrResult,
                             const char* aLocalFileName)
{
#ifdef XP_PC
  char* lfn = strdup(aLocalFileName);
  char* cp = lfn;
  while ((cp = strchr(cp, '/')) != 0) {
    *cp = '\\';
    cp++;
  }
#endif

#ifdef XP_PC
  PRInt32 fd = ::open(lfn, O_RDONLY | O_BINARY);
  free(lfn);
#else

#ifdef NS_WIN32
  PRInt32 fd = ::open(aLocalFileName, O_RDONLY | O_BINARY);
#endif
#ifdef XP_UNIX
  PRInt32 fd = ::open(aLocalFileName, O_RDONLY);
#endif 

#endif //XP_PC

  if (fd == -1) {
    return NS_INPUTSTREAM_OSERROR;
  }
  FileInputStream* it = new FileInputStream(fd);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIInputStreamIID, (void**)aInstancePtrResult);
}

NS_BASE nsresult NS_OpenResource(nsIInputStream** aInstancePtrResult,
                                 const char* aResourceFileName)
{
  // XXX For now, resources are not in jar files 
  // Find base path name to the resource file
  char* resourceBase;
  char* cp;

#ifdef XP_PC
  // XXX For now, all resources are relative to the .exe file
  resourceBase = new char[2000];
  DWORD mfnLen = GetModuleFileName(NULL, resourceBase, 2000);
  cp = strrchr(resourceBase, '\\');
  if (nsnull != cp) {
    *cp = '\0';
  }
#endif
#ifdef XP_UNIX
  //  FIX ME: write me!;
#endif

  // Join base path to resource name
  if (aResourceFileName[0] == '/') {
    aResourceFileName++;
  }
  PRInt32 baseLen = strlen(resourceBase);
  PRInt32 resLen = strlen(aResourceFileName);
  PRInt32 totalLen = baseLen + 1 + resLen + 1;
  char* fileName = new char[totalLen];
  PR_snprintf(fileName, totalLen, "%s/%s", resourceBase, aResourceFileName);

#ifdef XP_PC
  while ((cp = strchr(fileName, '/')) != 0) {
    *cp = '\\';
    cp++;
  }
#endif

  // Get 
#ifdef NS_WIN32
  PRInt32 fd = ::open(fileName, O_RDONLY | O_BINARY);
#endif
#ifdef XP_UNIX
  PRInt32 fd = ::open(fileName, O_RDONLY);
#endif

  delete fileName;

#ifdef XP_PC
  delete resourceBase;
#endif

  if (fd == -1) {
    return NS_INPUTSTREAM_OSERROR;
  }
  FileInputStream* it = new FileInputStream(fd);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIInputStreamIID, (void**)aInstancePtrResult);
}
