#ifndef URP_MANGER
#define URP_MANGER

#include "nsIInterfaceInfo.h"
#include "xptinfo.h"
#include "bcDefs.h"
#include "urpTransport.h"
#include "bcIORB.h"

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

typedef struct {
    urpPacket* mess;
    char header;
    bcIID iid;
    bcOID oid;
    bcTID tid;
    bcMID methodId;
    int request;
} forReply;

class urpManager {

friend void send_thread_start (void * arg);

public:
	urpManager(PRBool IsClient, bcIORB *orb, urpConnection* connection);
	~urpManager();
	void SendUrpRequest(bcOID oid, bcIID iid,
                                PRUint16 methodIndex,
                          nsIInterfaceInfo* interfaceInfo,
                          bcICall *call,
                          PRUint32 paramCount, const nsXPTMethodInfo* info,
			  urpConnection* conn);
	nsresult ReadMessage(urpConnection* conn, PRBool ic);
	nsresult SetCall(bcICall* call, PRMonitor *m, bcTID tid);
	nsresult RemoveCall(forReply* fR, bcTID tid);
	nsresult ReadReply(urpPacket* message, char header,
                        bcICall* call, PRUint32 paramCount, 
			const nsXPTMethodInfo *info, 
			nsIInterfaceInfo *interfaceInfo, 
			PRUint16 methodIndex, urpConnection* conn);
	nsresult ReadLongRequest(char header, urpPacket* message,
				bcIID iid, bcOID oid, bcTID tid,
				PRUint16 methodId, urpConnection* conn);
	bcTID GetThread();
private:
	nsHashtable* monitTable;
	bcIORB *broker;
	nsHashtable* threadTable;
/* for ReadReply */
    void TransformMethodIDAndIID();
    nsresult ReadShortRequest(char header, urpPacket* message);
    nsresult SendReply(bcTID tid, bcICall* call, PRUint32 paramCount,
                   const nsXPTMethodInfo* info,
                   nsIInterfaceInfo *interfaceInfo, 
		   PRUint16 methodIndex, urpConnection* conn);
};


#endif
