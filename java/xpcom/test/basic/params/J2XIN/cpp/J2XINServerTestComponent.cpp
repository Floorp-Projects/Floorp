/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-

 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s): 
 Client QA Team, St. Petersburg, Russia
*/

#include "plstr.h"
#include "prenv.h"
#include "stdio.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "J2XINServerTestComponent.h"
#include "prmem.h"
#include "nsString.h"
#include "nsStringUtil.h"

char* testLocation=NULL;
char* logLocation=NULL;
char* fBuffer=NULL;

J2XINServerTestComponentImpl::J2XINServerTestComponentImpl()
{    
    NS_INIT_REFCNT();
    printf("DEbug:avm:J2XINServerTestComponentImpl::J2XINServerTestComponentImp\n");
    testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
    logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
    if ((logLocation == NULL)||(testLocation == NULL)) {
        fprintf(stderr,"ERROR: %s or %s isn't set !", BC_TEST_LOCATION_VAR_NAME, BC_LOG_LOCATION_VAR_NAME);
    }
}

J2XINServerTestComponentImpl::~J2XINServerTestComponentImpl()
{
    //nb 
}

NS_IMPL_ISUPPORTS1(J2XINServerTestComponentImpl, iJ2XINServerTestComponent);
IMPL_PROCEED_RESULTS(J2XINServerTestComponentImpl)


NS_IMETHODIMP J2XINServerTestComponentImpl::GetTestLocation(char **tLocation,char **lLocation) {
    *tLocation = testLocation;
    *lLocation = logLocation;
    return NS_OK;
}
NS_IMETHODIMP J2XINServerTestComponentImpl::Flush(const char *type) {
    char* fileName = NULL;
    fileName = PR_sprintf_append(fileName,"j2x.in.server.%s",type);
    fprintf(stderr,"Flushing to %s\n", fileName);
    if(fBuffer) {
        PrintResult(fileName,fBuffer);
        PR_smprintf_free(fBuffer);
        fBuffer = NULL;
    } else {
        fprintf(stderr,"ERROR!!fBuffer is null at flush stage\n");
    }
    PR_smprintf_free(fileName);
    if(fBuffer != NULL ) {
        fprintf(stderr,"fBuffer not NULL after free\n");
    }else {
        fprintf(stderr,"fBuffer IS NULL after free\n");
    }
    return NS_OK;
}


//Test methods


  /* void TestShort (in short i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestShort(PRInt16 i) {
    fprintf(stderr,"Inside TestShort method\n");
    fBuffer = PR_sprintf_append(fBuffer,"%d\n",i);
    return NS_OK;
}

  /* void TestLong (in long i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestLong(PRInt32 i) {
    fprintf(stderr,"Inside TestLong method \n");
    if(fBuffer != NULL ) {
        fprintf(stderr,"fBuffer not NULL in TestLong\n");
    } else {
        fprintf(stderr,"fBuffer IS NULL in TestLong\n");
    }
    fBuffer = PR_sprintf_append(fBuffer,"%ld\n",i);
    return NS_OK;
}

  /* void TestLonglong (in long long i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestLonglong(PRInt64 i) {
    printf("Inside TestLonglong method\n");
    fBuffer = PR_sprintf_append(fBuffer,"%lld\n",i);
    return NS_OK;
}

  /* void TestByte (in octet i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestByte(PRUint8 i) {
    printf("TestByte\n");
    fBuffer = PR_sprintf_append(fBuffer,"%d\n",i);
    return NS_OK;
}

  /* void TestUShort (in unsigned short i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestUShort(PRUint16 i) {
    printf("TestUShort\n");
    fBuffer = PR_sprintf_append(fBuffer,"%u\n",i);
    return NS_OK;
}

  /* void TestULong (in unsigned long i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestULong(PRUint32 i) {
    printf("TestULong\n");
    fBuffer = PR_sprintf_append(fBuffer,"%lu\n",i);
    return NS_OK;
}


  /* void TestULonglong (in unsigned long long i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestULonglong(PRUint64 i) {
    printf("TestULonglong\n");
    fBuffer = PR_sprintf_append(fBuffer,"%llu\n",i);
    return NS_OK;
}

  /* void TestFloat (in float i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestFloat(float i) {
    fprintf(stderr,"TestFloat: append %.3e\n",i);
    fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",i);
    return NS_OK;
}

  /* void TestDouble (in double i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestDouble(double i) {
    fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",i);
    return NS_OK;
}

  /* void TestBoolean (in boolean i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestBoolean(PRBool i) {
    if(i == PR_TRUE) {
        fBuffer = PR_sprintf_append(fBuffer,"%s\n","true");
    } else {
        fBuffer = PR_sprintf_append(fBuffer,"%s\n","false");
    }
    return NS_OK;
}

/*
NS_IMETHODIMP J2XINServerTestComponentImpl::TestChar(char i) {
    fBuffer = PR_sprintf_append(fBuffer,"%c\n",i);
    return NS_OK;
}

  
NS_IMETHODIMP J2XINServerTestComponentImpl::TestWChar(PRUnichar i) {
    fBuffer = PR_sprintf_append(fBuffer,"%d\n",i);
    return NS_OK;
}
*/
  /* void TestString (in string i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestString(const char *i) {
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",i);
    return NS_OK;
}

  /* void TestWString (in wstring i); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestWString(const PRUnichar *i) {
    nsString str = *(new nsString(i));
    NS_ALLOC_STR_BUF(aBuf,str,100)
    printf("aBuf is %s",aBuf);
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",aBuf);
    NS_FREE_STR_BUF(aBuf)
    return NS_OK;
}

  /* void TestStringArray (in unsigned long count, [array, size_is (count)] in string valueArray); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestStringArray(PRUint32 count, const char **valueArray) {
    return PrintResultArray("j2x.in.server.stringArray",count,valueArray);
}

  /* void TestLongArray (in unsigned long count, [array, size_is (count)] in long longArray); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestLongArray(PRUint32 count, PRInt32 *longArray) {
    return PrintResultArray("j2x.in.server.longArray",count,longArray);
}

  /* void TestCharArray (in unsigned long count, [array, size_is (count)] in char valueArray); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestCharArray(PRUint32 count, char *valueArray) {
    return PrintResultArray("j2x.in.server.charArray",count,valueArray);;
}

  /* void TestMixed (in boolean bBool, in char cChar, in octet nByte, in short nShort, in unsigned short nUShort, in long nLong, in unsigned long nULong, in long long nHyper, in unsigned long long nUHyper, in float fFloat, in double fDouble, in string aString, in unsigned long count, [array, size_is (count)] in long longArray); */

NS_IMETHODIMP J2XINServerTestComponentImpl::TestMixed(PRBool bBool,/* char cChar,*/ PRUint8 nByte, PRInt16 nShort, PRUint16 nUShort, PRInt32 nLong, PRUint32 nULong, PRInt64 nHyper, PRUint64 nUHyper, float fFloat, double fDouble, const char *aString, PRUint32 count, PRInt32 *longArray) {
    char cChar = '0';
    return PrintResultMixed("j2x.in.server.mixed",(PRBool)bBool,(char)cChar,(PRUint8)nByte,(PRInt16)nShort, (PRUint16)nUShort, (PRInt32)nLong, (PRUint32)nULong,  (PRInt64)nHyper, (PRUint64)nUHyper, (float)fFloat, (double)fDouble, (char*)aString, (PRUint32)count, (PRInt32*)longArray);
}

  /* void TestObject (in iJ2XINServerTestComponent obj); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestObject(iJ2XINServerTestComponent *obj) {
    char *str;
    obj->GetTestObjectString(&str);
    PrintResult("j2x.in.server.object",str);
    return NS_OK;
}
NS_IMETHODIMP J2XINServerTestComponentImpl::GetTestObjectString(char **str) {
    *str = "String Ident";
    return NS_OK;
}

  /* void TestIID (in nsIIDRef iid); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestIID(const nsIID& iid) {
    fprintf(stderr,"TestIID: append %s\n",iid.ToString());
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",iid.ToString());
    return NS_OK;
}

  /* void TestCID (in nsCIDRef cid); */
NS_IMETHODIMP J2XINServerTestComponentImpl::TestCID(const nsCID& cid) {
    fprintf(stderr,"TestCID: append %s\n",cid.ToString());
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",cid.ToString());
    return NS_OK;
}
