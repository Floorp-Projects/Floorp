/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgParseMailMsgState.idl
 */

#ifndef __gen_nsIMsgParseMailMsgState_h__
#define __gen_nsIMsgParseMailMsgState_h__

#include "nsISupports.h"
#include "nsrootidl.h"
class nsIMsgDatabase; /* forward decl */
class nsIMsgDBHdr; /* forward decl */
#include "nsIMsgDatabase.h"
#include "nsIMsgHdr.h"
typedef PRInt32  nsMailboxParseState;

/* starting interface:    nsIMsgParseMailMsgState */

/* {0CC23170-1459-11d3-8097-006008128C4E} */
#define NS_IMSGPARSEMAILMSGSTATE_IID_STR "0CC23170-1459-11d3-8097-006008128C4E"
#define NS_IMSGPARSEMAILMSGSTATE_IID \
  {0x0CC23170, 0x1459, 0x11d3, \
    { 0x80, 0x97, 0x00, 0x60, 0x08, 0x12, 0x8C, 0x4E }}

class nsIMsgParseMailMsgState : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGPARSEMAILMSGSTATE_IID)

  /* void SetMailDB (in nsIMsgDatabase aDatabase); */
  NS_IMETHOD SetMailDB(nsIMsgDatabase *aDatabase) = 0;

  /* void Clear (); */
  NS_IMETHOD Clear(void) = 0;

  /* void SetState (in nsMailboxParseState aState); */
  NS_IMETHOD SetState(nsMailboxParseState aState) = 0;

  /* void SetEnvelopePos (in unsigned long aEnvelopePos); */
  NS_IMETHOD SetEnvelopePos(PRUint32 aEnvelopePos) = 0;

  /* void ParseAFolderLine (in string line, in unsigned long lineLength); */
  NS_IMETHOD ParseAFolderLine(const char *line, PRUint32 lineLength) = 0;

  /* nsIMsgDBHdr GetNewMsgHdr (); */
  NS_IMETHOD GetNewMsgHdr(nsIMsgDBHdr **_retval) = 0;

  /* void FinishHeader (); */
  NS_IMETHOD FinishHeader(void) = 0;

  /* long GetAllHeaders (out string headers); */
  NS_IMETHOD GetAllHeaders(char **headers, PRInt32 *_retval) = 0;

  enum { ParseEnvelope = 0 };

  enum { ParseHeaders = 1 };

  enum { ParseBody = 2 };
};

#endif /* __gen_nsIMsgParseMailMsgState_h__ */
