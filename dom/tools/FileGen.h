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

#ifndef _FileGen_h__
#define _FileGen_h__

#include <string.h>
#include "plhash.h"

#ifdef XP_MAC
#include <fstream.h>			// required for namespace resolution
#else
class ofstream;
#endif

#define USE_COMPTR 1

class IdlObject;
class IdlSpecification;
class IdlInterface;
class IdlVariable;
class IdlParameter;
class IdlFunction;

class FileGen {
public:
    FileGen();
    virtual ~FileGen();
    virtual void    Generate(char *aFileName, char *aOutputDirName, 
                             IdlSpecification &aSpec, int aIsGlobal)=0;

protected:
    void            GenerateNPL();
    int             OpenFile(char *aFileName, char *aOutputDirName,
                             const char *aFilePrefix, const char *aFileSuffix);
    void            CloseFile();
    void            GetVariableTypeForParameter(char *aBuffer, IdlVariable &aVariable);
    void            GetVariableTypeForLocal(char *aBuffer, IdlVariable &aVariable);    
    void            GetVariableTypeForMethodLocal(char *aBuffer, IdlVariable &aVariable);
    void            GetVariableTypeForReturnValue(char *aBuffer, IdlVariable &aVariable);

    void            GetParameterType(char *aBuffer, IdlParameter &aParameter);
    void            GetInterfaceIID(char *aBuffer, IdlInterface &aInterface);
    void            GetInterfaceIID(char *aBuffer, char *aInterfaceName);
    void            GetXPIDLInterfaceIID(char *aBuffer, char *aInterfaceName);
    void            GetXPIDLInterfaceIID(char *aBuffer, IdlInterface &aInterface);
    void            GetCapitalizedName(char *aBuffer, IdlObject &aObject);
    void            EnumerateAllObjects(IdlSpecification &aSpec, 
                                        PLHashEnumerator aEnumerator,
                                        void *aArg,
                                        PRBool aOnlyPrimary);
    void            EnumerateAllObjects(IdlInterface &aInterface,
                                        PLHashEnumerator aEnumerator,
                                        void *aArg);
    PRBool          HasConstructor(IdlInterface &aInterface, 
                                   IdlFunction **aConstructor);

    ofstream*       GetFile() { return mOutputFile; }
    
    void            StrUpr(char *aBuffer);
    void            StrLwr(char *aBuffer);

private:
    void            CollectAllInInterface(IdlInterface &aInterface,
                                          PLHashTable *aTable);

    ofstream        *mOutputFile;
};

#endif // _FileGen_h__

