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
#include "prenv.h"
#include "prio.h"
#include "ClassCentral.h"
#include "ErrorHandling.h"
#include "DiskFileReader.h"

#include "CUtils.h"
#include "StringUtils.h"

#include "plstr.h"
#include "prprf.h"


UT_DEFINE_LOG_MODULE(ClassCentral);


ClassCentral::ClassCentral(Pool &p, ClassWorld &world, StringPool &sp,
                           const char *cp) : 
  classPool(staticPool), world(world), sp(sp), dynamicPool(p)
{  
  classPath = 0;
  setClassPath(cp);
}


/* Returns a new path string formed by concatenating the
 * path string t to the path string s. A path string is a
 * colon-separated list of (canonical) directory names.
 */
char *ClassCentral::concatPath(const char *s, const char *t)
{
  char *newStr = new (staticPool) char[PL_strlen(s)+PL_strlen(t)+1];

  PL_strcpy(newStr, s);

  if (newStr[PL_strlen(s)-1] != ':')
    PL_strcat(newStr, ":");

  PL_strcat(newStr, t);

  return newStr;
}

char *ClassCentral::getEnvPath() 
{
  char *env;
  if ((env = PR_GetEnv("CLASSPATH")) != 0)
    return dupString(env, staticPool);
  else
    return 0;
}

#ifdef NO_NSPR
  #ifdef EF_WINDOWS
   #define PATH_SEPARATOR '\\'
  #endif
#else
#define PATH_SEPARATOR '/'
#endif


static bool fileExists(const char *fn)
{
#ifdef NO_NSPR
  fn = 0;    // Dummy
  return true;
#else
  PRFileInfo info;
  return (PR_GetFileInfo(fn, &info) >= 0);
#endif
}


/* Look for a file name that matches maxFileNameLen characters
 * of className. This is to look for truncated files on the
 * MAC.
 */
FileReader *ClassCentral::lookForClassFile(const char* className, 
                                           size_t maxFileNameLen)
{
  for (DoublyLinkedList<PathNode>::iterator i = pathList.begin();
       !pathList.done(i); i = pathList.advance(i)) {
    PathComponent *component = pathList.get(i).component;
    FileReader *fileReader = component->getFile(className, maxFileNameLen);
    
    if (fileReader)
      return fileReader;
  }
  
  return 0;
}

/* Returns the canonical filename of the class from the fully
 * qualified className. The current class path is searched to
 * find the file and the first available match is returned.
 * Returns NULL if no matching file was found.
 */
FileReader *ClassCentral::resolveClassName(const char *className)
{
  FileReader *reader;
  
#ifdef XP_MAC
  /* first look for the class file with a maximum length of 31 
   * for the name
   */
  if ((reader = lookForClassFile(className, 30)) != 0)
    return reader;
#endif
 
  /* now look for the class file with its whole name */
  if ((reader = lookForClassFile(className)) != 0)
    return reader;

  return 0;
}


/* Adds the paths represented in cpath either to the
 * end of, or at the beginning of, the current classPath
 * depending on whether end is true or false. cpath is
 * a colon-separated list of canonical pathnames. cpath
 * is altered as a result of this routine.
 */
void ClassCentral::addPath(char *cpath, bool end)
{
  Strtok tok(cpath);
  char *s = tok.get(":");

  if (end) {
    while (s) {
      PathNode *pathNode = new (staticPool) PathNode;
      pathNode->component = new (staticPool) PathComponent(s, staticPool);
      
      pathList.addLast(*pathNode);
      s = tok.get(":");
    }
  } else {
    if (s) {
      /* Add the first component to the beginning */
      PathNode *firstNode = new (staticPool) PathNode;
      firstNode->component = new (staticPool) PathComponent(s, staticPool);
    
      pathList.addFirst(*firstNode);    
      s = tok.get(":");
      
      /* Now add the rest of the components */
      while (s) {
        PathNode *pathNode = new (staticPool) PathNode;
        pathNode->component = new (staticPool) PathComponent(s, staticPool);
        
        pathList.insertAfter(*pathNode, pathList.location(*firstNode));
        s = tok.get(":");
        
      }
    }
  }
}

void ClassCentral::setClassPath(const char *cp)
{
  pathList.clear();
  
  classPath = (cp) ? dupString(cp, staticPool) : getEnvPath();

  if (classPath) {
    char *cpath = dupString(classPath, staticPool);

    addPath(cpath, true);
  }

  /* Always add the current directory to the CLASSPATH; this might result
   * in the current directory being searched twice if it's already in the
   * CLASSPATH, but that's OK.
   */
  addPath(dupString(".", staticPool), true);
}

void ClassCentral::prefixClassPath(const char *path)
{
  classPath = concatPath(path, classPath);
  char *cpath = dupString(path, staticPool);

  addPath(cpath, false);
}

void ClassCentral::suffixClassPath(const char *path)
{
   classPath = concatPath(classPath, path);

   char *cpath = dupString(path, staticPool);

   addPath(cpath, true);
}

/* Create a new ClassFileSummary object with the given parameters without
 * adding it to our repository (classPool). Verify that the name of the
 * class matches className.  See the documentation for
 * ClassFileSummary::ClassFileSummary for the interpretation of the
 * className, fileName, arrayType, numDimensions, primitiveElementType,
 * tk, and component parameters.
 */
ClassFileSummary &ClassCentral::newSummary(const char *className, 
                                           FileReader *fileReader,
                                           bool arrayType,
                                           Uint32 numDimensions,
                                           bool primitiveElementType,
                                           TypeKind tk,
                                           ClassFileSummary *component)
{
  ClassFileSummary *info;
  
  info = new (staticPool) ClassFileSummary(staticPool, 
                                           dynamicPool,
                                           sp,
                                           *this,
                                           world,
                                           className,
                                           fileReader,
                                           arrayType,
                                           numDimensions,
                                           primitiveElementType,
                                           tk,
                                           component);

  /* XXX Will this result in calling of the correct destructor? */
  if (fileReader)
    fileReader->FileReader::~FileReader();

  if (*className != '[') {
    const char *classNameInFile = 
      info->getClassName();
    
    
    /* Check to see if the class defined in this class file is indeed 
     * the classname we're looking for
     */
      if (PL_strcmp(classNameInFile, className) != 0)
        verifyError(VerifyError::classNotFound);
  }

  return *info;
}

/* Loads (but does not fully resolve) the class whose fully qualified name is
 * className and stores it in ClassCentral's
 * classPool. If fileName is non-NULL, it is the canonical name of the
 * file in which to find the class; if it is NULL, classPath is used to
 * construct the name of the file. If the class has already been loaded
 * or if the class is actually an array or an interface,
 * loadParents is set to false; otherwise, loadParents is set to true, 
 * indicating that the parent classes of this class must be loaded
 * by the caller.
 * components is a vector of ClassFileSummary objects that have already been
 * loaded in recursive calls to Classcentral::addClass; this is passed on
 * to recursive calls, if any, to ClassCentral::addClass().
 */
ClassFileSummary &ClassCentral::loadClass(Vector<ClassFileSummary *> &components,
                                          const char *className, 
                                          const char *fileName,
                                          bool &alreadyLoaded,
                                          bool &loadParents)
{
  ClassFileSummary *hashNodeData;
  loadParents = false;
  
  /* Has Class already been loaded? */
  if (classPool.get(className, &hashNodeData)) {
    alreadyLoaded = true;
    return *hashNodeData;
  }
  alreadyLoaded = false;

  /* Array classes */
  if (*className == '[') {
    const char *cname = className;

    Uint32 numDimensions = 1;
    while (*++cname == '[')
      numDimensions++;
    
    if (!*cname)
      verifyError(VerifyError::noClassDefFound);
    
    TypeKind tk;
    switch (*cname) {
    case 'L':
      if (!*++cname)
        verifyError(VerifyError::noClassDefFound);
      else {    
        ClassFileSummary *elementSummary;

        TemporaryStringCopy copy(cname);
        char *cnameCopy = copy;

        /* Over-write the semi-colon, if we can find it */
        char *t;
        for (t = cnameCopy; *t && *t != ';' ; t++)
          ;
        
        if (*t != ';' || *(t+1))
          verifyError(VerifyError::badClassFormat);
        
        *t = 0;
        
        if (classPool.get(cnameCopy, &hashNodeData))      
          elementSummary = hashNodeData;
        else 
          elementSummary = &addClass(components, cnameCopy);     

        return newSummary(className,
                          (FileReader *) 0,
                          true, numDimensions,
                          false,
                          tkVoid,
                          elementSummary);
        
      }
      break;
    
    default:
      if (!getTypeKindFromDescriptor(*cname, tk) || *(cname+1))
        verifyError(VerifyError::badClassFormat);                       
                        
      break;
    }
    
    /* If we're here, we have an array of a primitive type */
    return newSummary(className,
                      (FileReader *) 0,
                      true, numDimensions,
                      true,
                      tk,
                      0);
  }


  UT_LOG(ClassCentral, PR_LOG_DEBUG, ("\tLoading class %s\n", className));


  /* If we're here, we're a non-array class or interface that must be loaded from
   * a class file.
   */

  FileReader *fileReader;
  loadParents = true;
  
  if (fileName && fileExists(fileName))
    fileReader = new DiskFileReader(staticPool, fileName);
  else {
    if (!(fileReader = resolveClassName(className)))
      verifyError(VerifyError::noClassDefFound);
  }
  
  return newSummary(className, 
                    fileReader, 
                    false, 0, false, tkVoid,
                    0);
}

/* Add a class, ensuring that none of the summaries in the Vector
 * component represent the class to be resolved.
 */
ClassFileSummary &ClassCentral::addClass(Vector<ClassFileSummary *> &components,
                                         const char *className,
                                         const char *fileName)
{  
    bool loadParents, alreadyLoaded;
    ClassFileSummary *pParentInfo;
    
    /* Make sure we're not already on the component vector. If we are,
     * we've detected a circularity
     */
    Uint32 size = components.size();
    for (Uint32 i = 0; i < size; i++) {
        ClassFileSummary *summ = components[i];
        
        /* Note that this has to be a strcmp since className may or may
         * not be an interned string
         */
        if (!strcmp(className, summ->getClassName()))
            verifyError(VerifyError::classCircularity);
    }
    
    ClassFileSummary &info = loadClass(components, 
        className, fileName, alreadyLoaded, loadParents);
    
    if (alreadyLoaded)
        return info;
    
    components.append(&info);
    
    /* All interfaces have a null superclass, despite the fact that the .class file
       lists Object as the superclass of all interfaces. */
    if ((className[0] != '[') && // Ensure non-array class
        (info.getAccessFlags() & CR_ACC_INTERFACE)) {
        pParentInfo = NULL;
    } else if (loadParents) {
        const char *superclassName = info.getSuperclassName();
        
        if (superclassName) { 
            ClassFileSummary &parentInfo = addClass(components, superclassName, 0);
            
            /* Our parent must be a class; it cannot be a final class */
            Uint16 accessFlags = parentInfo.getAccessFlags();
            if ((accessFlags & CR_ACC_INTERFACE) || 
                (parentInfo.getAccessFlags() & CR_ACC_FINAL)) {
                verifyError(VerifyError::badClassFormat);
            }
            
            /* Our parent shouldn't be an array class */
            Type *parentType = parentInfo.getThisClass();
            if (parentType->typeKind != tkObject)
                verifyError(VerifyError::badClassFormat);
            
            pParentInfo = &parentInfo;
        } else {
            
            /* This must be the Object class */   
            pParentInfo = NULL;   
        }
    } else {
        /* This must be an array class.  Arrays always have Object as the superclass */
        pParentInfo = Standard::get(cObject).summary;
    }
    
    classPool.add(className, &info);
    info.setParent(pParentInfo);
    
    return info;
}

/* This routine enforces a series of constraints on the className
 * and returns true if the className satisfies all the constraints.
 * Otherwise, returns false
 */
bool ClassCentral::validateName(const char *className) const
{

    if (!className)	
	return false;

                
  /* Make sure that the className is not relative. Currently,
   * this means that it does not have a relative path (.) in it.
   */
  for (const char *s = className; *s; s++)
    if (*s == '.')
      return false;

  return true;
}

ClassFileSummary &ClassCentral::addClass(const char *className,
                                         const char *fileName)
{
  if (!validateName(className))
    verifyError(VerifyError::illegalAccess);

  Vector<ClassFileSummary *> vec(5);
  
  return addClass(vec, className, fileName);
}

void ClassCentral::removeClass(const char * /* className */)
{
  
}

void ClassCentral::removeClass(ClassFileSummary * /* info */)
{
  
}

Type &ClassCentral::parseFieldDescriptor(const char *s, const char *&next)
{
  TypeKind tk;
  
  switch (*s) {
  case '[': {
    Type &type = parseFieldDescriptor(s+1, next);
    return *(const_cast<Type *>(static_cast<const Type *> (&type.getArrayType())));
  }

  case 'L': {
    char *t = PL_strchr(++s, ';');

    if (!t)
      verifyError(VerifyError::badClassFormat);
      
    Int32 len = t-s+1;
    char *instanceClass = new char [len];

    if (!instanceClass)
      verifyError(VerifyError::badClassFormat);

    PL_strncpy(instanceClass, s, len-1);
    instanceClass[len-1] = 0;
    next = t+1;

    ClassFileSummary &summ = addClass(instanceClass);
    delete [] instanceClass;

    return *summ.getThisClass();
  }

  default:
    if (!(getTypeKindFromDescriptor(*s, tk)))
      verifyError(VerifyError::badClassFormat);
    break;
  }
        
  /* If we're here, we're a primitive type */
  Type &type = *(const_cast<Type *>(static_cast<const Type *>(&PrimitiveType::obtain(tk))));
  next = s+1;
  return type;
}







