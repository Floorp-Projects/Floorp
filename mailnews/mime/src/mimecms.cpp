/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corp..
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsICMS.h"
#include "mimecms.h"
#include "nsCRT.h"
#include "nspr.h"
#include "nsEscape.h"
#include "mimemsg.h"
#include "nsIWindowWatcher.h"
#include "nsIPrompt.h"


#define MIME_SUPERCLASS mimeEncryptedClass
MimeDefClass(MimeEncryptedCMS, MimeEncryptedCMSClass,
	     mimeEncryptedCMSClass, &MIME_SUPERCLASS);

static void *MimeCMS_init(MimeObject *,
							int (*output_fn) (const char *, PRInt32, void *),
							void *);
static int MimeCMS_write (const char *, PRInt32, void *);
static int MimeCMS_eof (void *, PRBool);
static char * MimeCMS_generate (void *);
static void MimeCMS_free (void *);
static void MimeCMS_get_content_info (MimeObject *,
										nsICMSMessage **,
										char **, PRInt32 *, PRInt32 *, PRBool *);

extern int SEC_ERROR_CERT_ADDR_MISMATCH;

static int
MimeEncryptedCMSClassInitialize(MimeEncryptedCMSClass *clazz)
{
  MimeObjectClass    *oclass = (MimeObjectClass *)    clazz;
  MimeEncryptedClass *eclass = (MimeEncryptedClass *) clazz;

  PR_ASSERT(!oclass->class_initialized);

  eclass->crypto_init          = MimeCMS_init;
  eclass->crypto_write         = MimeCMS_write;
  eclass->crypto_eof           = MimeCMS_eof;
  eclass->crypto_generate_html = MimeCMS_generate;
  eclass->crypto_free          = MimeCMS_free;

  clazz->get_content_info	   = MimeCMS_get_content_info;

  return 0;
}


typedef struct MimeCMSdata {
  int (*output_fn) (const char *buf, PRInt32 buf_size, void *output_closure);
  void *output_closure;
  nsCOMPtr<nsICMSDecoder> decoder_context;
  nsCOMPtr<nsICMSMessage> content_info;
  PRBool ci_is_encrypted;
  char *sender_addr;
  PRInt32 decode_error;
  PRInt32 verify_error;
  MimeObject *self;
  PRBool parent_is_encrypted_p;
  PRBool parent_holds_stamp_p;
} MimeCMSdata;


static void
MimeCMS_get_content_info(MimeObject *obj,
							   nsICMSMessage **content_info_ret,
							   char **sender_email_addr_return,
							   PRInt32 *decode_error_ret,
							   PRInt32 *verify_error_ret,
                               PRBool *ci_is_encrypted)
{
  MimeEncrypted *enc = (MimeEncrypted *) obj;
  if (enc && enc->crypto_closure)
	{
	  MimeCMSdata *data = (MimeCMSdata *) enc->crypto_closure;

	  *decode_error_ret = data->decode_error;
	  *verify_error_ret = data->verify_error;
	  *content_info_ret = data->content_info;
      *ci_is_encrypted  = data->ci_is_encrypted;

	  if (sender_email_addr_return)
		*sender_email_addr_return = (data->sender_addr
    ? nsCRT::strdup(data->sender_addr)
									 : 0);
	}
}


/*   SEC_PKCS7DecoderContentCallback for SEC_PKCS7DecoderStart() */
static void
MimeCMS_content_callback (void *arg, const char *buf, unsigned long length)
{
  int status;
  MimeCMSdata *data = (MimeCMSdata *) arg;
  if (!data) return;

  if (!data->output_fn)
	return;

  PR_SetError(0,0);
  status = data->output_fn (buf, length, data->output_closure);
  if (status < 0)
	{
	  PR_SetError(status, 0);
	  data->output_fn = 0;
	  return;
	}
}

PRBool
MimeEncryptedCMS_encrypted_p (MimeObject *obj)
{
  PRBool encrypted;

  if (!obj) return PR_FALSE;
  if (mime_typep(obj, (MimeObjectClass *) &mimeEncryptedCMSClass))
	{
	  MimeEncrypted *enc = (MimeEncrypted *) obj;
	  MimeCMSdata *data = (MimeCMSdata *) enc->crypto_closure;
	  if (!data || !data->content_info) return PR_FALSE;
    data->content_info->ContentIsEncrypted(&encrypted);
	  return encrypted;
	}
  return PR_FALSE;
}

// extern MimeMessageClass mimeMessageClass;			/* gag */
extern MimeObjectClass mimeMultipartSignedClass;	/* double gag */

extern int ParseRFC822Addresses (const char *line,
									 char **names, char **addresses);

extern char *IMAP_CreateReloadAllPartsUrl(const char *url);


PRBool
MimeCMSHeadersAndCertsMatch(MimeObject *obj,
							  nsICMSMessage *content_info,
							  char **sender_email_addr_return)
{
  MimeHeaders *msg_headers = 0;
  nsXPIDLCString from_addr;
  nsXPIDLCString from_name;
  nsXPIDLCString sender_addr;
  nsXPIDLCString sender_name;
  nsXPIDLCString cert_name;
  nsXPIDLCString cert_addr;
  PRBool match = PR_TRUE;

  /* Find the name and address in the cert.
   */
  if (content_info)
	{
	  content_info->GetSignerCommonName (getter_Copies(cert_name));
	  content_info->GetSignerEmailAddress (getter_Copies(cert_addr));
	}
  if (!cert_name && !cert_addr) goto DONE;


  /* Find the headers of the MimeMessage which is the parent (or grandparent)
	 of this object (remember, crypto objects nest.) */
  {
	MimeObject *o2 = obj;
	msg_headers = o2->headers;
	while (o2 &&
		   o2->parent &&
		   !mime_typep(o2->parent, (MimeObjectClass *) &mimeMessageClass))
	  {
		o2 = o2->parent;
		msg_headers = o2->headers;
	  }
  }

  if (!msg_headers) goto DONE;

  /* Find the names and addresses in the From and/or Sender fields.
   */
  {
	char *s;

	/* Extract the name and address of the "From:" field. */
	s = MimeHeaders_get(msg_headers, HEADER_FROM, PR_FALSE, PR_FALSE);
	if (s)
	  {
		int n = ParseRFC822Addresses(s, getter_Copies(from_name), getter_Copies(from_addr));
		PR_FREEIF(s);
	  }

	/* Extract the name and address of the "Sender:" field. */
	s = MimeHeaders_get(msg_headers, HEADER_SENDER, PR_FALSE, PR_FALSE);
	if (s)
	  {
		int n = ParseRFC822Addresses(s, getter_Copies(sender_name), getter_Copies(sender_addr));
		PR_FREEIF(s);
	  }
  }

  /* Now compare them --
	 consider it a match if the address in the cert matches either the
	 address in the From or Sender field; and if the name in the cert
	 matches either the name in the From or Sender field.

	 Consider it a match if the cert does not contain a name (address.)
	 But do not consider it a match if the cert contains a name (address)
	 but the message headers do not.
   */

  /* ======================================================================
	 First check the addresses.
   */

  /* If there is no addr in the cert, then consider it a match. */
  if (!cert_addr)
	match = PR_TRUE;

  /* If there is both a from and sender address, and if neither of
	 them match, then error. */
  else if (from_addr && *from_addr &&
		   sender_addr && *sender_addr)
	{
    if (nsCRT::strcasecmp(cert_addr, from_addr) &&
        nsCRT::strcasecmp(cert_addr, sender_addr))
		match = PR_FALSE;
	}
  /* If there is a from but no sender, and it doesn't match, then error. */
  else if (from_addr && *from_addr)
	{
    if (nsCRT::strcasecmp(cert_addr, from_addr))
		match = PR_FALSE;
	}
  /* If there is a sender but no from, and it doesn't match, then error. */
  else if (sender_addr && *sender_addr)
	{
    if (nsCRT::strcasecmp(cert_addr, sender_addr))
		match = PR_FALSE;
	}
  /* Else there are no addresses at all -- error. */
  else
	{
	  match = PR_FALSE;
	}


  if (sender_email_addr_return) {
    if (match && cert_addr)
      *sender_email_addr_return = nsCRT::strdup(cert_addr);
    else if (from_addr && *from_addr)
      *sender_email_addr_return = nsCRT::strdup(from_addr);
    else if (sender_addr && *sender_addr)
      *sender_email_addr_return = nsCRT::strdup(sender_addr);
    else
      *sender_email_addr_return = 0;
  }

 DONE:
  return match;
}

static void *
MimeCMS_init(MimeObject *obj,
			   int (*output_fn) (const char *buf, PRInt32 buf_size,
								 void *output_closure),
			   void *output_closure)
{
  MimeCMSdata *data;
  MimeDisplayOptions *opts;
  nsresult rv;

  if (!(obj && obj->options && output_fn)) return 0;

  opts = obj->options;
  data = (MimeCMSdata *) PR_MALLOC(sizeof(*data));
  if (!data) return 0;

  nsCRT::memset(data, 0, sizeof(*data));

  data->self = obj;
  data->output_fn = output_fn;
  data->output_closure = output_closure;
  PR_SetError(0, 0);
  data->decoder_context = do_CreateInstance(NS_CMSDECODER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return 0;

  rv = data->decoder_context->Start(MimeCMS_content_callback, data);
  if (NS_FAILED(rv)) return 0;

  // XXX Fix later XXX //
  data->parent_holds_stamp_p =
	(obj->parent &&
	 (mime_crypto_stamped_p(obj->parent) ||
	  mime_typep(obj->parent, (MimeObjectClass *) &mimeEncryptedClass)));

  data->parent_is_encrypted_p =
	(obj->parent && MimeEncryptedCMS_encrypted_p (obj->parent));

  /* If the parent of this object is a crypto-blob, then it's the grandparent
	 who would have written out the headers and prepared for a stamp...
	 (This shit sucks.)
   */
  if (data->parent_is_encrypted_p &&
	  !data->parent_holds_stamp_p &&
	  obj->parent && obj->parent->parent)
	data->parent_holds_stamp_p =
	  mime_crypto_stamped_p (obj->parent->parent);

  return data;
}

static int
MimeCMS_write (const char *buf, PRInt32 buf_size, void *closure)
{
  MimeCMSdata *data = (MimeCMSdata *) closure;
  nsresult rv;

  if (!data || !data->output_fn || !data->decoder_context) return -1;

  PR_SetError(0, 0);
  rv = data->decoder_context->Update(buf, buf_size);
  if (NS_FAILED(rv)) {
    data->verify_error = -1;
  }

  return 0;
}

static int
MimeCMS_eof (void *crypto_closure, PRBool abort_p)
{
  MimeCMSdata *data = (MimeCMSdata *) crypto_closure;
  nsresult rv;
  nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService("@mozilla.org/embedcomp/window-watcher;1"));
  nsCOMPtr<nsIPrompt> prompter;
  wwatch->GetNewPrompter(0, getter_AddRefs(prompter));

  if (!data || !data->output_fn || !data->decoder_context)
	return -1;

  /* Hand an EOF to the crypto library.  It may call data->output_fn.
	 (Today, the crypto library has no flushing to do, but maybe there
	 will be someday.)

	 We save away the value returned and will use it later to emit a
	 blurb about whether the signature validation was cool.

   NOTE: We currently put up a dialog box to alert the user. This will go away
         very soon
   */

  PR_SetError(0, 0);
  rv = data->decoder_context->Finish(getter_AddRefs(data->content_info));
  if (NS_FAILED(rv)) {
	  data->verify_error = PR_GetError();
    nsString msg(NS_LITERAL_STRING("Error decrypted message that was encrypted by the sender").get());
    prompter->Alert(0, msg.get());
  } else {
    nsString msg(NS_LITERAL_STRING("Successfully decrypted message that was encrypted by the sender").get());
    prompter->Alert(0, msg.get());
  }
  data->decoder_context = 0;

  /* Is the content info encrypted? */
  if (data->content_info) {
    data->ci_is_encrypted = PR_TRUE;
  }

  return 0;
}

static void
MimeCMS_free (void *crypto_closure)
{
  MimeCMSdata *data = (MimeCMSdata *) crypto_closure;
  if (!data) return;

  PR_FREEIF(data->sender_addr);

  if (data->content_info)
	{
    // Free reference to nsICMSMessage //
	  data->content_info = 0;
	}

  // Do an orderly release of nsICMSDecoder and nsICMSMessage //
  if (data->decoder_context)
	{
    nsCOMPtr<nsICMSMessage> cinfo;
    data->decoder_context->Finish(getter_AddRefs(cinfo));
    data->decoder_context = 0;
	}

  PR_FREEIF(data);
}

char *
MimeCMS_MakeSAURL(MimeObject *obj)
{
  char *stamp_url = 0;

  /* Skip over any crypto objects which lie between us and a message/rfc822.
	 But if we reach an object that isn't a crypto object or a message/rfc822
	 then stop on the crypto object *before* it.  That is, leave `obj' set to
	 either a crypto object, or a message/rfc822, but leave it set to the
	 innermost message/rfc822 above a consecutive run of crypto objects.
   */
  while (1)
	{
	  if (!obj->parent)
		break;
	  else if (mime_typep (obj->parent, (MimeObjectClass *) &mimeMessageClass))
		{
		  obj = obj->parent;
		  break;
		}
#if 0 // XXX Fix later XXX //
	  else if (!mime_typep (obj->parent,
							(MimeObjectClass *) &mimeEncryptedClass) &&
			   !mime_typep (obj->parent,
							(MimeObjectClass *) &mimeMultipartSignedClass))
#endif
	  else if (!mime_typep (obj->parent,
							(MimeObjectClass *) &mimeEncryptedClass))
		{
		  break;
		}
	  obj = obj->parent;
	  PR_ASSERT(obj);
	}


  if (obj->options)
	{
	  const char *base_url = obj->options->url;
	  char *id = (base_url ? mime_part_address (obj) : 0);
	  char *url = (id && base_url
				   ? mime_set_url_part(base_url, id, PR_TRUE)
				   : 0);
	  char *url2 = (url ? nsEscape(url, url_XAlphas) : 0);
	  PR_FREEIF(id);
	  PR_FREEIF(url);

    stamp_url = (char *) PR_MALLOC(nsCRT::strlen(url2) + 50);
	  if (stamp_url)
		{
      PL_strcpy(stamp_url, "about:security?advisor=");
      PL_strcat(stamp_url, url2);
		}
	  PR_FREEIF(url2);
	}
  return stamp_url;
}

static char *
MimeCMS_generate (void *crypto_closure)
{
  MimeCMSdata *data = (MimeCMSdata *) crypto_closure;
  PRBool self_signed_p = PR_FALSE;
  PRBool self_encrypted_p = PR_FALSE;
  PRBool union_encrypted_p = PR_FALSE;
  PRBool good_p = PR_TRUE;
  PRBool unverified_p = PR_FALSE;

  if (!data || !data->output_fn) return 0;

  if (data->content_info)
	{
	  data->content_info->ContentIsSigned(&self_signed_p);
	  data->content_info->ContentIsEncrypted(&self_encrypted_p);
	  union_encrypted_p = (self_encrypted_p || data->parent_is_encrypted_p);

	  if (self_signed_p)
		{
		  PR_SetError(0, 0);
      good_p = data->content_info->VerifySignature();
		  if (!good_p)
			{
			  if (!data->verify_error)
				data->verify_error = PR_GetError();
			  if (data->verify_error >= 0)
				data->verify_error = -1;
			}
		  else
			{
			  good_p = MimeCMSHeadersAndCertsMatch(data->self,
													 data->content_info,
													 &data->sender_addr);
			  if (!good_p && !data->verify_error)
				// data->verify_error = SEC_ERROR_CERT_ADDR_MISMATCH; XXX Fix later XXX //
        data->verify_error = -1;
			}
		}

#if 0 
	  if (SEC_PKCS7ContainsCertsOrCrls(data->content_info))
		{
		  /* #### call libsec telling it to import the certs */
		}
#endif

	  /* Don't free these yet -- keep them around for the lifetime of the
		 MIME object, so that we can get at the security info of sub-parts
		 of the currently-displayed message. */
#if 0
	  SEC_PKCS7DestroyContentInfo(data->content_info);
	  data->content_info = 0;
#endif /* 0 */
	}
  else
	{
	  /* No content info?  Something's horked.  Guess. */
	  self_encrypted_p = PR_TRUE;
	  union_encrypted_p = PR_TRUE;
	  good_p = PR_FALSE;
	  if (!data->decode_error && !data->verify_error)
		data->decode_error = -1;
	}

  unverified_p = data->self->options->missing_parts;

  if (data->self && data->self->parent)
	  mime_set_crypto_stamp(data->self->parent, self_signed_p, self_encrypted_p);


  {
	char *stamp_url = 0, *result = nsnull;
	if (data->self)
	{
		if (unverified_p && data->self->options)
			// stamp_url = IMAP_CreateReloadAllPartsUrl(data->self->options->url); XXX Fix later XXX //
      stamp_url = nsnull;
		else
			stamp_url = MimeCMS_MakeSAURL(data->self);
	}

	result =
	  MimeHeaders_make_crypto_stamp (union_encrypted_p,
									 self_signed_p,
									 good_p,
									 unverified_p,
									 data->parent_holds_stamp_p,
									 stamp_url);
	PR_FREEIF(stamp_url);
	return result;
  }
}
