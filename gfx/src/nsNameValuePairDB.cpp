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

#include "nspr.h"
#include "nsCOMPtr.h"
#include "nsAppDirectoryServiceDefs.h"
#include "nsNameValuePairDB.h"
#include "nsILocalFile.h"

#define NVPDB_VERSION_MAJOR 1
#define NVPDB_VERSION_MINOR 0
#define NVPDB_VERSION_MAINTENANCE   0

#ifdef DEBUG
# define NVPDB_PRINTF(x) \
            PR_BEGIN_MACRO \
              printf x ; \
              printf(", %s %d\n", __FILE__, __LINE__); \
            PR_END_MACRO 
#else
# define NVPDB_PRINTF(x)
#endif

PRBool
nsNameValuePairDB::CheckHeader()
{
  const char *name, *value;
  int num, major, minor, maintenance;
  PRBool foundVersion = PR_FALSE;

  if (!mFile)
    return PR_FALSE;

  if (fseek(mFile, 0L, SEEK_SET) != 0)
    return PR_FALSE;
  mCurrentGroup = 0;
  mAtEndOfGroup = PR_FALSE;
  while (GetNextElement(&name, &value) > 0) {
    if (*name == '\0') // ignore comments
      continue;
    if (strcmp(name, "Version")==0) {
      foundVersion = PR_TRUE;
      num = sscanf(value, "%d.%d.%d", &major, &minor, &maintenance);
      if (num != 3) {
        NVPDB_PRINTF(("failed to parse version number (%s)", value));
        return PR_FALSE;
      }

      // NVPDB_VERSION_MAJOR
      // It is presumed that major versions are not backwards compatibile.
      if (major != NVPDB_VERSION_MAJOR) {
        NVPDB_PRINTF(("version major %d != %d", major, NVPDB_VERSION_MAJOR));
        return PR_FALSE;
      }

      // NVPDB_VERSION_MINOR
      // It is presumed that minor versions are backwards compatible
      // but will have additional features.
      // Put any tests related to minor versions here.

      // NVPDB_VERSION_MAINTENANCE
      // It is presumed that maintenance versions are backwards compatible,
      // have no new features, but can have bug fixes.
      // Put any tests related to maintenance versions here.

      mMajorNum = major;
      mMinorNum = minor;
      mMaintenanceNum = maintenance;
    }
  }

  return foundVersion;
}

//
// Re-get an element. Used if the element is bigger than
// the buffer that was first passed in
//
// PRInt32 GetCurrentElement(const char** aName, const char** aValue,
//                           char *aBuffer, PRUint32 aBufferLen);
//
// to implement this the GetNextElement methods need to save
// the file position so this routine can seek backward to it.
//

PRInt32
nsNameValuePairDB::GetNextElement(const char** aName, const char** aValue)
{
  return GetNextElement(aName, aValue, mBuf, sizeof(mBuf));
}

//
// Get the next element
//
// returns 1 if complete element read
// return  0 on end of file
// returns a negative number on error
//           if error < -NVPDB_MIN_BUFLEN
//               then the value is the negative of the needed buffer len
//
//
PRInt32
nsNameValuePairDB::GetNextElement(const char** aName, const char** aValue,
                                  char *aBuffer, PRUint32 aBufferLen)
{
  char *line, *name, *value;
  unsigned int num;
  int len;
  unsigned int groupNum;

  *aName  = "";
  *aValue = "";

  if (aBufferLen < NVPDB_MIN_BUFLEN) {
    return NVPDB_BUFFER_TOO_SMALL;
  }

  if (mAtEndOfGroup) {
    return NVPDB_END_OF_GROUP;
  }

  //
  // Get a line
  //
  line = fgets(aBuffer, aBufferLen, mFile);
  if (!line) {
    if (feof(mFile)) { // end of file
      mAtEndOfGroup = PR_TRUE;
      mAtEndOfCatalog = PR_TRUE;
      return NVPDB_END_OF_FILE;
    }
    return NVPDB_FILE_IO_ERROR;
  }

  //
  // Check we got a complete line
  //
  len = strlen(line);
  NS_ASSERTION(len!=0, "an empty string is invalid");
  if (len == 0)
    return NVPDB_GARBLED_LINE;
  if (line[len-1] != '\n') {
    len++; // space for the line terminator
    while (1) {
      int val = getc(mFile);
      if (val == EOF)
        return -len;
      len++;
      if (val == '\n')
        return -len;
    }
  }
  len--;
  line[len] = '\0';
  //NVPDB_PRINTF(("line = (%s)", line));

  //
  // Check the group number
  //
  num = sscanf(line, "%u", &groupNum);
  if ((num != 1) || (groupNum != (unsigned)mCurrentGroup))
    return NVPDB_END_OF_GROUP;

  //
  // Get the name
  //
  name = strchr(line, ' ');
  if ((!name) || (name[1]=='\0'))
    return NVPDB_GARBLED_LINE;
  name++;

  //
  // If it is a comment 
  //   return a blank name (strlen(*aName)==0)
  //   return the comment in the value field
  //
  if (*name == '#') {
    *aValue = name;
    return 1;
  }

  //
  // Get the value
  //
  value = strchr(name, '=');
  if (!value)
    return NVPDB_GARBLED_LINE;
  *value = '\0';
  value++;

  //
  // Check for end of group
  //
  if (strcmp(name,"end")==0) {
    mAtEndOfGroup = PR_TRUE;
    return NVPDB_END_OF_GROUP;
  }

  //
  // Got the name and value
  //
  *aName = name;
  *aValue = value;
  return 1;
}

PRBool
nsNameValuePairDB::GetNextGroup(const char** aType)
{
  return GetNextGroup(aType, nsnull, 0);
}

PRBool
nsNameValuePairDB::GetNextGroup(const char** aType, const char* aName)
{
  return GetNextGroup(aType, aName, strlen(aName));
}

PRBool
nsNameValuePairDB::GetNextGroup(const char** aType, const char* aName, int aLen)
{
  const char *name, *value;
  long pos = 0;

  *aType = "";

  if (mAtEndOfCatalog)
    return PR_FALSE;

  //
  // Move to end of current Group
  //
  while (GetNextElement(&name, &value) > 0) 
    continue;
  mCurrentGroup++;
  mAtEndOfGroup = PR_FALSE;
  // save current pos in case this in not the desired type 
  // and we need to backup
  if (aName)
    pos = ftell(mFile);

  // check if there are more Groups
  if (GetNextElement(&name, &value) <= 0) {
    mAtEndOfGroup = PR_TRUE;
    mAtEndOfCatalog = PR_TRUE;
    return PR_FALSE;
  }
  if (strcmp(name,"begin"))
    goto GetNext_Error;

  // check if this is the desired type
  if (aName) {
    if (strncmp(value,aName,aLen)) {
      fseek(mFile, pos, SEEK_SET);
      mCurrentGroup--;
      mAtEndOfGroup = PR_TRUE;
      return PR_FALSE;
    }
  }

  *aType = value;
  return PR_TRUE;

GetNext_Error:
  mError = PR_TRUE;
  NVPDB_PRINTF(("GetNext_Error"));
  return PR_FALSE;
}

nsNameValuePairDB::nsNameValuePairDB()
{
  mFile = nsnull;
  mBuf[0] = '\0';
  mMajorNum = 0;
  mMinorNum = 0;
  mMaintenanceNum = 0;
  mCurrentGroup = 0;
  mAtEndOfGroup = PR_FALSE;
  mAtEndOfCatalog = PR_FALSE;
  mError = PR_FALSE;
}

nsNameValuePairDB::~nsNameValuePairDB()
{
  if (mFile) {
    fclose(mFile);
    mFile = nsnull;
  }
}

PRBool
nsNameValuePairDB::OpenForRead(const nsACString & aCatalogName) // native charset
{
  nsresult result;

  nsCOMPtr<nsILocalFile> local_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID,
                                                        &result);
  if (NS_FAILED(result))
    goto error_return;

  local_file->InitWithNativePath(aCatalogName);
  local_file->OpenANSIFileDesc("r", &mFile);
  if (mFile && CheckHeader())
    return PR_TRUE;

error_return:
  mError = PR_TRUE;
  NVPDB_PRINTF(("OpenForRead error"));
  return PR_FALSE;
}

PRBool
nsNameValuePairDB::OpenTmpForWrite(const nsACString& aCatalogName) // native charset
{
  nsresult result;
  nsCOMPtr<nsILocalFile> local_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID,
                                                        &result);
  if (NS_FAILED(result))
    return PR_FALSE;
  local_file->InitWithNativePath(aCatalogName + NS_LITERAL_CSTRING(".tmp"));
  local_file->OpenANSIFileDesc("w+", &mFile);
  if (mFile == nsnull)
    return PR_FALSE;

  // Write the header
  mAtEndOfGroup = PR_TRUE;
  mCurrentGroup = -1;
  PutStartGroup("Header");
  char buf[64];
  PutElement("", "########################################");
  PutElement("", "#                                      #");
  PutElement("", "#          Name Value Pair DB          #");
  PutElement("", "#                                      #");
  PutElement("", "#   This is a program generated file   #");
  PutElement("", "#                                      #");
  PutElement("", "#             Do not edit              #");
  PutElement("", "#                                      #");
  PutElement("", "########################################");
  PR_snprintf(buf, sizeof(buf), "%d.%d.%d", NVPDB_VERSION_MAJOR,
                NVPDB_VERSION_MINOR, NVPDB_VERSION_MAINTENANCE);
  PutElement("Version", buf);
  PutEndGroup("Header");

  return PR_TRUE;
}

PRBool
nsNameValuePairDB::PutElement(const char* aName, const char* aValue)
{
  if (mAtEndOfGroup) {
    mError = PR_TRUE;
    NVPDB_PRINTF(("PutElement_Error"));
    return PR_FALSE;
  }

  if ((!*aName) && (*aValue == '#'))
    fprintf(mFile, "%u %s\n", mCurrentGroup, aValue);
  else
    fprintf(mFile, "%u %s=%s\n", mCurrentGroup, aName, aValue);
#ifdef DEBUG
  fflush(mFile);
#endif
  return PR_TRUE;
}

PRBool
nsNameValuePairDB::PutEndGroup(const char* aType)
{
  if (mAtEndOfGroup) {
    mError = PR_TRUE;
    NVPDB_PRINTF(("PutEndGroup_Error"));
    return PR_FALSE;
  }

  mAtEndOfGroup = PR_TRUE;
  fprintf(mFile, "%u end=%s\n", mCurrentGroup, aType);
#ifdef DEBUG
  fflush(mFile);
#endif
  return PR_TRUE;
}

PRBool
nsNameValuePairDB::PutStartGroup(const char* aType)
{
  if (!mAtEndOfGroup) {
    mError = PR_TRUE;
    NVPDB_PRINTF(("PutStartGroup_Error"));
#ifdef DEBUG
    fflush(mFile);
#endif
    return PR_FALSE;
  }

  mAtEndOfGroup = PR_FALSE;
  mCurrentGroup++;
  fprintf(mFile, "%u begin=%s\n", mCurrentGroup, aType);
#ifdef DEBUG
  fflush(mFile);
#endif
  return PR_TRUE;
}

PRBool
nsNameValuePairDB::RenameTmp(const char* aCatalogName)
{
  nsresult rv;
  nsCOMPtr<nsILocalFile> dir;
  PRBool exists = PR_FALSE;
  nsCAutoString old_name(aCatalogName);
  nsDependentCString current_name(aCatalogName);
  nsCAutoString tmp_name(aCatalogName);
  nsCAutoString old_name_tail;
  nsCAutoString current_name_tail;
  nsCOMPtr<nsILocalFile> old_file;
  nsCOMPtr<nsILocalFile> current_file;
  nsCOMPtr<nsILocalFile> tmp_file;
  nsCAutoString parent_dir;
  nsCAutoString parent_path;
  nsCAutoString cur_path;

  //
  // Split the parent dir and file name
  //
  PRInt32 slash = 0, last_slash = -1;
  nsCAutoString fontDirName(aCatalogName);
  // RFindChar not coded so do it by hand
  while ((slash=fontDirName.FindChar('/', slash))>=0) {
    last_slash = slash;
    slash++;
  }
  if (last_slash < 0)
    goto Rename_Error;

  fontDirName.Left(parent_dir, last_slash);
  dir = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    goto Rename_Error;
  dir->InitWithNativePath(parent_dir);
  dir->GetNativePath(parent_path);

  if (!mAtEndOfGroup || mError)
    goto Rename_Error;

  //
  // check that we have a tmp copy
  //
  tmp_name.Append(".tmp");
  tmp_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    goto Rename_Error;
  tmp_file->InitWithNativePath(tmp_name);
  tmp_file->Exists(&exists);
  if (!exists)
    goto Rename_Error;

  //
  // get rid of any old copy
  //
  old_name.Append(".old");
  old_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    goto Rename_Error;
  old_file->InitWithNativePath(old_name);

  //
  // Check we have a current copy
  //
  current_file = do_CreateInstance(NS_LOCAL_FILE_CONTRACTID, &rv);
  if (NS_FAILED(rv))
    goto Rename_Error;
  current_file->InitWithNativePath(current_name);
  current_file->Exists(&exists);
  if (exists) {
    //
    // Rename the current copy to old
    //
    current_file->GetNativePath(cur_path);
    old_name.Right(old_name_tail, old_name.Length() - last_slash - 1);
    rv = current_file->MoveToNative(dir, old_name_tail);
    if (NS_FAILED(rv))
      goto Rename_Error;
  }

  //
  // Rename the tmp to current
  //
  current_name_tail = Substring(current_name, last_slash+1,
                                current_name.Length() - (last_slash + 1));
  rv = tmp_file->MoveToNative(dir, current_name_tail);
  if (NS_FAILED(rv))
    goto Rename_Error;

  //
  // remove the previous copy
  //
  if (exists) {
    old_file->Remove(PR_FALSE);
  }

  return PR_TRUE;

Rename_Error:
  mError = PR_TRUE;
  NVPDB_PRINTF(("Rename_Error"));
  return PR_FALSE;
}

