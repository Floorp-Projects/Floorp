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
#include "stdio.h"
#include "nsMemory.h"
#include "BCTest.h"
#include <sys/stat.h>
#include <stdlib.h>
#include "J2XINOUTServerTestComponent.h"
#include "prmem.h"
#include "prenv.h"
#include "nsString.h"
#include "nsStringUtil.h"

char* testLocation=NULL;
char* logLocation=NULL;
char* fBuffer=NULL;
PRUint8 end_of_data = 112;
int all=0;


J2XINOUTServerTestComponentImpl::J2XINOUTServerTestComponentImpl()
{    
    NS_INIT_REFCNT();
    testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
    logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
    if((testLocation == NULL)||(logLocation == NULL)) {
        fprintf(stderr,"ERROR: %s or %s isn't set !", BC_TEST_LOCATION_VAR_NAME, BC_LOG_LOCATION_VAR_NAME);
    }
    InitStackVars();
    printf("DEbug:avm:J2XINOUTServerTestComponentImpl::J2XINOUTServerTestComponentImp\n");
}

J2XINOUTServerTestComponentImpl::~J2XINOUTServerTestComponentImpl()
{
}

NS_IMPL_ISUPPORTS1(J2XINOUTServerTestComponentImpl, iJ2XINOUTServerTestComponent);
IMPL_PROCEED_RESULTS(J2XINOUTServerTestComponentImpl)
IMPL_VAR_STACKS(J2XINOUTServerTestComponentImpl)

NS_IMETHODIMP J2XINOUTServerTestComponentImpl::GetTestLocation(char **tLocation, char **lLocation) {
    *tLocation = testLocation;
    *lLocation = logLocation;
    return NS_OK;
}

NS_IMETHODIMP J2XINOUTServerTestComponentImpl::Flush(const char *type) {
    char* fileName = NULL;

    fileName = PR_sprintf_append(fileName,"j2x.inout.server.%s",type);
    if(fBuffer) {
        PrintResult(fileName,fBuffer);
        PR_smprintf_free(fBuffer);
        fBuffer = NULL;
    } else {
    }
  PR_smprintf_free(fileName);

    if(fBuffer != NULL ) {
    }else {
    }
    all=0;

    return NS_OK;
}


//Test methods



NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestShort(PRInt16 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
      else Flush("short");
    return NS_OK;

}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestLong(PRInt32 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
      else Flush("long");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestLonglong(PRInt64 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%lld\n",*i);
      else Flush("longlong");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestByte(PRUint8 *i) {
// Strange bug here
/*    if (*i!=112)*/ fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
    all++;
    if (all==4) Flush("octet");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestUShort(PRUint16 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
      else Flush("ushort");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestULong(PRUint32 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
      else Flush("ulong");
    return NS_OK;
}


  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestULonglong(PRUint64 *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%lld\n",*i);
      else Flush("ulonglong");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestFloat(float *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
      else Flush("float");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestDouble(double *i) {
    if (*i!=112) fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
      else Flush("double");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestBoolean(PRBool *i) {
    if (*i==PR_TRUE) fBuffer = PR_sprintf_append(fBuffer,"%s\n","true");
      else if (*i==PR_FALSE) fBuffer = PR_sprintf_append(fBuffer,"%s\n","false");
	else fBuffer = PR_sprintf_append(fBuffer,"%s\n","strange value");
    all++;
    if (all==2) Flush("boolean");
    return NS_OK;
}


NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestChar(char *i) {
// Strange bug here
/*    if (*i!='x')*/ fBuffer = PR_sprintf_append(fBuffer,"%c\n",i);
	all++;
     if (all==2) Flush("char");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestWChar(PRUnichar *i) {
// Strange bug here
/*    if (*i!='x')*/ fBuffer = PR_sprintf_append(fBuffer,"%c\n",i);
	all++;
    if (all==2) Flush("wchar");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestString(char **i) {
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",*i);
    all++;
    if (all==3) Flush("string");
  return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestWString(PRUnichar **i) {
    
    nsString str = *(new nsString(*i));
    {
    NS_ALLOC_STR_BUF(aBuf,str,100)
    fBuffer = PR_sprintf_append(fBuffer,"%s\n",aBuf);
    NS_FREE_STR_BUF(aBuf)
    }
	all++;
      if (all==3) Flush("wstring");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestStringArray(PRUint32 count, char ***valueArray) {
    for(int i=0;i<count;i++)
      fBuffer = PR_sprintf_append(fBuffer,"%s\n",valueArray[0][i]);
    Flush("stringArray");
    return NS_OK;//PrintResultArray("j2x.in.server.stringArray",count,valueArray);
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestLongArray(PRUint32 count, PRInt32 **longArray) {
    for(int i=0;i<count;i++)
      fBuffer = PR_sprintf_append(fBuffer,"%d\n",longArray[i]);
    Flush("longArray");
    return NS_OK;//PrintResultArray("j2x.in.server.longArray",count,longArray);
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestCharArray(PRUint32 count, char **valueArray) {
    for(int i=0;i<count;i++)
      fBuffer = PR_sprintf_append(fBuffer,"%c\n",valueArray[i]);
    Flush("charArray");
    return NS_OK;//PrintResultArray("j2x.in.server.charArray",count,valueArray);;
}

  

NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestMixed(PRBool *bBool, char *cChar, PRUint8 *nByte, PRInt16 *nShort, PRUint16 *nUShort, PRInt32 *nLong, PRUint32 *nULong, PRInt64 *nHyper, PRUint64 *nUHyper, float *fFloat, double *fDouble, char **aString, PRUint32 count, PRInt32 **longArray) {
    fBuffer = PR_sprintf_append(fBuffer,"%d\n%d\n%d",nByte, nShort, nUShort);
    Flush("mixed");
    return NS_OK;
}

  
NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestObject(iJ2XINOUTServerTestComponent **obj) {
    obj[0]->TestObj();
    return NS_OK;
}

NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestObj() {
    fBuffer = PR_sprintf_append(fBuffer,"!!!Right string!!!");    	
    Flush("object");
    return NS_OK;
}

NS_IMETHODIMP J2XINOUTServerTestComponentImpl::TestObj2() {
    fBuffer = PR_sprintf_append(fBuffer,"!!!Right string!!!");    	

    char* fileName = NULL;

    fileName = PR_sprintf_append(fileName,"j2x.inout.xclient.object");
    if(fBuffer) {
        PrintResult(fileName,fBuffer);
        PR_smprintf_free(fBuffer);
        fBuffer = NULL;
    } else {
    }
  PR_smprintf_free(fileName);


    return NS_OK;
}
