/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): 
 * Client QA Team, St. Petersburg, Russia
 */

#include "prio.h"
//#include "prmem.h"
#include "prprf.h"

//Output methods

#define defaultPerm 00660 
#define defaultFlags PR_CREATE_FILE|PR_RDWR

#define DECL_PROCEED_RESULTS \
NS_IMETHOD PrintResult(char* fileName, const char* result); \
NS_IMETHOD PrintResultArray(char* fileName, PRInt32 count, const char** result); \
NS_IMETHOD PrintResultArray(char* fileName, PRInt32 count, const int* result); \
NS_IMETHOD PrintResultArray(char* fileName, PRInt32 count, const char* result); \
NS_IMETHOD PrintResultMixed(char* fileName, PRBool result1, char result2, PRUint8 result3, \
			    PRInt16 result4, PRUint16 result5, PRInt32 result6, \
			    PRUint32 result7, PRInt64 result8, PRUint64 result9, float result10, \
			    double result11, char* result12, PRInt32 count, PRInt32* result13); \


#define IMPL_PROCEED_RESULTS(_MYCLASS_) \
 \
NS_IMETHODIMP _MYCLASS_::PrintResult(char* fileName,const char* result) { \
 char* fullName = NULL;\
 fullName = PR_sprintf_append(fullName,"%s",logLocation);\
 fullName = PR_sprintf_append(fullName,"/%s",fileName);\
 PRFileDesc* f = PR_Open(fullName,defaultFlags,defaultPerm);\
  if (!f) {\
   fprintf(stderr, "ERROR: File %s is not created", fullName);\
   return NS_OK;\
  }\
  PR_fprintf(f,"%s\n",result);\
  PR_Close(f);\
  PR_smprintf_free(fullName);\
  return NS_OK;\
}\
NS_IMETHODIMP _MYCLASS_::PrintResultArray(char* fileName, PRInt32 count, const char** result) { \
 char* fullName = NULL;\
 fullName = PR_sprintf_append(fullName,"%s",logLocation);\
 fullName = PR_sprintf_append(fullName,"/%s",fileName);\
 PRFileDesc* f = PR_Open(fullName,defaultFlags,defaultPerm);\
  if (!f) {\
   fprintf(stderr, "ERROR: File %s is not created", fullName);\
   return NS_OK;\
  }\
  for(int i=0;i<count;i++)\
    PR_fprintf(f,"%s\n",result[i]);\
  PR_Close(f);\
  PR_smprintf_free(fullName);\
  return NS_OK;\
}\
NS_IMETHODIMP _MYCLASS_::PrintResultArray(char* fileName, PRInt32 count, const int* result) { \
 char* fullName = NULL;\
 fullName = PR_sprintf_append(fullName,"%s",logLocation);\
 fullName = PR_sprintf_append(fullName,"/%s",fileName);\
 PRFileDesc* f = PR_Open(fullName,defaultFlags,defaultPerm);\
  if (!f) {\
   fprintf(stderr, "ERROR: File %s is not created", fullName);\
   return NS_OK;\
  }\
  for(int i=0;i<count;i++)\
    PR_fprintf(f,"%d\n",result[i]);\
  PR_Close(f);\
  PR_smprintf_free(fullName);\
  return NS_OK;\
}\
NS_IMETHODIMP _MYCLASS_::PrintResultArray(char* fileName, PRInt32 count, const char* result) { \
 char* fullName = NULL;\
 fullName = PR_sprintf_append(fullName,"%s",logLocation);\
 fullName = PR_sprintf_append(fullName,"/%s",fileName);\
 PRFileDesc* f = PR_Open(fullName,defaultFlags,defaultPerm);\
  if (!f) {\
   fprintf(stderr, "ERROR: File %s is not created", fullName);\
   return NS_OK;\
  }\
  for(int i=0;i<count;i++)\
    PR_fprintf(f,"%c\n",result[i]);\
  PR_Close(f);\
  PR_smprintf_free(fullName);\
  return NS_OK;\
} \
NS_IMETHODIMP _MYCLASS_::PrintResultMixed(char* fileName, PRBool result1, char result2, PRUint8 result3, \
			    PRInt16 result4, PRUint16 result5, PRInt32 result6, \
			    PRUint32 result7, PRInt64 result8, PRUint64 result9, float result10, \
			    double result11, char* result12, PRInt32 count, PRInt32* result13) { \
 char* fullName = NULL;\
 fullName = PR_sprintf_append(fullName,"%s",logLocation);\
 fullName = PR_sprintf_append(fullName,"/%s",fileName);\
 PRFileDesc* f = PR_Open(fullName,defaultFlags,defaultPerm);\
  if (!f) {\
   fprintf(stderr, "ERROR: File %s is not created", fullName);\
   return NS_OK;\
  }\
  if (result1==PR_TRUE) PR_fprintf(f,"%s\n","true"); else PR_fprintf(f,"%s\n","false");\
  PR_fprintf(f,"%c\n",result2);\
  PR_fprintf(f,"%d\n",result3);\
  PR_fprintf(f,"%d\n",result4);\
  PR_fprintf(f,"%d\n",result5);\
  PR_fprintf(f,"%d\n",result6);\
  PR_fprintf(f,"%d\n",result7);\
  PR_fprintf(f,"%lld\n",result8);\
  PR_fprintf(f,"%llu\n",result9);\
  PR_fprintf(f,"%0.1f\n",result10);\
  PR_fprintf(f,"%0.1f\n",result11);\
  PR_fprintf(f,"%s\n",result12);\
  for(int i=0;i<count;i++)\
    PR_fprintf(f,"%d\n",result13[i]);\
  PR_Close(f);\
  PR_smprintf_free(fullName);\
  return NS_OK;\
}
