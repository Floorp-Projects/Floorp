/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMailboxUrl.idl
 */

#ifndef __gen_nsIMailboxUrl_h__
#define __gen_nsIMailboxUrl_h__

#include "nsISupports.h"
#include "nsIFileSpec.h"
#include "nsrootidl.h"
#include "MailNewsTypes2.h"
class nsIStreamListener; /* forward decl */
class nsIMsgDBHdr; /* forward decl */
#include "nsIStreamListener.h"
typedef PRInt32  nsMailboxAction;

/* starting interface:    nsIMailboxUrl */

/* {C272A1C1-C166-11d2-804E-006008128C4E} */
#define NS_IMAILBOXURL_IID_STR "C272A1C1-C166-11d2-804E-006008128C4E"
#define NS_IMAILBOXURL_IID \
  {0xC272A1C1, 0xC166, 0x11d2, \
    { 0x80, 0x4E, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMailboxUrl : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMAILBOXURL_IID)

  /* attribute nsIStreamListener mailboxParser; */
  NS_IMETHOD GetMailboxParser(nsIStreamListener * *aMailboxParser) = 0;
  NS_IMETHOD SetMailboxParser(nsIStreamListener * aMailboxParser) = 0;

  /* attribute nsIStreamListener mailboxCopyHandler; */
  NS_IMETHOD GetMailboxCopyHandler(nsIStreamListener * *aMailboxCopyHandler) = 0;
  NS_IMETHOD SetMailboxCopyHandler(nsIStreamListener * aMailboxCopyHandler) = 0;

  /* readonly attribute nsFileSpecPtr filePath; */
  NS_IMETHOD GetFilePath(nsFileSpec * *aFilePath) = 0;

  /* readonly attribute nsIMsgDBHdr messageHeader; */
  NS_IMETHOD GetMessageHeader(nsIMsgDBHdr * *aMessageHeader) = 0;

  /* readonly attribute nsMsgKey messageKey; */
  NS_IMETHOD GetMessageKey(nsMsgKey *aMessageKey) = 0;

  /* attribute unsigned long messageSize; */
  NS_IMETHOD GetMessageSize(PRUint32 *aMessageSize) = 0;
  NS_IMETHOD SetMessageSize(PRUint32 aMessageSize) = 0;

  /* attribute nsMailboxAction mailboxAction; */
  NS_IMETHOD GetMailboxAction(nsMailboxAction *aMailboxAction) = 0;
  NS_IMETHOD SetMailboxAction(nsMailboxAction aMailboxAction) = 0;

  /* attribute nsIFileSpec messageFile; */
  NS_IMETHOD GetMessageFile(nsIFileSpec * *aMessageFile) = 0;
  NS_IMETHOD SetMessageFile(nsIFileSpec * aMessageFile) = 0;

  enum { ActionParseMailbox = 0 };

  enum { ActionDisplayMessage = 1 };

  enum { ActionCopyMessage = 2 };

  enum { ActionMoveMessage = 3 };

  enum { ActionSaveMessageToDisk = 4 };

  enum { ActionAppendMessageToDisk = 5 };
};

#endif /* __gen_nsIMailboxUrl_h__ */
