/*
 * DO NOT EDIT.  THIS FILE IS GENERATED FROM nsIMsgSend.idl
 */

#ifndef __gen_nsIMsgSend_h__
#define __gen_nsIMsgSend_h__

#include "nsISupports.h" /* interface nsISupports */
#include "nsIMsgCompFields.h" /* interface nsIMsgCompFields */
#include "nsID.h" /* interface nsID */
#include "rosetta.h"

#ifdef XPIDL_JS_STUBS
#include "jsapi.h"
#endif

typedef enum
{
  nsMsgDeliverNow,
  nsMsgQueueForLater,
  nsMsgSave,
  nsMsgSaveAs,
  nsMsgSaveAsDraft,
  nsMsgSaveAsTemplate
} nsMsgDeliverMode;

struct nsMsgAttachmentData
{
  char *url;			/* The URL to attach.
						   This should be 0 to signify "end of list".
						 */

  char *desired_type;	/* The type to which this document should be
						   converted.  Legal values are NULL, TEXT_PLAIN
						   and APPLICATION_POSTSCRIPT (which are macros
						   defined in net.h); other values are ignored.
						 */

  char *real_type;		/* The type of the URL if known, otherwise NULL.
						   For example, if you were attaching a temp file
						   which was known to contain HTML data, you would
						   pass in TEXT_HTML as the real_type, to override
						   whatever type the name of the tmp file might
						   otherwise indicate.
						 */
  char *real_encoding;	/* Goes along with real_type */

  char *real_name;		/* The original name of this document, which will
						   eventually show up in the Content-Disposition
						   header.  For example, if you had copied a
						   document to a tmp file, this would be the
						   original, human-readable name of the document.
						 */

  char *description;	/* If you put a string here, it will show up as
						   the Content-Description header.  This can be
						   any explanatory text; it's not a file name.
						 */

  char *x_mac_type, *x_mac_creator;
						/* Mac-specific data that should show up as optional parameters
						   to the content-type header.
						 */
};


/* This structure is the interface between compose.c and composew.c.
   When we have downloaded a URL to a tmp file for attaching, this
   represents everything we learned about it (and did to it) in the
   process. 
 */
/* Used by libmime -- mimedrft.c
 * Front end shouldn't use this structure.
 */
typedef struct nsMsgAttachedFile
{
  char *orig_url;		/* Where it came from on the network (or even elsewhere
						   on the local disk.)
						 */
  char *file_name;		/* The tmp file in which the (possibly converted) data
						   now resides.
						*/
  char *type;			/* The type of the data in file_name (not necessarily
						   the same as the type of orig_url.)
						 */
  char *encoding;		/* Likewise, the encoding of the tmp file.
						   This will be set only if the original document had
						   an encoding already; we don't do base64 encoding and
						   so forth until it's time to assemble a full MIME
						   message of all parts.
						 */

  /* #### I'm not entirely sure where this data is going to come from...
   */
  char *description;					/* For Content-Description header */
  char *x_mac_type, *x_mac_creator;		/* mac-specific info */
  char *real_name;						/* The real name of the file. */

  /* Some statistics about the data that was written to the file, so that when
	 it comes time to compose a MIME message, we can make an informed decision
	 about what Content-Transfer-Encoding would be best for this attachment.
	 (If it's encoded already, we ignore this information and ship it as-is.)
   */
  uint32 size;
  uint32 unprintable_count;
  uint32 highbit_count;
  uint32 ctl_count;
  uint32 null_count;
  uint32 max_line_length;
  
  HG68452

} nsMsgAttachedFile;


/* starting interface:    nsIMsgSend */

/* {9E9BD970-C5D6-11d2-8297-000000000000} */
#define NS_IMSGSEND_IID_STR "9E9BD970-C5D6-11d2-8297-000000000000"
#define NS_IMSGSEND_IID \
  {0x9E9BD970, 0xC5D6, 0x11d2, \
    { 0x82, 0x97, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }}

class nsIMsgSend : public nsISupports {
 public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMSGSEND_IID)

  NS_IMETHOD SendMessage(
 						  nsIMsgCompFields                  *fields,
              const char                        *smtp,
						  PRBool                            digest_p,
						  PRBool                            dont_deliver_p,
						  PRInt32                           mode, 
						  const char                        *attachment1_type,
						  const char                        *attachment1_body,
						  PRUint32                          attachment1_body_length,
						  const struct nsMsgAttachmentData  *attachments,
						  const struct nsMsgAttachedFile    *preloaded_attachments,
						  void                              *relatedPart, /* nsMsgSendPart  */
						  void                              (*message_delivery_done_callback)(void *context, void *fe_data,
								                                                                  int status, const char *error_message)) = 0;

#ifdef XPIDL_JS_STUBS
  static NS_EXPORT_(JSObject *) InitJSClass(JSContext *cx);
  static NS_EXPORT_(JSObject *) GetJSObject(JSContext *cx, nsIMsgSend *priv);
#endif
};

#endif /* __gen_nsIMsgSend_h__ */
