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
#include "HeaderGenerator.h"
#include "plstr.h"
#include "prprf.h"

#include "CUtils.h"
#include "StringUtils.h"
#include "Pool.h"
#include "FieldOrMethod.h"

#ifdef USE_PR_IO
#define PR_fprintf fprintf
#define PR_Write fwrite
#define PRFileDesc FILE
#endif

HeaderGenerator::HeaderGenerator(ClassCentral &central) : central(central)
{ 
  /* Default directory to write stuff into is the current directory */
  headerDir = PL_strdup(".");
  tempDir = PL_strdup("/tmp");
} 

HeaderGenerator::~HeaderGenerator()
{
  //free(headerDir);
  //free(tempDir);  
}


void HeaderGenerator::setClassPath(const char *classPath)
{
  central.setClassPath(classPath);
}


void HeaderGenerator::setOutputDir(const char *dir)
{
  //free(headerDir);

  headerDir = (dir) ? PL_strdup(dir) : PL_strdup(".");
}

void HeaderGenerator::setTempDir(const char *dir)
{
  //free(tempDir);

  tempDir = (dir) ? PL_strdup(dir) : PL_strdup("/tmp");
}

  
bool HeaderGenerator::writeFile(const char *className)
{
  ClassFileSummary *summ;
  
  try {
    summ = &central.addClass(className);
  } catch (VerifyError err) {
#ifdef DEBUG
    printf("VerifyError loading class %s: %d\n", className, err.cause);
#endif
    return false;
  }
  
  TemporaryStringCopy copy(className);
  char *cname = copy;

  /* Replace slashes in the className with underscores */
  mangleClassName(cname, '/');

  /* The 2 at the end is for the NULL character and an extra slash at the 
   * end of the directory name
   */
  uint32 fileLen = strlen(headerDir)+strlen(className)+sizeof(".h")+5;

  TemporaryBuffer buffer(fileLen);
  char *fileName = buffer;

  PR_snprintf(fileName, fileLen, "%s/%s.h", headerDir, cname);
  
#ifndef USE_PR_IO
  FileDesc desc(fileName, (PR_WRONLY | PR_CREATE_FILE), 00644);
  PRFileDesc *fp = desc;
#else
  FILE *fp = fopen(fileName, "w");
#endif

  if (!fp)
    return false;
  
  writeHeader(cname, fp);
  
  bool ret = genHeaderFile(*summ, fp);
    
  if (!ret)
    return false;

  writeFooter(cname, fp);

#ifndef USE_PR_IO
  desc.close();
#else
  fclose (fp);
#endif

  /* Write the headers for our parent objects if they exist */
  if (needParents()) {
    const char *parentName = getParentName(*summ->getReader());
    
    if (parentName && !writeFile(parentName))
      return false;
  }
    
  return ret;
}

void HeaderGenerator::writeHeader(const char *cname, PRFileDesc *fp) 
{  
  PR_fprintf(fp, "#ifndef _%s_H_\n#define _%s_H_\n\n", cname, cname);
}


void HeaderGenerator::writeFooter(const char *cname, PRFileDesc *fp) 
{
  PR_fprintf(fp, "#endif /* _%s_H_ */\n", cname);
}

char *HeaderGenerator::getMangledName(const Type &type, bool asArray)
{
  switch (type.typeKind) {
  case tkChar:    
    return (asArray) ? PL_strdup("char") : PL_strdup("int32 /* char */");
    break;
      
  case tkShort:
    return (asArray) ? PL_strdup("int16") : PL_strdup("int32 /* short */");
    break;
    
  case tkInt:
    return PL_strdup("int32");
    break;
    
  case tkByte:
    return (asArray) ? PL_strdup("uint8") : PL_strdup("uint32 /* byte */");
    break;
    
  case tkBoolean:
    return (asArray) ? PL_strdup("char") : PL_strdup("uint32 /* bool */");
    break;

  case tkLong:
    return PL_strdup("int64");
    break;
      
  case tkFloat:
    return PL_strdup("Flt32");
    break;
    
  case tkDouble:
    return PL_strdup("Flt64");
    break;

  case tkVoid:
    return PL_strdup("void");
    break;
    
  case tkObject:
  case tkInterface: {
    Class &clazz = *const_cast<Class *>(static_cast<const Class *>(&type));
    TemporaryStringCopy copy(clazz.getName());
    char *nameCopy = copy;
    mangleClassName(nameCopy, '.');
    int32 objNameLen = strlen(nameCopy)+ 1 + sizeof("Java_");
    char *objName = (char *) malloc(objNameLen);
    PR_snprintf(objName, objNameLen, "Java_%s", nameCopy);
    return objName;
  } 

  case tkArray: {
    const Array &atype = *static_cast<const Array *>(&type);
    char *elementString = getMangledName(atype.componentType, true);
    assert(elementString);
    int32 len = sizeof("ArrayOf_")+strlen(elementString)+1;
    char *arrayStr = (char *) malloc(len);
    PR_snprintf(arrayStr, len, "ArrayOf_%s", elementString);

	// NSPR needs to provide a way to free strings that it allocates
#ifdef DEBUG
    free(elementString);
#endif

    return arrayStr;
  } 

  default:
    return 0;
  }
}


char *HeaderGenerator::getArgString(const Type &type)
{
  switch (type.typeKind) {
  case tkObject:  /* For objects, we always pass a pointer to the object */
  case tkInterface:
  case tkArray: {
    char *objArg = getMangledName(type);
    assert(objArg);
    objArg = (char *) realloc(objArg, strlen(objArg)+3);
    PL_strcat(objArg, " *");
    return objArg;
  }
  
  
  default:
    return getMangledName(type);
  }
    
}


void HeaderGenerator::addMangledName(ClassFileSummary &summ, 
				     const Type &type, StringPool &sp)
{
  Type *ourType = summ.getThisClass();

  /* No need to generate forward declarators for ourselves */
  if (ourType == &type)
    return;
  
  /* XXX Note here that we still generate forward declarations for
   * references to our parent types. These are infrequent, but still
   * redundant. But they'll compile, so I'll come back some other day
   * and filter them out -- kini
   */

  char *mangledObjName = 0;
  mangledObjName = getMangledName(type);
    
  if (!mangledObjName)
    verifyError(VerifyError::noClassDefFound);

  sp.intern(mangledObjName);
  free((void *) mangledObjName);
}

				       
void HeaderGenerator::genForwards(ClassFileSummary &summ, 
				  StringPool &sp)
{
  /* So as not to generate duplicate forward declarations, we'll go
   * through the fields and methods and put all object and array names into
   * a StringPool, which we will use later to generate the actual
   * forward declarations.
   */

  /* Go through the fields */
  const Field **fields = summ.getFields();
  uint32 fieldCount = summ.getFieldCount();
  
  uint32 i;
  for (i = 0; i < fieldCount; i++) {    
    const Type &type = fields[i]->getType();

    if (type.typeKind == tkObject || type.typeKind == tkInterface || 
	type.typeKind == tkArray) 
      addMangledName(summ, type, sp);
  }
  
  const Method **methods = summ.getMethods();
  uint32 methodCount = summ.getMethodCount();
  
  for (i = 0; i < methodCount; i++) {
    Method *m = (Method *) methods[i];
    const Signature &sig = m->getSignature();

    if ((m->getModifiers() & CR_METHOD_NATIVE)) {
      /* Generate forward declarators for the return type, if neccessary */
      if (sig.resultType->typeKind == tkObject ||
	  sig.resultType->typeKind == tkInterface ||
	  sig.resultType->typeKind == tkArray)
	addMangledName(summ, *sig.resultType, sp);
      
      for (uint32 j = 0; j < sig.nArguments; j++) 
	if (sig.argumentTypes[j]->typeKind == tkObject ||
	    sig.argumentTypes[j]->typeKind == tkInterface ||
	    sig.argumentTypes[j]->typeKind == tkArray) 
	  addMangledName(summ, *sig.argumentTypes[j], sp);  
    }
  }

}




