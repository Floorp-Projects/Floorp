


#include "xp_core.h"
#include "prtypes.h"
#define RESOURCE_STR

// Dirty trick:

#define _RESDEF_H_

#define RES_OFFSET 7000
#define RES_START
#define BEGIN_STR(FUNC_NAME) static char * (FUNC_NAME) (int16 i) {switch(i) {
#define ResDef(NAME, ID, STRING) case (ID)+ RES_OFFSET: return (STRING); 
#define END_STR(FUNC_NAME) } return 0; }

#include <allxpstr.h>

#undef _RESDEF_H_

extern "C"
PUBLIC char *XP_GetBuiltinString(int16 i)
{
	static char	buf[128];
	char		*ret;

	i += RES_OFFSET;

	if
	(
		((ret = mcom_include_merrors_i_strings (i))) ||
		((ret = mcom_include_secerr_i_strings  (i))) ||
		((ret = mcom_include_sec_dialog_strings(i))) ||
		((ret = mcom_include_sslerr_i_strings  (i))) ||
		((ret = mcom_include_xp_error_i_strings(i))) ||
		((ret = mcom_include_xp_msg_i_strings  (i)))
	)
	{
		return ret;
	}

	(void) sprintf(buf, "XP_GetBuiltinString: %d not found", i);

	return buf;
}

#include "client.h"
#include "qmsgbox.h"
#include "qdict.h"
#include "qapp.h"

class CDNSObj
{
public:
    CDNSObj();
    ~CDNSObj();
    int		     i_finished;
    XP_List	   * m_sock_list;
    int 	     m_iError;
    char	   * m_host;
    struct hostent * m_hostent;
    HANDLE           m_handle;
    // Used by FE_DeleteDNSList in fenet.cpp
    MWContext		* context;
};

CDNSObj::CDNSObj()
{
    //  make sure we're clean.
    m_hostent = NULL;
    m_host = NULL;
    m_handle = NULL;
	context = NULL;
    m_iError = 0;
    i_finished = 0;
    m_sock_list = 0;
}

CDNSObj::~CDNSObj()
{
    if(m_hostent)   {
        XP_FREE(m_hostent);
        m_hostent = NULL;
    }
    if(m_host)  {
        XP_FREE(m_host);
        m_host = NULL;
    }

    if (m_sock_list) {
	while (XP_ListRemoveTopObject(m_sock_list))
	    ;
	XP_ListDestroy(m_sock_list);
    }
}

QDict<CDNSObj> DNSCacheMap;

const UINT NEAR msg_FoundDNS = RegisterWindowMessage("NetscapeDNSFound");

class QtDNSHandler : public QWidget
{
public:
    QtDNSHandler() : QWidget(0) {}
    bool winEvent( MSG* );
};

static QtDNSHandler *dnsHandler = 0;

// Large portions taken from hiddenfr.cpp in the windows fe.

bool QtDNSHandler::winEvent( MSG *m )
{
    if ( m->message == msg_FoundDNS ) {
	int iError = WSAGETASYNCERROR( m->lParam );

	//  Go through the DNS cache, find the correct task ID.
	//  The find should always be successful.
	//  Be sure to initalize values.
	CDNSObj *obj = NULL;

	QDictIterator<CDNSObj> it(DNSCacheMap);
	for( obj = it.toFirst() ; obj ; obj = ++it ) {
	    // Since the handle is not unique for the session only
	    // compare handles that are currently in use (i.e. active entries)
	    if(!obj->i_finished && obj->m_handle == (HANDLE)m->wParam)
		break;
	}
	if(!obj)
	    return TRUE;
  
	//	TRACE("%s error=%d h_name=%d task=%d\n", obj->m_host, iError,
	//	      (obj->m_hostent->h_name != NULL) ? 1 : 0, obj->m_handle);

	//  Mark this as completed.
	//
	obj->i_finished = 1;
  
	//  If there was an error, set it.
	if (iError) {
	    // TRACE("DNS Lookup failed! \n"); 
	    obj->m_iError = iError;
	}
  	
	/* call ProcessNet for each socket in the list */
	/* use a for loop so that we don't reference the "obj"
	 * after our last call to processNet.  We need to do
	 * this because the "obj" can get free'd by the call
	 * chain after all the sockets have been removed from
	 * sock_list
	 */
	PRFileDesc *tmp_sock;
	int count = XP_ListCount(obj->m_sock_list);
	while( count-- ) {
	    tmp_sock = (PRFileDesc *) XP_ListRemoveTopObject(obj->m_sock_list);

	    int iWantMore;
	    static int recursive = FALSE;
#if 0
	    if ( recursive )
		QMessageBox::information( 0, "", "RECURSIVE!" );
	    else
		QMessageBox::information( 0, "", "NON-RECURSIVE!" );
#endif
	    recursive = TRUE;
	    iWantMore = NET_ProcessNet( tmp_sock, NET_SOCKET_FD);
	    recursive = FALSE;
	}
	return TRUE;
    }
    return FALSE;
}

#if 1
int FE_AsyncDNSLookup(MWContext *context, char * host_port, PRHostEnt ** hoststruct_ptr_ptr, PRFileDesc *socket)
{
    //  Initialize our DNS objects.
    CDNSObj * dns_obj=NULL;
    *hoststruct_ptr_ptr = NULL;

    //  Look up and see if the host is in our cached DNS lookups and if so....
    if ( (dns_obj = DNSCacheMap.find(host_port) ) ) {
	  //  See if the cached value was an error, and if so, return an error.
          //  Should we retry the lookup if it was?
	if (dns_obj->m_iError != 0 && dns_obj->i_finished != 0) {  // DNS error
            WSASetLastError(dns_obj->m_iError);
            DNSCacheMap.remove( host_port );
			// Only delete the dns object if there are no
			// more sockets in the sock list
            if ( dns_obj && XP_ListCount( dns_obj->m_sock_list ) == 0 ) 
		delete dns_obj;
            return -1;
        }	
        //  If this isn't NULL, then we have a good match.
	//  Return it as such and clear the context.
        else if (dns_obj->m_hostent->h_name != NULL && dns_obj->i_finished != 0) {
            *hoststruct_ptr_ptr = (PRHostEnt *)dns_obj->m_hostent;
            return 0;
        }
        //  There isn't an error, and there isn't a host name,
	//  return that we are waiting for the lookup....
	//  This doesn't make clear sense, unless you take into
	//  account that the hostent structure is empty when
	//  another window is also looking this same host up,
        //  so we return that we are waiting too.
        else {
			/* see if the socket we are looking up is already in
			 * the socket list.  If it isn't, add it.
			 * The socket list provides us with a way to
			 * send events to NET_ProcessNet for each socket
			 * waiting for a lookup
			 */
			XP_List * list_ptr = dns_obj->m_sock_list;
			PRFileDesc *tmp_sock;

			if(list_ptr)
				list_ptr = list_ptr->next;

			while(list_ptr) {
				tmp_sock = (PRFileDesc *) list_ptr->object;

				if(tmp_sock == socket)
					return MK_WAITING_FOR_LOOKUP;

				list_ptr = list_ptr->next;
			}

			/* socket not in the list, add it */
			XP_ListAddObject(dns_obj->m_sock_list, (void *)socket);
			return MK_WAITING_FOR_LOOKUP;
        }   
    } else {
	//  There is no cache entry, begin the async dns lookup.
	//  Capture the current window handle, 
	//  we pass this to the async code, for passing
	//      back a message when the lookup is complete.
	//  Actually, just pass the application's main window to avoid
	//      needing a frame window to do DNS lookups.
	if ( !dnsHandler )
	    dnsHandler = new QtDNSHandler;
	HWND hWndFrame = dnsHandler->winId();
    
	//  Create and initialize our dns object.  Allocating hostent struct, 
	//  host name and port string, and error code from the lookup.
	dns_obj = new CDNSObj();
	dns_obj->m_hostent = 
	    (struct hostent *)XP_ALLOC(MAXGETHOSTSTRUCT*sizeof(char));
	if(dns_obj->m_hostent) {
	    memset(dns_obj->m_hostent, 0,MAXGETHOSTSTRUCT*sizeof(char));
	}
        dns_obj->m_host = XP_STRDUP(host_port);
	dns_obj->m_sock_list = XP_ListNew();
	dns_obj->context = context;
	XP_ListAddObject( dns_obj->m_sock_list, (void *) socket );

	//  Insert the entry into the cached DNS lookups.
	//  Also, set the context, and actually begin the DNS lookup.
	//  Return that we're waiting for it to complete.

	if( !(dns_obj->m_handle = WSAAsyncGetHostByName(hWndFrame,
					msg_FoundDNS,
					dns_obj->m_host,
					(char *)dns_obj->m_hostent,
					MAXGETHOSTSTRUCT) ) ) {
	    delete dns_obj;
	    return -1;
	}

	DNSCacheMap.insert( host_port, dns_obj );

	return MK_WAITING_FOR_LOOKUP;
    }
}

#else

int FE_AsyncDNSLookup(MWContext *context, char * host_port, PRHostEnt ** hoststruct_ptr_ptr, PRFileDesc *socket)
{
    QMessageBox::information( 0, "", "FE_ASYNCDNSLOOKUP" );
    return -1;
}

#endif



