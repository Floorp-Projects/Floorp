/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM bsIConnection.idl
 */

#ifndef __gen_bsIConnection_h__
#define __gen_bsIConnection_h__

#include "nsISupports.h"
#include "nsrootidl.h"

/* starting interface:    bsIConnection */

#define BSICONNECTION_IID_STR "acedc9b0-395c-11d3-aeb9-0000f8e25c06"
#define BSICONNECTION_IID \
  {0xacedc9b0, 0x395c, 0x11d3, \
    { 0xae, 0xb9, 0x00, 0x00, 0xf8, 0xe2, 0x5c, 0x06 }}

class bsIConnection : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(BSICONNECTION_IID)

  /* attribute boolean isConnected; */
  NS_IMETHOD GetIsConnected(PRBool *aIsConnected) = 0;
  NS_IMETHOD SetIsConnected(PRBool aIsConnected) = 0;

  /* attribute boolean linebufferFlag; */
  NS_IMETHOD GetLinebufferFlag(PRBool *aLinebufferFlag) = 0;
  NS_IMETHOD SetLinebufferFlag(PRBool aLinebufferFlag) = 0;

  /* attribute unsigned short port; */
  NS_IMETHOD GetPort(PRUint16 *aPort) = 0;
  NS_IMETHOD SetPort(PRUint16 aPort) = 0;

  /* void init (in string hostname); */
  NS_IMETHOD init(const char *hostname) = 0;

  /* void destroy (); */
  NS_IMETHOD destroy(void) = 0;

  /* boolean disconnect (); */
  NS_IMETHOD disconnect(PRBool *_retval) = 0;

  /* boolean connect (in unsigned short port, in string bindIP, in boolean tcpFlag); */
  NS_IMETHOD connect(PRUint16 port, const char *bindIP, PRBool tcpFlag, PRBool *_retval) = 0;

  /* void sendData (in string data); */
  NS_IMETHOD sendData(const char *data) = 0;

  /* boolean hasData (); */
  NS_IMETHOD hasData(PRBool *_retval) = 0;

  /* string readData (in unsigned long timeout); */
  NS_IMETHOD readData(PRUint32 timeout, char **_retval) = 0;
};
// {25b7b6e0-3af4-11d3-aeb9-0000f8e25c06}
#define BS_CONNECTION_CID \
{ 0x25b7b6e0, 0x3af4, 0x11d3, { 0xae, 0xb9, 0x0, 0x0, 0xf8, 0xe2, 0x5c, 0x6 }} 
#define BS_CONNECTION_PROGID "component://misc/bs/connection"
#define BS_CONNECTION_CLASSNAME "bsConnection"
extern nsresult
BS_NewConnection(bsIConnection** aConnection);

#endif /* __gen_bsIConnection_h__ */
