#ifndef URP_MANGER
#define URP_MANGER

#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "urpTransport.h"
#include "bcXPCOMMarshalToolkit.h"


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


class urpManager {

public:
	urpManager(urpTransport* trans);
	urpManager(urpTransport* trans, bcIORB *orb);
	~urpManager();
	nsresult HandleRequest(urpConnection* conn);
	void SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
                          bcICall *call, nsXPTCVariant* params,
                          PRUint32 paramCount, const nsXPTMethodInfo* info);
    nsresult ReadReply(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex);
private:
	bcIORB *broker;
	enum SizeMode { GET_SIZE, GET_LENGTH };
	urpConnection* connection;
 	urpTransport* transport;
    void TransformMethodIDAndIID();
    void WriteType(bcIID iid, urpPacket* message);
    bcIID ReadType(urpPacket* message);
    void WriteOid(bcOID oid, urpPacket* message);
    bcOID ReadOid(urpPacket* message);
    void WriteThreadID(long tid, urpPacket* message);
    bcTID ReadThreadID(urpPacket* message);
    nsresult WriteParams(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo * info, nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex);
    nsresult ReadParams(nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, urpPacket* message, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex);
    nsresult ParseReply(urpPacket* message, char header,
                        nsXPTCVariant* params, PRUint32 paramCount, const nsXPTMethodInfo *info, nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex);
    nsresult ReadLongRequest(char header, urpPacket* message);
    nsresult ReadShortRequest(char header, urpPacket* message);
    nsresult SendReply(bcTID tid, nsXPTCVariant *params, PRUint32 paramCount,
                   const nsXPTMethodInfo* info,
                   nsIInterfaceInfo *interfaceInfo, PRUint16 methodIndex);
    nsresult MarshalElement(void *data, nsXPTParamInfo * param, uint8 type, uint8 ind,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, const nsXPTMethodInfo* info,
                        nsXPTCVariant* params);
    nsresult GetArraySizeFromParam( nsIInterfaceInfo *interfaceInfo,
                           const nsXPTMethodInfo* method,
                           const nsXPTParamInfo& param,
                           uint16 methodIndex,
                           uint8 paramIndex,
                           nsXPTCVariant* nativeParams,
                           SizeMode mode,
                           PRUint32* result);
    PRInt16 GetSimpleSize(uint8 type);
    nsresult UnMarshal(void *data, nsXPTParamInfo * param, uint8 type,
                        nsIInterfaceInfo* interfaceInfo, urpPacket* message,
                        PRUint16 methodIndex, bcIAllocator * allocator);
};

#endif
