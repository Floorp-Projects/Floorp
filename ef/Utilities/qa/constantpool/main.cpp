/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

/* main.cpp for constant pool edit */

#include "ClassReader.h"
#include "FileWriter.h"
#include "nspr.h"
#include "DebugUtils.h"
#include "JavaBytecodes.h"
#include "ClassGen.h"

/* main */
int main(int argc, char **argv)
{
  
  //Check for the correct set of arguments.
  if (argc < 3) {
    printf("Error:\tillegal number of arguments\n");
    printf("Usage:\tconstantpool -d <classname> or\n");
	printf("\t\tconstantpool -m <classname>\n");
    return 1;
  }
  
  CrError status;
  Pool pool;
  StringPool sp(pool);
  char classname[256];
  
  strcpy(classname,argv[2]);
  strcat(classname,".class");
  
  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 1);
  
  //Create instance to read the classfile.
  ClassFileReader reader(pool, sp, classname, &status);

  //Check for dump operation.  
  if (strcmp(argv[1],"-d") == 0) {

	char dumpfile[256];

	strcpy(dumpfile,argv[2]);
	strcat(dumpfile,".txt");
    
    if (status != crErrorNone) {
      printf("Cannot read class file:error (%d)\n", status);
      return 1;

    } else {
      
      //Dump constant pool into a text file.
      reader.dumpClass(dumpfile);

      printf("Read Class File\n");
      return 0;

    } 
  
  } //End if strcmp.

  //Check for merge operation.
  if (strcmp(argv[1],"-m") == 0 ) {
    
     
    char filename[256];
	char classfile[256];

	strcpy(filename,argv[2]);
	strcat(filename,".txt");
	
	strcpy(classfile,"temp/");
	strcat(classfile,argv[2]);
	strcat(classfile,".class");
	
	printf("Using %s to create %s\n",filename,classfile);
    
	//Create instance to write a new classfile.
    FileWriter writer(classfile);
	
	//Open the dumpfile.
	FILE *df;
    df = fopen(filename,"r");
    if (!df) {
      printf("Error: file does not exist.");
      return 1;
    }
    

    /* Create the new classfile. */	  
    
    createClass(df,writer,reader);

    printf("Created new classfile.\n");
    fclose(df);
    return 0;
    
  } //End if strcmp.
  
  printf("Error: don't know what to do.\n");
  return 1;
  
}












