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
#include "MThreadContext.h"
#include "prmem.h"

#include "nsString.h"
#include "nsStringUtil.h"
#include "nsID.h"
#include "prio.h"
#include "prprf.h"

nsIComponentManager* cm=NULL;
char* resFile=(char*)PR_Malloc(sizeof(char*));
char* inFile=(char*)PR_Malloc(sizeof(char*));
char* outFile=(char*)PR_Malloc(sizeof(char*));

#define MT_COMPONENT_LIST_VAR_NAME "MT_COMPONENT_LIST"

int threads=0, stages=0, nRead=0;

MThreadContextImpl::MThreadContextImpl()
{    
    NS_INIT_REFCNT();
    printf("DEbug:avm:MThreadContextImpl::MThreadContextImp\n");
    nsresult rv = NS_GetGlobalComponentManager(&cm);
    if(NS_FAILED(rv)) {
        fprintf(stderr, "ERROR: Can't get GlobalComponentManager!!\n");
	return;
    }

    if(!makeParamFile()) printf("Something is going wrong :-(.\n");

    char* cont=(char*)PR_Malloc(sizeof(char*));
    GetPath(PR_TRUE, &cont);

    int j=0, k=0;
    PRBool isEnd=PR_FALSE;
    for(PRUint32 i=0;i<PL_strlen(cont);i++) {
	switch(cont[i]) {
	  case 10: j++;stages=k;k=0;break;
	  case 13: break;
	  case '-': isEnd=PR_TRUE;break;
	  default: k++;break;
	 }
     threads=j;
     if (isEnd) break;
    }
}

MThreadContextImpl::~MThreadContextImpl()
{
    //nb 
}

NS_IMPL_ISUPPORTS1(MThreadContextImpl, iMThreadContext);

NS_IMETHODIMP MThreadContextImpl::GetStages(int *_retval) {
	*_retval=stages;
	return *_retval;
}

NS_IMETHODIMP MThreadContextImpl::GetThreads(int *_retval) {
        *_retval=threads;
	return *_retval;
}

NS_IMETHODIMP MThreadContextImpl::GetResFile(char **_retval) {
        *_retval=resFile;
	return NS_OK;
}


NS_IMETHODIMP MThreadContextImpl::GetContractID(int i, int j, char **_retstr) {

   nsCID aClass;
   int stg=0, thr=0;
   char* tmp=NULL;
   char* tmp2=NULL;
   char* first=NULL;
   char* tokens[256];
   char* id[256];

   GetStages(&stg);

   if((i<0)||(i>stg)) {
	fprintf(stderr, "\nComponent index is out of array bounds!!!\n");
	*_retstr="NULL";
	return NS_OK;
   }

    char* cont=(char*)PR_Malloc(sizeof(char*));
    GetPath(PR_FALSE, &cont);
    GetThreads(&thr);
    char** paths=(char**)PR_Malloc(sizeof(char*)*thr);
    int jj=0, kk=0;

	tmp=(char*)PR_Malloc(sizeof(char*)*1);
	tmp2=(char*)PR_Malloc(sizeof(char*)*1);
	first=(char*)PR_Malloc(sizeof(char*)*1);

	sprintf(tmp,"%d,%d,",i,j);

	int strs=0;
	for(int h=0;h<nRead;h++) if(cont[h]=='\n') strs++;

	first=strtok(cont,"\n");
	tokens[0]=first;

	for(int g=0;g<strs-1;g++) {
	 tokens[g+1]=strtok(NULL,"\n");
	}

	PRBool is=PR_FALSE;

	for(int hh=0;hh<strs;hh++) {

		char* t1=strtok(tokens[hh],",");
		char* t2=strtok(NULL,",");
		id[hh]=strtok(NULL,",");
		sprintf(tmp2,"%s,%s,",t1,t2);

		if(PL_strncmp(tmp,tmp2,PL_strlen(tmp))==0) {
			*_retstr=id[hh];
			is=PR_TRUE;
		}
	}

	if(!is) {
		*_retstr="NULL";
		return NS_OK;
	}


   nsresult rv = cm->ContractIDToClassID(*_retstr, &aClass);

   if(NS_FAILED(rv)) {
      printf("MThreadContextImpl::GetContractID. Can't convert ContractID to ClassID\n");
      *_retstr = "NULL";
      return NS_OK;
   }

  *_retstr = aClass.ToString();
  return NS_OK;
}

NS_IMETHODIMP MThreadContextImpl::GetComponentManager(nsIComponentManager **_retval) {
    *_retval = cm;
    return NS_OK;
}

NS_IMETHODIMP MThreadContextImpl::GetPath(PRBool in, char **_retval) {

	struct stat st;
	FILE *file = NULL;
	char* content=NULL;
	char* filo=NULL;
	int count = 0;
	
	if (in) filo=inFile;
	   else filo=outFile;

	if(filo == NULL) {
		fprintf(stderr, "ERROR: %s value is'n set\n",MT_COMPONENT_LIST_VAR_NAME);
        	return NULL;
	}

	if (stat(filo, &st) < 0) {
		fprintf(stderr, "ERROR: can't get stat from file %s\n",filo);
		return NULL;
	}
	content = (char*)PR_Malloc(1*(st.st_size+1));
	if ((file = fopen(filo, "r+t")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",filo);
		return NULL;
	}
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
	}

	fclose(file);
        *_retval=content;
	return NS_OK;
}

PRBool MThreadContextImpl::makeParamFile() {
	PRBool isMade=PR_FALSE;
	struct stat st;
	FILE *file=NULL, *file2=NULL;
	char* content=(char*)PR_Malloc(sizeof(char*));
	int j=0, k=0;
	int objects=0;
	char paths[100][100];

	sprintf(inFile,"%s%s",PR_GetEnv(MT_COMPONENT_LIST_VAR_NAME),"threads.dat");
	sprintf(outFile,"%s%s",PR_GetEnv(MT_COMPONENT_LIST_VAR_NAME),"MTTest.lst");
	sprintf(resFile,"%s%s",PR_GetEnv(MT_COMPONENT_LIST_VAR_NAME),"Results.dat");

	if ((file = fopen(inFile, "r+t")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",inFile);
		return isMade;
	}

	stat(inFile,&st);
	content = (char*)PR_Malloc(1*(st.st_size+1));

	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
	}

	int i=0; stages=0; threads=0;
	while(content[i]!='-') {
	 if(content[i]=='\n') threads++;
	 if((content[i]!='\n')&&(content[i]!=13)) stages=stages+1;
	 i++;
	}

	stages=stages/threads;
	
	rewind(file);
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
	}

	i=k=j=0;	

	while(content[i]!='-') {
		switch(content[i]) {
			case '0x0d':break;
			case '\n':j++;k=0;break;
			default:paths[j][k]=content[i];k++;if(content[i]!=' ') objects++;break;
		}
		
		i++;
	}

	
	if ((file2 = fopen(outFile, "w+t")) == NULL) {
		fprintf(stderr, "ERROR: can't open file %s\n",outFile);
		return isMade;
	}

	char t[1];
	char tmp[256];

	rewind(file);
	if ((nRead = fread(content, 1, st.st_size, file)) < st.st_size) {
		fprintf(stderr, "WARNING: can't read entire file in text mode (%d of %d) !\n", nRead, st.st_size);
	}

	content=PL_strpbrk(content,"-");
	int contentLength=PL_strlen(content);
					
	
	for(int ii=0;ii<threads;ii++)
		for(int jj=0;jj<stages;jj++) {
			switch(paths[ii][jj]) {
				case ' ':
				case '\n':break;
				default: {
					sprintf(tmp,"%d",ii);
					fwrite((void*)tmp,1,PL_strlen(tmp),file2);
				
					t[0]=',';
					fwrite((void*)t,1,1,file2);
					sprintf(tmp,"%d",jj);
					fwrite((void*)tmp,1,PL_strlen(tmp),file2);
					fwrite((void*)t,1,1,file2);
					
					char con[256];

					for(int h=0;h<contentLength;h++) {
						if((content[h]==',')&&(paths[ii][jj]==content[h-1])) {
							sprintf(tmp,"%c%c",content[h-1],content[h]);
							sprintf(tmp,"%s",PL_strstr(content,tmp));
							sprintf(tmp,"%s",strtok(tmp,"\n"));
							for(unsigned int hh=0;hh<PL_strlen(tmp);hh++)
								con[hh]=tmp[hh+2];
							con[hh+1]='\0';
							fwrite((void*)con,1,PL_strlen(con),file2);
						}

					}
					
					t[0]='\n';
					fwrite((void*)t,1,1,file2);
				}
			}
		}

	fclose(file);
	fclose(file2);
	isMade=PR_TRUE;

	return isMade;
}
