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

#ifndef _XPCOMGen_h__
#define _XPCOMGen_h__

#include <string.h>
#include "FileGen.h"

#ifdef XP_MAC
#include <ostream.h>			// required for namespace resolution
#else
class ofstream;
#endif

class IdlSpecification;
class IdlInterface;
class IdlFunction;

class XPCOMGen : public FileGen {
public:
     XPCOMGen();
     virtual ~XPCOMGen();

     virtual void     Generate(char *aFileName, char *aOutputDirName, 
                               IdlSpecification &aSpec, int aIsGlobal);

protected:
     void     GenerateIfdef(IdlInterface &aInterface);
     void     GenerateIncludes(IdlInterface &aInterface);
     void     GenerateForwardDecls(IdlInterface &aInterface);
     void     GenerateGuid(IdlInterface &aInterface);
     void     GenerateClassDecl(IdlInterface &aInterface);
     void     GenerateStatic(IdlInterface &aInterface);
     void     GenerateEnums(IdlInterface &aInterface);
     void     GenerateMethods(IdlInterface &aInterface);
     void     GenerateEndClassDecl();
     void     GenerateFactory(IdlInterface &aInterface);
     void     GenerateEpilog(IdlInterface &aInterface, PRBool aIsPrimary);
     void     GenerateDeclMacro(IdlInterface &aInterface);
     void     GenerateForwardMacro(IdlInterface &aInterface);

     int mIsGlobal;
};

#endif // _XPCOMGen_h__ 
