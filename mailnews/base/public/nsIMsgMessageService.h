/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgMessageService.idl
 */

#ifndef __gen_nsIMsgMessageService_h__
#define __gen_nsIMsgMessageService_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsrootidl.h" /* interface nsrootidl */
class nsIURL; /* forward decl */
class nsIUrlListener; /* forward decl */
class nsIStreamListener; /* forward decl */
class nsIFileSpec; /* forward decl */

/* starting interface:    nsIMsgMessageService */

/* {F11009C1-F697-11d2-807F-006008128C4E} */
#define NS_IMSGMESSAGESERVICE_IID_STR "F11009C1-F697-11d2-807F-006008128C4E"
#define NS_IMSGMESSAGESERVICE_IID \
  {0xF11009C1, 0xF697, 0x11d2, \
    { 0x80, 0x7F, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMsgMessageService : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGMESSAGESERVICE_IID)
     
     /////////////////////////////////////////////////////////////////
     // If you want a handle on the running task, pass in a valid nsIURL 
     // ptr. You can later interrupt this action by asking the netlib 
     // service manager to interrupt the url you are given back. 
     // Remember to release aURL when you are done with it. Pass nsnull
     // in for aURL if you don't  care about the returned URL.
     /////////////////////////////////////////////////////////////////
     /////////////////////////////////////////////////////////////////
     // CopyMessage: Pass in the URI for the message you want to have 
     // copied.
     // aCopyListener already knows about the destination folder. 
     // Set aMoveMessage to PR_TRUE if you want the message to be moved.
     // PR_FALSE leaves it as just a copy.
     ///////////////////////////////////////////////////////////////
  


  /* void CopyMessage (in string aSrcURI, in nsIStreamListener aCopyListener, in boolean aMoveMessage, in nsIUrlListener aUrlListener, out nsIURL aURL); */
  NS_IMETHOD CopyMessage(const char *aSrcURI, nsIStreamListener *aCopyListener, PRBool aMoveMessage, nsIUrlListener *aUrlListener, nsIURL **aURL) = 0;
     /////////////////////////////////////////////////////////////////////
     //  DisplayMessage: When you want a message displayed....
     //  aMessageURI is a uri representing the message to display.
     //  aDisplayConsumer is (for now) a nsIWebshell which we'll use to load 
     // the message into. 
     // It would be nice if we can figure this out for ourselves in the 
     // protocol but we can't do that right now. 
     ///////////////////////////////////////////////////////////////////


  /* void DisplayMessage (in string aMessageURI, in nsISupports aDisplayConsumer, in nsIUrlListener aUrlListener, out nsIURL aURL); */
  NS_IMETHOD DisplayMessage(const char *aMessageURI, nsISupports *aDisplayConsumer, nsIUrlListener *aUrlListener, nsIURL **aURL) = 0;
     /////////////////////////////////////////////////////////////////////
     // SaveMessageToDisk: When you want to spool a message out to a file
	 // on disk. This is an asynch operation of course. You must pass in a
	 // url listener in order to figure out when the operation is done.
	 // aMessageURI--> uri representing the message to spool out to disk.
	 // aFile - the file you want the message saved to
	 // aAppendToFile --> usually FALSE. Set to TRUE if you want the msg
	 //					  appended at the end of the file. 
     ///////////////////////////////////////////////////////////////////


  /* void SaveMessageToDisk (in string aMessageURI, in nsIFileSpec aFile, in boolean aAppendToFile, in nsIUrlListener aUrlListener, out nsIURL aURL); */
  NS_IMETHOD SaveMessageToDisk(const char *aMessageURI, nsIFileSpec *aFile, PRBool aAppendToFile, nsIUrlListener *aUrlListener, nsIURL **aURL) = 0;
};

#endif /* __gen_nsIMsgMessageService_h__ */
