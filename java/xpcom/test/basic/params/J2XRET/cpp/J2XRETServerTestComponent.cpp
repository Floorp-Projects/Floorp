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
#include "J2XRETServerTestComponent.h"
#include "prmem.h"
#include "nsString.h"
#include "nsStringUtil.h"


char* testLocation=NULL;
char* logLocation=NULL;
char* fBuffer=NULL;
PRUint8 end_of_data = 112;

int all=0;


J2XRETServerTestComponentImpl::J2XRETServerTestComponentImpl()
{    
    NS_INIT_REFCNT();
    testLocation=PR_GetEnv(BC_TEST_LOCATION_VAR_NAME);
    logLocation=PR_GetEnv(BC_LOG_LOCATION_VAR_NAME);
    InitStackVars();
    printf("DEbug:avm:J2XRETServerTestComponentImpl::J2XRETServerTestComponentImp\n");
}

J2XRETServerTestComponentImpl::~J2XRETServerTestComponentImpl()
{
}

NS_IMPL_ISUPPORTS1(J2XRETServerTestComponentImpl, iJ2XRETServerTestComponent);
IMPL_PROCEED_RESULTS(J2XRETServerTestComponentImpl)
IMPL_VAR_STACKS(J2XRETServerTestComponentImpl)

NS_IMETHODIMP J2XRETServerTestComponentImpl::GetTestLocation(char **tLocation, char **lLocation) {
    *tLocation = testLocation;
    *lLocation = logLocation;
    return NS_OK;
}

NS_IMETHODIMP J2XRETServerTestComponentImpl::Flush(const char *type) {
    char* fileName = NULL;

    fileName = PR_sprintf_append(fileName,"j2x.ret.server.%s",type);
    if(fBuffer) {
        PrintResult(fileName,fBuffer);
	PR_smprintf_free(fileName);
        PR_smprintf_free(fBuffer);
        fBuffer = NULL;
    } else {
    }
    if(fBuffer != NULL ) {
    }else {
    }
    all=0;

    return NS_OK;
}


NS_IMETHODIMP J2XRETServerTestComponentImpl::TestShort(PRInt16 *i) {
    if(PRInt16Vars.size()) {
        *i = PRInt16Vars.top();
         PRInt16Vars.pop();
         fBuffer = PR_sprintf_append(fBuffer,"%hd\n",*i);
    } else {
        *i = end_of_data;
	Flush("short");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestLong(PRInt32 *i) {
    if(PRInt32Vars.size()) {
        *i = PRInt32Vars.top();
        PRInt32Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%ld\n",*i);
    } else {
        *i = end_of_data;
	Flush("long");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestLonglong(PRInt64 *i) {
    if(PRInt64Vars.size()) {
        *i = PRInt64Vars.top();
        PRInt64Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%lld\n",*i);
    } else {
        *i = end_of_data;
	Flush("longlong");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestByte(PRUint8 *i) {
   if(PRUint8Vars.size()) {
       *i = PRUint8Vars.top();
       PRUint8Vars.pop();
         fBuffer = PR_sprintf_append(fBuffer,"%d\n",*i);
   } else {
       *i = end_of_data;
	Flush("octet");
   }
   return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestUShort(PRUint16 *i) {
    if(PRUint16Vars.size()) {
        *i = PRUint16Vars.top();
        PRUint16Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%hu\n",*i);
    } else {
        *i = end_of_data;
	Flush("ushort");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestULong(PRUint32 *i) {
    if(PRUint32Vars.size()) {
        *i = PRUint32Vars.top();
        PRUint32Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%lu\n",*i);
    } else {
        *i = end_of_data;
	Flush("ulong");
    }
    return NS_OK;
}


  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestULonglong(PRUint64 *i) {
    if(PRUint64Vars.size()) {
        *i = PRUint64Vars.top();
        PRUint64Vars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%llu\n",*i);
    } else {
        *i = end_of_data;
	Flush("ulonglong");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestFloat(float *i) {
    if(floatVars.size()) {
        *i = floatVars.top();
        floatVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
    } else {
        *i = end_of_data;
	Flush("float");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestDouble(double *i) {
    if(doubleVars.size()) {
        *i = doubleVars.top();
        doubleVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%.3e\n",*i);
    } else {
        *i = end_of_data;
	Flush("double");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestBoolean(PRBool *i) {
    if(PRBoolVars.size()) {
        *i = PRBoolVars.top();
        PRBoolVars.pop();
        if(*i == PR_TRUE) {
            fBuffer = PR_sprintf_append(fBuffer,"%s\n","true");
        }else if (*i == PR_FALSE) {
            fBuffer = PR_sprintf_append(fBuffer,"%s\n","false");
        }
    } else Flush("boolean");
    return NS_OK;
}
  

NS_IMETHODIMP J2XRETServerTestComponentImpl::TestChar(char *i) {
    if(charVars.size()) {
        *i = charVars.top();
        charVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%c\n",*i);
    } else {
        *i = end_of_data;
	Flush("char");
    }
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestWChar(PRUnichar *i) {
    if(wcharVars.size()) {
        *i = wcharVars.top();
        wcharVars.pop();
        fBuffer = PR_sprintf_append(fBuffer,"%c\n",*i);
    } else {
        *i = end_of_data;
	Flush("wchar");
    }
    return NS_OK;
}

    
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestString(char* *i) {

   if(stringVars.size()) {
       *i = stringVars.top();
        stringVars.pop();
	fBuffer = PR_sprintf_append(fBuffer,"%s\n",*i);
    } else {
	*i="";
	fBuffer = PR_sprintf_append(fBuffer,"%s",*i);
	Flush("string");
      }

   return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestWString(PRUnichar* *i) {
char* _string="";

    if (all==0) {
     _string="";
     nsCString* cstr=new nsCString(_string);
     *i=cstr->ToNewUnicode();
     nsString str = *(new nsString(*i));
     {
      NS_ALLOC_STR_BUF(aBuf,str,100)
      fBuffer=PR_sprintf_append(fBuffer,"%s\n",aBuf);
      NS_FREE_STR_BUF(aBuf)
     }
    }
    if (all==1) {
     _string="Test string.";
     nsCString* cstr=new nsCString(_string);
     *i=cstr->ToNewUnicode();
     nsString str = *(new nsString(*i));
     {
      NS_ALLOC_STR_BUF(aBuf,str,100)
      fBuffer=PR_sprintf_append(fBuffer,"%s\n",aBuf);
      NS_FREE_STR_BUF(aBuf)
     }
    }
    if (all==2) { 
     _string="null string";
     nsCString* cstr=new nsCString(_string);
     *i=cstr->ToNewUnicode();
     nsString str = *(new nsString(*i));
     {
      NS_ALLOC_STR_BUF(aBuf,str,100)
      fBuffer=PR_sprintf_append(fBuffer,"%s\n",aBuf);
      NS_FREE_STR_BUF(aBuf)
     }
     Flush("wstring");
    }

    all++;
    return NS_OK;
} 

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestStringArray(PRUint32 count, char ***valueArray) {

  char** _ret=(char**)PR_Malloc(sizeof(char*)*3);
  _ret[0]="ctrl-f";
  _ret[1]="iddqd";
  _ret[2]="idkfa";


    for(int j=0;j<count;j++)
      fBuffer = PR_sprintf_append(fBuffer,"%s\n",_ret[j]);
    Flush("stringArray");

  *valueArray=_ret;

    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestLongArray(PRUint32 count, int **intA) {

  int *ret=(int*)PR_Malloc(sizeof(int)*3);
       ret[0]=33;
       ret[1]=44;
       ret[2]=55;

    for(int j=0;j<count;j++)
      fBuffer = PR_sprintf_append(fBuffer,"%d\n",ret[j]);
    Flush("longArray");

    *intA=ret;
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestCharArray(PRUint32 count, char **valueArray) {

    char* ret=(char*)PR_Malloc(sizeof(char)*3);
        ret[0]='x';
        ret[1]='y';
        ret[2]='z';

    for(int j=0;j<count;j++)
      fBuffer = PR_sprintf_append(fBuffer,"%c\n",ret[j]);
    Flush("charArray");

    *valueArray=ret;
    return NS_OK;
}

  
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestObject(iJ2XRETServerTestComponent **obj) {
    *obj=this;
    return NS_OK;
} 
    
NS_IMETHODIMP J2XRETServerTestComponentImpl::TestObj() {
    fBuffer = PR_sprintf_append(fBuffer,"!!!Right string!!!");
    Flush("object");
    return NS_OK;
} 
