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
#include "NetscapeHeaderGenerator.h"
#include "prprf.h"
#include "plstr.h"
#include "CUtils.h"
#include "StringUtils.h"
#include "JavaObject.h"
#include "FieldOrMethod.h"
#include "NetscapeManglers.h"

#ifdef USE_PR_IO
#define PR_fprintf fprintf
#define PR_Write fwrite
#define PRFileDesc FILE
#endif

bool NetscapeHeaderGenerator::genHeaderFile(ClassFileSummary &summ,
					    PRFileDesc *fp)
{
  const ClassFileReader &reader = *summ.getReader();
  const char *className = reader.getThisClass()->getUtf()->getUtfString();

  PR_fprintf(fp, "/* This file is machine-generated. Do not edit!! */\n");

  PR_fprintf(fp, "\n/* Header for class %s */\n\n", className);

  PR_fprintf(fp, "#include \"prtypes.h\"\n");
  PR_fprintf(fp, "#include \"NativeDefs.h\"\n");

  /* If we have a parent, put in a #include so that we can access the
   * parent's struct as well
   */
  const char *constParentName = getParentName(reader);
  char *parentCName = 0;

  if (constParentName) {
    TemporaryStringCopy pcopy(constParentName);
    char *parentName = pcopy;
    int32 parentLen = PL_strlen(parentName)+sizeof("Java_")+1;
    
    parentCName = (char *) malloc(parentLen+1);
    
    PL_strcpy(parentCName, mangleClassName(parentName, '/'));
    
    PR_fprintf(fp, "#include \"%s.h\"\n", parentCName);
  } 
  
  PR_fprintf(fp, "\n\n");
  
  /* Generate forward declarations */
  Pool p;
  StringPool sp(p);
  genForwards(summ, sp);

  /* Now get all the keys in the string pool */
  Vector<char *> &vec = sp;
  
  for (uint32 index = 0; index < vec.size(); index++) 
    PR_fprintf(fp, "struct %s;\n", vec[index]);

  PR_fprintf(fp, "\n\n");

  TemporaryStringCopy copy(className);
  char *mangledClassName = mangleClassName((char *) copy, '/');
  
  if (parentCName) {
    PR_fprintf(fp, "struct Java_%s : Java_%s {\n", mangledClassName, 
	       parentCName);
    PR_fprintf(fp, "\tJava_%s(const Type &type) : Java_%s(type) { }\n",
	       mangledClassName, parentCName);
    free(parentCName);
  } else {
    PR_fprintf(fp, "struct Java_%s : JavaObject {\n", mangledClassName);
    PR_fprintf(fp, "\tJava_%s(const Type &type) : JavaObject(type) { }\n",
	       mangledClassName);
  }
 
  
  /* Go through each field and generate it's type */
  const Field **fields = summ.getFields();
  uint32 fieldCount = summ.getFieldCount();
  
  uint32 i;
  for (i = 0; i < fieldCount; i++) {    
    const char *fieldName = fields[i]->getName();
    const Type &type = fields[i]->getType();
    
    
    const char *typeStr = getArgString(type);
    
    if (!typeStr)
      return false;
    
    if (fields[i]->getModifiers() & CR_FIELD_STATIC)
      PR_fprintf(fp, "\t// static %s %s; \n", typeStr, fieldName);
    else
      PR_fprintf(fp, "\t%s %s;\n", typeStr, fieldName);
    
	// Currently kills the release build since NSPR needs to give us a 
	// clean way of freeing strings that it allocates
#ifdef DEBUG 
    //free((void *) typeStr);  /* getArgString() uses malloc to allocate */
#endif

  }
  
  PR_fprintf(fp, "};\n\n");
  
  /* Generate a declaration for the array type corresponding to this class */
  PR_fprintf(fp, "ARRAY_OF(Java_%s);\n", mangledClassName);

  const Method **methods = summ.getMethods();
  uint32 methodCount = summ.getMethodCount();

  /* Now go through each method and see if it is overloaded.  */
  TemporaryBuffer overbuf(methodCount*sizeof(bool));
  bool *overloadedMethods = (bool *) (char *) overbuf;

  for (i = 0; i < methodCount; i++) {
    const char *methodName = methods[i]->getName();

    overloadedMethods[i] = false;
    for (int j = 0; j < methodCount; ++j)
      if (i != j && strcmp(methodName, methods[j]->getName()) == 0)
        overloadedMethods[i] = true;
  }
  
  /* Now go through each native method and generate a declaration for it */

  NetscapeShortMangler smangler;
  NetscapeLongMangler lmangler;
  
  /* extern "C" the generated method headers */
  PR_fprintf(fp, "extern \"C\" {\n");

  for (i = 0; i < methodCount; i++) {
    Method *m = (Method *) methods[i];
    const Signature &sig = m->getSignature();
    
    if ((m->getModifiers() & CR_METHOD_NATIVE)) {
      /* Generate the return type */
      char *retString = getArgString(*sig.resultType);
      const char *methodName = m->getName();
      const char *signature = const_cast<Method *>(m)->getSignatureString();

      if (!retString)
	return false;
      
      int32 len = PL_strlen(methodName) + PL_strlen(className)*2 +
	PL_strlen(signature)*2 + sizeof("Netscape_Java___");
  
      TemporaryBuffer copy(len);
      char *mangledMethodName = copy;
      
      if (overloadedMethods[i]) {
        if (!lmangler.mangle(className, methodName, signature, 
			     mangledMethodName, len)) 
	  continue;
      }
      else {
        if (!smangler.mangle(className, methodName, signature, 
			     mangledMethodName, len)) 
	  continue;
      }

      PR_fprintf(fp, "\n/*\n");
      PR_fprintf(fp, " * Class : %s\n", className);
      PR_fprintf(fp, " * Method : %s\n", methodName);
      PR_fprintf(fp, " * Signature : %s\n", signature);
      PR_fprintf(fp, " */\n");

      PR_fprintf(fp, "NS_EXPORT NS_NATIVECALL(%s)\n%s(", 
		 retString, mangledMethodName);

	  // Kills the release build since NSPR needs to give us a clean way to 
	  // free strings that it allocates
#ifdef DEBUG
      //free(retString);
#endif

      for (uint32 j = 0; j < sig.nArguments; j++) {
	char *typeString = getArgString(*sig.argumentTypes[j]);
	
	if (!typeString)
	  return false;
	
	if (j == 0)
		PR_fprintf(fp, "%s", typeString);
	else
		PR_fprintf(fp, ", %s", typeString);
	
	// Currently does not work with the release build since NSPR needs to
	// give us a clean way of freeing strings that it allocates
#ifdef DEBUG
	//free(typeString);
#endif

      }       	
      
      PR_fprintf(fp, ");\n\n");
    }
  }
  
  /* Matches the extern "C" declaration generated earlier */
  PR_fprintf(fp, "}\n");
  return true;
}






