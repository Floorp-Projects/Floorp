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
 * Sergey Lunegov <lsv@sparc.spb.su>
 */

#ifndef URP_MARSHALTOOLKIT
#define URP_MARSHALTOOLKIT

#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "bcDefs.h"
#include "urpTransport.h"
#include "bcIORB.h"
#include "urpManager.h"

#include "nsHashtable.h"

#define BIG_HEADER 0x80
#define REQUEST 0x40
#define NEWTYPE 0x20
#define NEWOID 0x10
#define NEWTID 0x08
#define LONGMETHODID 0x04
#define IGNORECACHE 0x02
#define MOREFLAGS 0x01

#define MUSTREPLY 0x80
#define SYNCHRONOUSE 0x40

#define DIR_MID 0x40
#define EXCEPTION 0x20

#define INTERFACE 22
#define INTERFACE_STRING "com.sun.star.uno.XInterface"


class urpMarshalToolkit {

public:
	urpMarshalToolkit(PRBool isClient);
	~urpMarshalToolkit();

    nsresult ReadParams(PRUint32 paramCount, const nsXPTMethodInfo *info, urpPacket* message, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex, bcICall* call, bcIORB *orb, urpManager* man, urpConnection* conn);
    nsresult WriteParams(bcICall *call, PRUint32 paramCount, const nsXPTMethodInfo * info, nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex);
    void WriteType(bcIID iid, urpPacket* message);
    bcIID ReadType(urpPacket* message);
    void WriteOid(bcOID oid, urpPacket* message);
    bcOID ReadOid(urpPacket* message);
    void WriteThreadID(long tid, urpPacket* message);
    bcTID ReadThreadID(urpPacket* message);

private:
    PRBool isClient;
    nsresult WriteElement(bcIUnMarshaler * um, nsXPTParamInfo * param, uint8 type, uint8 ind,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, const nsXPTMethodInfo* info,
			bcIAllocator * allocator);
    PRInt16 GetSimpleSize(uint8 type);
    nsresult ReadElement(nsXPTParamInfo * param, uint8 type,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, bcIAllocator * allocator,
			bcIMarshaler* m, bcIORB *orb, urpManager* man, 
			urpConnection* conn);
    bcXPType XPTType2bcXPType(uint8 type);
};

#endif
