/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
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
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _LIBMIME_H_
#define _LIBMIME_H_

#ifdef XP_UNIX
#undef Bool
#endif

#include "prtypes.h"
#include "nsMailHeaders.h"
#include "nsIMimeStreamConverter.h"
#include "nsIPref.h"
#include "mozITXTToHTMLConv.h"
#include "nsICharsetConverterManager2.h"

#define MIME_DRAFTS

/* Opaque object describing a block of message headers, and a couple of
   routines for extracting data from one.
 */

typedef struct MimeHeaders 
{
  char *all_headers;			/* A char* of the entire header section. */
  PRInt32 all_headers_fp;			/* The length (it is not NULL-terminated.) */
  PRInt32 all_headers_size;		/* The size of the allocated block. */

  PRBool done_p;				/* Whether we've read the end-of-headers marker
								   (the terminating blank line.) */

  char **heads;					/* An array of length n_headers which points
								   to the beginning of each distinct header:
								   just after the newline which terminated
								   the previous one.  This is to speed search.

								   This is not initialized until all the
								   headers have been read.
								 */
  PRInt32 heads_size;				/* The length (and consequently, how many
								   distinct headers are in here.) */


  char *obuffer;				/* This buffer is used for output. */
  PRInt32 obuffer_size;
  PRInt32 obuffer_fp;

  char *munged_subject;			/* What a hack.  This is a place to write down
								   the subject header, after it's been
								   charset-ified and stuff.  Remembered so that
								   we can later use it to generate the
								   <TITLE> tag. (Also works for giving names to RFC822 attachments) */
} MimeHeaders;

class MimeDisplayOptions;
typedef struct MimeParseStateObject MimeParseStateObject;
typedef struct MSG_AttachmentData MSG_AttachmentData;

/* Given the name of a header, returns the contents of that header as
   a newly-allocated string (which the caller must free.)  If the header
   is not present, or has no contents, NULL is returned.

   If `strip_p' is PR_TRUE, then the data returned will be the first token
   of the header; else it will be the full text of the header.  (This is
   useful for getting just "text/plain" from "text/plain; name=foo".)

   If `all_p' is PR_FALSE, then the first header encountered is used, and
   any subsequent headers of the same name are ignored.  If PR_TRUE, then
   all headers of the same name are appended together (this is useful
   for gathering up all CC headers into one, for example.)
 */
extern char *MimeHeaders_get(MimeHeaders *hdrs,
							 const char *header_name,
							 PRBool strip_p,
							 PRBool all_p);

/* Given a header of the form of the MIME "Content-" headers, extracts a
   named parameter from it, if it exists.  For example,
   MimeHeaders_get_parameter("text/plain; charset=us-ascii", "charset")
   would return "us-ascii".

   Returns NULL if there is no match, or if there is an allocation failure.

	RFC2231 - MIME Parameter Value and Encoded Word Extensions: Character Sets,
	Languages, and Continuations

	RFC2231 has added the character sets, languages, and continuations mechanism.
	charset, and language information may also be returned to the caller.
	For example,
	MimeHeaders_get_parameter("text/plain; name*=us-ascii'en-us'This%20is%20%2A%2A%2Afun%2A%2A%2A", "name")
	MimeHeaders_get_parameter("text/plain; name*0*=us-ascii'en-us'This%20is%20; CRLFLWSPname*1*=%2A%2A%2Afun%2A%2A%2A", "name")
	would return "This is ***fun***" and *charset = "us-ascii", *language = "en-us"
 */
extern char *MimeHeaders_get_parameter (const char *header_value,
										const char *parm_name,
										char **charset,
										char **language);

extern      MimeHeaders *MimeHeaders_copy (MimeHeaders *srcHeaders);

extern void MimeHeaders_free (MimeHeaders *hdrs);
  
typedef enum {
  MimeHeadersAll,				  /* Show all headers */
  MimeHeadersSome,				/* Show all "interesting" headers */
  MimeHeadersSomeNoRef,		/* Same, but suppress the `References' header
								             (for when we're printing this message.) */
  MimeHeadersMicro,				/* Show a one-line header summary */
  MimeHeadersMicroPlus,		/* Same, but show the full recipient list as
								             well (To, CC, etc.) */
  MimeHeadersCitation,		/* A one-line summary geared toward use in a
								             reply citation ("So-and-so wrote:") */
  MimeHeadersOnly,        /* Just parse and output headers...nothing else! */
  MimeHeadersNone         /* Skip showing any headers */
} MimeHeadersState;


/* The signature for various callbacks in the MimeDisplayOptions structure.
 */
typedef char *(*MimeHTMLGeneratorFunction) (const char *data, void *closure,
											MimeHeaders *headers);

class MimeDisplayOptions
{
public:
  MimeDisplayOptions();
  virtual ~MimeDisplayOptions();
  mozITXTToHTMLConv   *conv;        // For text conversion...
  nsIPref             *prefs;       /* Connnection to prefs service manager */
  nsMimeOutputType    format_out;   // The format out type
  nsCString           charsetForCachedInputDecoder;
  nsCOMPtr<nsIUnicodeDecoder>   m_inputCharsetToUnicodeDecoder;
  nsCOMPtr<nsIUnicodeEncoder>   m_unicodeToUTF8Encoder;

  const char *url;			/* Base URL for the document.  This string should
							   be freed by the caller, after the parser
							   completes (possibly at the same time as the
							   MimeDisplayOptions itself.) */

  MimeHeadersState headers;	/* How headers should be displayed. */
  PRBool fancy_headers_p;	/* Whether to do clever formatting of headers
							   using tables, instead of spaces. */

  PRBool output_vcard_buttons_p;	/* Whether to output the buttons */
									/* on vcards. */

  PRBool fancy_links_p;		/* Whether to insert fancy links, so you can
								   do things like click on an email address to
								   add it to your address book.  Something you
								   don't want to do while printing. */

  PRBool variable_width_plaintext_p;	/* Whether text/plain messages should
										   be in variable width, or fixed. */
  PRBool wrap_long_lines_p;	/* Whether to wrap long lines in text/plain
								   messages. */

  PRBool rot13_p;			/* Whether text/plain parts should be rotated
							   Set by "?rot13=true" */
  char *part_to_load;		/* The particular part of the multipart which
							   we are extracting.  Set by "?part=3.2.4" */

  PRBool write_html_p;		/* Whether the output should be HTML, or raw. */

  PRBool decrypt_p;		/* Whether all traces of xlateion should be
							   eradicated -- this is only meaningful when
							   write_html_p is PR_FALSE; we set this when
							   attaching a message for forwarding, since
							   forwarding someone else a message that wasn't
							   xlated for them doesn't work.  We have to
							   dexlate it before sending it.
							 */

  PRBool nice_html_only_p;		/* If PR_TRUE, then we only should write html if
								   it's pretty HTML (stuff that we're willing
								   to get shipped out in mail messages).  If we
								   can't generate nice stuff for some part,
								   then don't say anything at all. */

  PRUint32 whattodo ; 			/* from the prefs, we'll get if the user wants to do glyph or structure substitutions and set this member variable. */

  char *default_charset;	/* If this is non-NULL, then it is the charset to
							   assume when no other one is specified via a
							   `charset' parameter.
							 */
  PRBool override_charset;	/* If this is PR_TRUE, then we will assume that
							   all data is in the default_charset, regardless
                               of what the `charset' parameter of that part
                               says. (This is to cope with the fact that, in
                               the real world, many messages are mislabelled
                               with the wrong charset.)
							 */
  PRBool  force_user_charset; /* this is the new strategy to deal with incorrectly
                                 labeled attachments */

  /* =======================================================================
	 Stream-related callbacks; for these functions, the `closure' argument
	 is what is found in `options->stream_closure'.  (One possible exception
	 is for output_fn; see "output_closure" below.)
   */
  void *stream_closure;

  /* For setting up the display stream, so that the MIME parser can inform
	 the caller of the type of the data it will be getting. */
  int (*output_init_fn) (const char *type,
						 const char *charset,
						 const char *name,
						 const char *x_mac_type,
						 const char *x_mac_creator,
						 void *stream_closure);

  /* How the MIME parser feeds its output (HTML or raw) back to the caller. */
  int (*output_fn) (char *buf, PRInt32 size, void *closure);

  /* Closure to pass to the above output_fn.  If NULL, then the
	 stream_closure is used. */
  void *output_closure;

  /* A hook for the caller to perform charset-conversion before HTML is
	 returned.  Each set of characters which originated in a mail message
	 (body or headers) will be run through this filter before being converted
	 into HTML.  (This should return bytes which may appear in an HTML file,
	 ie, we must be able to scan through the string to search for "<" and
	 turn it in to "&lt;", and so on.)

	 `input' is a non-NULL-terminated string of a single line from the message.
	 `input_length' is how long it is.
	 `input_charset' is a string representing the charset of this string (as
	   specified by MIME headers.)
	 `output_charset' is the charset to which conversion is desired.
	 `output_ret' is where a newly-malloced string is returned.  It may be
	   NULL if no translation is needed.
	 `output_size_ret' is how long the returned string is (it need not be
	   NULL-terminated.).
   */
  int (*charset_conversion_fn) (const PRBool input_autodetect, const char *input_line, 
                PRInt32 input_length, const char *input_charset,
								const char *output_charset,
								char **output_ret, PRInt32 *output_size_ret,
								void *stream_closure, nsIUnicodeDecoder *decoder, nsIUnicodeEncoder *encoder);

  /* If PR_TRUE, perform both charset-conversion and decoding of
	 MIME-2 header fields (using RFC-1522 encoding.)
   */
  PRBool rfc1522_conversion_p;

  /* A hook for the caller to turn a file name into a content-type. */
  char *(*file_type_fn) (const char *filename, void *stream_closure);

  /* A hook by which the user may be prompted for a password by the security
	 library.  (This is really of type `SECKEYGetPasswordKey'; see sec.h.) */
  void *(*passwd_prompt_fn)(void *arg1, void *arg2);

  /* =======================================================================
	 Various callbacks; for all of these functions, the `closure' argument
	 is what is found in `html_closure'.
   */
  void *html_closure;

  /* For emitting some HTML before the start of the outermost message
	 (this is called before any HTML is written to layout.) */
  MimeHTMLGeneratorFunction generate_header_html_fn;

  /* For emitting some HTML after the outermost header block, but before
	 the body of the first message. */
  MimeHTMLGeneratorFunction generate_post_header_html_fn;

  /* For emitting some HTML at the very end (this is called after libmime
	 has written everything it's going to write.) */
  MimeHTMLGeneratorFunction generate_footer_html_fn;

  /* For turning a message ID into a loadable URL. */
  MimeHTMLGeneratorFunction generate_reference_url_fn;

  /* For turning a mail address into a mailto URL. */
  MimeHTMLGeneratorFunction generate_mailto_url_fn;

  /* For turning a newsgroup name into a news URL. */
  MimeHTMLGeneratorFunction generate_news_url_fn;

  /* =======================================================================
     Callbacks to handle the backend-specific inlined image display
	 (internal-external-reconnect junk.)  For `image_begin', the `closure'
	 argument is what is found in `stream_closure'; but for all of the
	 others, the `closure' argument is the data that `image_begin' returned.
   */

  /* Begins processing an embedded image; the URL and content_type are of the
	 image itself. */
  void *(*image_begin) (const char *image_url, const char *content_type,
						void *stream_closure);

  /* Stop processing an image. */
  void (*image_end) (void *image_closure, int status);

  /* Dump some raw image data down the stream. */
  int (*image_write_buffer) (char *buf, PRInt32 size, void *image_closure);

  /* What HTML should be dumped out for this image. */
  char *(*make_image_html) (void *image_closure);


  /* =======================================================================
	 Other random opaque state.
   */
  MimeParseStateObject *state;		/* Some state used by libmime internals;
									   initialize this to 0 and leave it alone.
									 */


#ifdef MIME_DRAFTS
  /* =======================================================================
	Mail Draft hooks -- 09-19-1996
   */
  PRBool decompose_file_p;            /* are we decomposing a mime msg 
									   into separate files */
  PRBool done_parsing_outer_headers;  /* are we done parsing the outer message
										  headers; this is really useful when
										  we have multiple Message/RFC822 
										  headers */
  PRBool is_multipart_msg;            /* are we decomposing a multipart
										  message */

  int decompose_init_count;            /* used for non multipart message only
										*/

  PRBool signed_p;					   /* to tell draft this is a signed
										  message */

  PRBool caller_need_root_headers;    /* set it to true to receive the message main
                                         headers through the callback
                                         decompose_headers_info_fn */

  /* Callback to gather the outer most headers so we could use the 
	 information to initialize the addressing/subject/newsgroups fields
	 for the composition window. */
  int (*decompose_headers_info_fn) (void *closure, 
									MimeHeaders *headers);

  /* Callbacks to create temporary files for drafts attachments. */
  nsresult (*decompose_file_init_fn) (void *stream_closure, 
								 MimeHeaders *headers );

  nsresult (*decompose_file_output_fn) (char *buf, PRInt32 size,
								   void *stream_closure);

  nsresult (*decompose_file_close_fn) (void *stream_closure);
#endif /* MIME_DRAFTS */

  PRInt32 attachment_icon_layer_id; /* Hackhackhack.  This is zero if we have
									 not yet emitted the attachment layer
									 stuff.  If we have, then this is the
									 id number for that layer, which is a
									 unique random number every time, to keep
									 evil people from writing javascript code
									 to hack it. */

  PRBool missing_parts;	/* Whether or not this message is going to contain
							   missing parts (from IMAP Mime Parts On Demand) */

};

#endif /* _MODLMIME_H_ */
