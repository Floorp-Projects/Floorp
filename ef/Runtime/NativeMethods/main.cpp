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
#include "NativeMethodDispatcher.h"

Pool p;
StringPool sp(p);
ClassWorld world(p);

/* Simple test for the native method dispatch mechanism
 * argv[1] -- fully qualified classname 
 * argv[2] -- shared lib to load (without system-specific extension)
 * argv[3] -- methodName (optional)
 */
int main(int argc, char **argv)
{
  const char *className = argv[1];
  const char *libName = argv[2];
  const char *methodName = (argc > 3) ? argv[3] : 0;
  Method *theMethod = 0;
    
  ClassCentral central(p, world, sp, 0);

  initDistinguishedObjects(central);
  NativeMethodDispatcher::staticInit();

  ClassFileSummary &summ = central.addClass(className);

  /* Load the shared library */
  if (NativeMethodDispatcher::loadLibrary(libName) == 0) {
    fprintf(stderr, "Cannot load library %s\n", libName);
    return 1;
  }

  /* Now display all methods and ask the user to pick a method to resolve */
  if (!methodName) {
    fprintf(stderr, "Choose from the following methods:\n");
    
    Method **methods = (Method **) summ.getMethods();
    int32 methodCount = summ.getMethodCount();
    for (int32 i = 0; i < methodCount; i++) {
      const char *name = methods[i]->toString();

      if (methods[i]->getModifiers() & CR_METHOD_NATIVE)
	fprintf(stderr, "%d. %s\n", i+1, name);
    }

    fprintf(stderr, "\nChoose number of choice-> ");
    char s[80];
    gets(s);
    int choice = atoi(s)-1;
    if (choice > 0 && choice <= methodCount) {
      methodName = methods[choice]->toString();
      theMethod = methods[choice];
    } else {
      fprintf(stderr, "Bad Boy. You have entered a bad index. Try again.\n");
      return 1;
    }
  } else {
    if (!(methodName = sp.get(methodName)) ||
	!(world.getMethod(methodName, theMethod))) {
      fprintf(stderr, "That seems to be an invalid methodname. Try again.\n");
      return 1;
    }
  }

  assert(theMethod);
  if (NativeMethodDispatcher::resolve(*theMethod))
    fprintf(stderr, "Resolved method.\n");
  else
    fprintf(stderr, "Could not resolve method!\n");

  return 0;
}

#ifdef __GNUC__
/* for gcc with -fhandle-exceptions */
void terminate() {}
#endif
