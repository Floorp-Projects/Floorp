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
#ifndef _CLASS_CENTRAL_H_
#define _CLASS_CENTRAL_H_

#include "DoublyLinkedList.h"
#include "HashTable.h"
#include "Pool.h"
#include "Vector.h"
#include "ClassFileSummary.h"
#include "ClassWorld.h"
#include "PathComponent.h"

/* A node on pathList; represents a canonical path in which
 * to find classes
 */
class PathNode : public DoublyLinkedEntry<PathNode> {
public:
  PathComponent *component;
};

/* ClassCentral is a repository of all classes currently loaded
 */
class NS_EXTERN ClassCentral {
public:
  /* classPath is a colon-separated list of directories in which to search
   * for class files. If this is NULL, the system-dependent environment
   * setting (the env variable CLASSPATH on Unix and Windows, on MAC?)
   * is used to determine the directories to search for class files. 
   * This routine makes its own copy of classPath, so it does not need to 
   * persist after this routine exits.
   * world is a runtime repository used to hold Class, Interface and
   * Type objects for all runtime objects that ClassCentral creates.
   * sp is a repository that will hold all strings interned
   * during class-file resolution.
   * p is a pool that will be used to allocate runtime objects.
   */
  ClassCentral(Pool &p, ClassWorld &world, StringPool &sp,
	       const char *classPath=0);

  /* Set the path to search for class files. The path must be canonical
   * (Unix-like). An argument of zero resets the value of the classPath
   * to whatever is derived from the environment; in effect, it "resets"
   * the classPath to the default.
   * This routine makes its own copy of classPath, so it does not need to 
   * persist after this routine exits.
   */
  void setClassPath(const char *classPath=0);

  /* Get the current class path */
  const char *getClassPath() const {
    return classPath;
  }

  /* Add the set of directories in path in front of, or behind, the current
   * classPath
   */
  void prefixClassPath(const char *path);
  void suffixClassPath(const char *path);

  /* Load a class and return a pointer to the class information. If a class
   * is already loaded, it just returns a pointer to the loaded class.
   * Will also recursively load all of the class's ancestor classes (but not
   * interfaces) that haven't already been loaded.
   * className is the fully qualified name of the class. 
   * (eg., java/lang/Object)
   * fileName, if non-NULL, points to an absolute or relative pathname
   * which represents the file that contains this class. If fileName
   * is set to NULL, then the user's CLASSPATH is searched to determine
   * the location of the file. fileName can be either a native or
   * canonical (Unix-like) file-path.
   */
  ClassFileSummary &addClass(const char *className, const char *fileName=0);
  
  /* Load a class using the given FileReader class to read the raw bytes 
   * in the class. Return a pointer to the loaded class. className is the
   * fully qualified name of the class.
   * If the class is already loaded, it returns a pointer to the loaded
   * class (not sure if it should do this).
   */
  ClassFileSummary &addClass(const char *className, FileReader &reader);

  /* Remove a class from the pool. Need to properly define the semantics
   * of this, but the idea is that this will attempt to also clean up
   * as many dependent classes as possible.
   */
  void removeClass(const char *className);
  void removeClass(ClassFileSummary *info);

  /* Parse the Java field descriptor string starting at s. Construct
   * and return a Type class corresponding to the parsed descriptor,
   * loading classes if neccessary. Sets *next to point to
   * remaining string on success.
   */
  Type &parseFieldDescriptor(const char *s, const char *&next);

  /* Return a reference to the string pool used to intern strings */
  StringPool &getStringPool() { return sp; }

  /* return a reference to the ClassWorld object used internally */
  ClassWorld &getClassWorld() { return world; }

private:
  char *classPath;
  DoublyLinkedList<PathNode> pathList;

  HashTable<ClassFileSummary *> classPool;
  Pool staticPool;   /* Pool to allocate static information */
  ClassWorld &world;
  StringPool &sp;
  Pool &dynamicPool; /* Pool to allocate dynamic types that will be used
		      *  by the runtime
		      */

  char *getEnvPath();
  char *concatPath(const char *s, const char *t);
  void addPath(char *cpath, bool end);

  bool validateName(const char *className) const;
  FileReader *resolveClassName(const char *className);
  FileReader *lookForClassFile(const char* className, 
			       size_t maxFileNameLen = 0);

  ClassFileSummary &loadClass(Vector<ClassFileSummary *> &vec,
			      const char *className, const char *fileName,
                  bool &alreadyLoaded,
			      bool &loadParents);

  ClassFileSummary &newSummary(const char *className, 
			       FileReader *fr,
			       bool arrayType, 
			       Uint32 numDimensions,
			       bool primitiveComponentType,
			       TypeKind tk,
			       ClassFileSummary *component);
  
  ClassFileSummary &addClass(Vector<ClassFileSummary *> &vec,
			     const char *className, const char *fileName=0);
};

#endif /* _CLASS_CENTRAL_H_ */
