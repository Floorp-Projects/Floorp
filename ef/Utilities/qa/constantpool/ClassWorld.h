/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#ifndef CLASSWORLD_H
#define CLASSWORLD_H

#include "JavaObject.h"
#include "Pool.h"
#include "HashTable.h"

struct Package
{
	const char *const name;						// Name of the package (stored in the ClassWorld's pool)

	explicit Package(const char *name): name(name) {}

  #ifdef DEBUG
	int printRef(FILE *f) const;
  #endif
};


// Pool of run-time information for a Java Program. New classes can be added
// to the pool explicitly (at runtime), or by ClassCentral. 
class ClassWorld {
public:
  ClassWorld(Pool &p) : table(p), pool(p) { }
  
  Type &getType(const char *packageName, const char *className);
 
  bool getType(const char *fullyQualifiedClassName, Type *&t) {
	return table.get(fullyQualifiedClassName, &t);
  }

  void add(const char *className, Type *type) {
	table.add(className, type);
  }

private:
  HashTable<Type *> table;
  Pool &pool;
};

// ----------------------------------------------------------------------------
// Distinguished packages

extern Package pkgInternal;						// Internal package for housekeeping classes
extern Package pkgJavaLang;

#endif
