/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *         Brian Stell <bstell@ix.netcom.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef NSNAMEVALUEPAIRDB_H
#define NSNAMEVALUEPAIRDB_H

#include "nsString.h"

#define FC_BUF_LEN 1024

#define NVPDB_MIN_BUFLEN 100

//
// Errors
//
#define NVPDB_END_OF_FILE       0
#define NVPDB_BUFFER_TOO_SMALL -1
#define NVPDB_END_OF_GROUP     -2
#define NVPDB_FILE_IO_ERROR    -3
#define NVPDB_GARBLED_LINE     -4

class nsNameValuePairDB {
public:
  nsNameValuePairDB();
  ~nsNameValuePairDB();

  inline PRBool HadError() { return mError; };
  // implement this to re-read an element if it is larger than the buffer
  // PRInt32 GetCurrentElement(const char** aName, const char** aValue,
  //                           char *aBuffer, PRUint32 aBufferLen);
  PRBool  GetNextGroup(const char** aType);
  PRBool  GetNextGroup(const char** aType, const char* aName);
  PRBool  GetNextGroup(const char** aType, const char* aName, int aLen);
  PRInt32 GetNextElement(const char** aName, const char** aValue);
  PRInt32 GetNextElement(const char** aName, const char** aValue,
                         char *aBuffer, PRUint32 aBufferLen);
  PRBool  OpenForRead(const nsACString& aCatalogName);     // native charset
  PRBool  OpenTmpForWrite(const nsACString& aCatalogName); // native charset
  PRBool  PutElement(const char* aName, const char* aValue);
  PRBool  PutEndGroup(const char* aType);
  PRBool  PutStartGroup(const char* aType);
  PRBool  RenameTmp(const char* aCatalogName);

protected:
  PRBool         CheckHeader();
  PRUint16       mMajorNum;
  PRUint16       mMinorNum;
  PRUint16       mMaintenanceNum;
  FILE*          mFile;
  char           mBuf[FC_BUF_LEN];
  PRInt32        mCurrentGroup;
  PRPackedBool   mAtEndOfGroup;
  PRPackedBool   mAtEndOfCatalog;
  PRPackedBool   mError;
private:
};

#endif /* NSNAMEVALUEPAIRDB_H */

