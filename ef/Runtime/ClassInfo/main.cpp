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
/* Test program for ClassFileSummary */

#include "prinit.h"
#include "ClassFileSummary.h"
#include "ClassCentral.h"
#include "ErrorHandling.h"

char *fileName = 0;
char *className = 0;
char *classPath = 0;

void printUsage()
{
  printf("Usage: classinfo [-f filename] [-cpath classpath] classname\n");
  exit(0);
}

void parseOptions(int argc, char **argv)
{
  int i;

  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-f"))
      fileName = argv[++i];
    else if (!strcmp(argv[i], "-cpath"))
      classPath = argv[++i];
    else if (!strcmp(argv[i], "-help"))
      printUsage();
    else if (className) {/* must be a classname */
      printf("Syntax error or duplicate filename\n");
      printUsage();
    } else
      className = argv[i];

  if (!className) {
    printf("Insufficient arguments\n");
    printUsage();
  }
    
}

Pool dynamicPool;
ClassWorld world(dynamicPool);

int main(int argc, char **argv)
{
  int i;

#ifndef NO_NSPR
  PR_Init(PR_USER_THREAD, PR_PRIORITY_NORMAL, 1);
#endif

  parseOptions(argc, argv);
  
  // Pool dynamicPool;
  StringPool sp(dynamicPool);
  // ClassWorld world(dynamicPool);

  ClassCentral central(dynamicPool, world, sp, classPath);

  printf("Initializing distinguished objects....\n");
  try {
    initDistinguishedObjects(central);
  } catch (VerifyError err) {
    printf("Error initializing distinguished objects: %d\n", err.cause);
    exit(1);
  }
  printf("Initialized distinguished objects!\n");

  ClassFileSummary *info;
  const ConstantPool *constantPool;
  int constantPoolCount;

  printf("Initialized class central\n");

  try {
    if (fileName) {
      printf("Loading from file %s\n", fileName);
      info = &central.addClass((const char *) className, 
			       (const char *) fileName);
    } else 
      info = &central.addClass((const char *) className);
    
  } catch (VerifyError err) {
    printf("Cannot load class %s: error %d\n", className, err.cause);
    exit(1);
  }

  constantPool = info->getConstantPool();
  constantPoolCount = info->getConstantPoolCount();

  printf("Total size of object : %d\n", info->getObjSize());

  /* Resolve fields, methods, constants, interfaceMethods */
  for (i = 0; i < constantPoolCount; i++) 
    if (constantPool->get(i)) {
      uint8 type = constantPool->get(i)->getType();

      if (type == CR_CONSTANT_FIELDREF) {
	ConstantFieldRef *ref = (ConstantFieldRef *) constantPool->get(i);
	ConstantNameAndType *nameAndType = (ConstantNameAndType *) ref->getTypeInfo();
	ConstantClass *cclass = ref->getClassInfo();

	printf("--> Resolving field %s::%s...\n", 
	       cclass->getUtf()->getUtfString(),
	       nameAndType->getName()->getUtfString());

	uint32 offset;
	bool isVolatile, isConstant;
	bool isStatic = false;
	TypeKind tk;
	ValueKind vk;

	try {
	  tk = info->lookupInstanceField(i, vk, 
					 offset, isVolatile, isConstant); 
	} catch (VerifyError err) {
	  printf("\tlookupInstanceField() failed with error %d.\n", 
		 err.cause);
	  printf("\tOk, so it's not an instance field. Trying static.\n");
	  addr a;

	  try {
	    tk = info->lookupStaticField(i, vk, a, isVolatile, isConstant);
	    isStatic = true;
	  } catch (VerifyError err) {
	    printf("Cannot resolve field %s: %d\n", 
		   nameAndType->getName()->getUtfString(), err.cause);
	  }
	}
	
	if (isStatic)
	  printf("Resolved as static field.\n");
	else
	  printf("Resolved as instance field with offset %u\n", offset);
	
      } else if (type == CR_CONSTANT_METHODREF) {
	ConstantMethodRef *ref = (ConstantMethodRef *) constantPool->get(i);
	ConstantNameAndType *nameAndType = (ConstantNameAndType *) ref->getTypeInfo();
	ConstantClass *cclass = ref->getClassInfo();

	const char *methodName = nameAndType->getName()->getUtfString();

	printf("--> Resolving method %s::%s...\n", 
	       cclass->getUtf()->getUtfString(),
	       methodName);
   
	uint32 vIndex;
	addr a;
	Signature sig;
	bool dynamicallyResolved = false;

	if (!strcmp(methodName, "<init>") || !strcmp(methodName, "<clinit>")) {
	  try {
	    info->lookupSpecialMethod(i, sig, a);

	    printf("\tResolved special method %s.\n", methodName);	    

	  } catch (VerifyError err) {
	    printf("Could not resolve special method %s: error %d\n", 
		   methodName, err.cause);
	    exit(1);
	  }
	} else {
	  try {
	    dynamicallyResolved = 
	      info->lookupVirtualMethod(i, sig, vIndex, a);
	  } catch (VerifyError err) {
	    printf("\tlookupVirtualMethod() failed with erorr %d\n",
		   err.cause);
	    printf("\tOk, so method is not virtual. trying static...\n");
	    
	    try {
	      info->lookupStaticMethod(i, sig, a);
	    } catch (VerifyError err) {
	      printf("Cannot resolve method %s: %d\n",
		     nameAndType->getName()->getUtfString(), err.cause);
	      exit(1);
	    }
	    
	  }
	}

	if (dynamicallyResolved)
	  printf("\tResolved method %s as virtual: vindex %u.\n", 
		 nameAndType->getName()->getUtfString(), vIndex);
	else {
	  printf("\tResolved method %s statically\n",
		 nameAndType->getName()->getUtfString());
	}
		
      } else if (type == CR_CONSTANT_INTERFACEMETHODREF) {
	ConstantMethodRef *ref = (ConstantMethodRef *) constantPool->get(i);
	ConstantNameAndType *nameAndType = (ConstantNameAndType *) ref->getTypeInfo();
	ConstantClass *cclass = ref->getClassInfo();

	const char *methodName = nameAndType->getName()->getUtfString();
	printf("--> Resolving interface method %s::%s...\n", 
	       cclass->getUtf()->getUtfString(),
	       methodName);

	Signature sig;
	uint32 interfaceIndex;
	addr a;
	uint32 nArgs;

	if (!strcmp(methodName, "<init>") || !strcmp(methodName, "<clinit>")) {
	  try {
	    info->lookupSpecialMethod(i, sig, a);
	    
	    printf("\tResolved special method %s.\n", methodName);	    

	  } catch (VerifyError err) {
	    printf("Could not resolve special method %s: error %d\n", 
		   methodName, err.cause);
	    exit(1);
	  }
	} else {
	  try {
	    bool staticallyResolved = info->lookupInterfaceMethod(i, sig,
								  interfaceIndex, 
								  a,  
								  nArgs);
	    printf("\tResolved interface method %s %s.\n", 
		   nameAndType->getName()->getUtfString(),
		   (staticallyResolved) ? "statically" : "dynamically");	  
	  } catch (VerifyError err) {
	    printf("\tCould not resolve interface method %s: error %d\n", 
		   nameAndType->getName()->getUtfString(), err.cause);
	    exit(1);
	  }
	}
      }
    }

  printf("Successfully loaded class file. \n");  
  
  printf("Getting class descriptor...\n");
  Class *clazz = static_cast<Class *>(info->getThisClass());
  printf("Obtained class descriptor.\n");

  /* Now look up all the interfaces this class implements */
  printf("Loading interfaces...\n");
  const Interface **interfaces = info->getInterfaces();

  printf("Done!\n");
  return 0;
}


#ifdef DEBUG_LAURENT 
#ifdef __GNUC__
// for gcc with -fhandle-exceptions
void terminate() {}
#endif
#endif
