/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#ifndef _JSStubGen_h__
#define _JSStubGen_h__

#include <string.h>
#include "plhash.h"
#include "FileGen.h"

#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ofstream;
#endif

class IdlSpecification;
class IdlInterface;
class IdlAttribute;

class JSStubGen : public FileGen {
public:
     JSStubGen();
     virtual ~JSStubGen();

     virtual void     Generate(char *aFileName, char *aOutputDirName, 
                               IdlSpecification &aSpec, int aIsGlobal);
  
     friend PRIntn JSStubGen_IIDEnumerator(PLHashEntry *he, 
                                           PRIntn i, 
                                           void *arg);
     friend PRIntn JSStubGen_DefPtrEnumerator(PLHashEntry *he, 
                                              PRIntn i, 
                                              void *arg);
     
protected:
  enum {
    JSSTUBGEN_PRIMARY,
    JSSTUBGEN_NONPRIMARY,
    JSSTUBGEN_DEFAULT,
    JSSTUBGEN_DEFAULT_NONPRIMARY,
    JSSTUBGEN_DEFAULT_ELLIPSIS,
    JSSTUBGEN_DEFAULT_NONPRIMARY_ELLIPSIS,
    JSSTUBGEN_NAMED_ITEM,
    JSSTUBGEN_NAMED_ITEM_NONPRIMARY,
    JSSTUBGEN_NAMED_ITEM_ELLIPSIS,
    JSSTUBGEN_NAMED_ITEM_NONPRIMARY_ELLIPSIS
  };

     void     GenerateIncludes(IdlSpecification &aSpec);
     void     GenerateIIDDefinitions(IdlSpecification &aSpec);
     void     GenerateDefPtrs(IdlSpecification &aSpec);
     void     GeneratePropertySlots(IdlSpecification &aSpec);
     void     GeneratePropertyFunc(IdlSpecification &aSpec, PRBool aIsGetter);
     void     GenerateCustomPropertyFuncs(IdlSpecification &aSpec);
     void     GenerateFinalize(IdlSpecification &aSpec);
     void     GenerateEnumerate(IdlSpecification &aSpec);
     void     GenerateResolve(IdlSpecification &aSpec);
     void     GenerateMethods(IdlSpecification &aSpec);
     void     GenerateJSClass(IdlSpecification &aSpec);
     void     GenerateClassProperties(IdlSpecification &aSpec);
     void     GenerateClassFunctions(IdlSpecification &aSpec);
     void     GenerateConstructor(IdlSpecification &aSpec);
     void     GenerateInitClass(IdlSpecification &aSpec);
     void     GenerateNew(IdlSpecification &aSpec);

     void     GeneratePropGetter(ofstream *file,
                                 IdlInterface &aInterface,
                                 IdlVariable &aAttribute,
                                 PRInt32 aType);
     void     GeneratePropSetter(ofstream *file,
                                 IdlInterface &aInterface,
                                 IdlAttribute &aAttribute,
                                 PRBool aIsPrimary);

     int mIsGlobal;
};

#endif // _JSStubGen_h__
