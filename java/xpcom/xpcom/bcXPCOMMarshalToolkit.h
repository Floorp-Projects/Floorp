/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Kushnirskiy <idk@eng.sun.com>
 */

#ifndef __bcXPCOMMarshalToolkit_h
#define __bcXPCOMMarshalToolkit_h

#include "nsISupports.h"
#include "xptcall.h"
#include "bcIMarshaler.h"
#include "bcIUnMarshaler.h"
#include "bcIORB.h"

class bcXPCOMMarshalToolkit {
public:
    bcXPCOMMarshalToolkit(PRUint16 methodIndex,
                        nsIInterfaceInfo *interfaceInfo, nsXPTCMiniVariant* params);
    bcXPCOMMarshalToolkit(PRUint16 methodIndex,
                        nsIInterfaceInfo *interfaceInfo, nsXPTCVariant* params);
    virtual ~bcXPCOMMarshalToolkit();
    nsresult Marshal(bcIMarshaler *);
    nsresult UnMarshal(bcIUnMarshaler *);
private:
    enum { unDefined, onServer, onClient } callSide; 
    enum SizeMode { GET_SIZE, GET_LENGTH };  
    PRUint16 methodIndex;
    nsXPTMethodInfo *info;
    nsXPTCVariant *params;
    nsIInterfaceInfo * interfaceInfo;
    nsresult GetArraySizeFromParam( nsIInterfaceInfo *interfaceInfo, 
                           const nsXPTMethodInfo* method,
                           const nsXPTParamInfo& param,
                           uint16 methodIndex,
                           uint8 paramIndex,
                           nsXPTCVariant* nativeParams,
                           SizeMode mode,
                           PRUint32* result);
    bcXPType XPTType2bcXPType(uint8 type); //conversion from xpcom to our own types system
    nsresult MarshalElement(bcIMarshaler *m, void *data, nsXPTParamInfo * param, uint8 type, uint8 ind);
    nsresult UnMarshalElement(void *data, bcIUnMarshaler *um, nsXPTParamInfo * param, uint8 type, 
                              bcIAllocator * allocator);
    PRInt16 GetSimpleSize(uint8 type);
};

#endif /*  __XPCOMMarshalToolkit_h */

