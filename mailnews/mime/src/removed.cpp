mimemoz.cpp
===========
references to pane have been removed

  //SHERRY - NO REAL PANE HERE!!!
  //msd->options->pane = url->msg_pane;


mimei.h
=======
MSG_Pane      pane;



MimeDisplayOptions in modlmime.h:
=================================
  MSG_Pane* pane;				/* The libmsg pane object that corresponds to
								           what we're showing.  This is used by very
								           little... */

mimehdrs.cpp
============
  PR_ASSERT(hdrs);
  if (!hdrs) return -1;


  downloaded = opt->missing_parts ? (MimeHeaders_get(hdrs, IMAP_EXTERNAL_CONTENT_HEADER, PR_FALSE, PR_FALSE) == NULL) : PR_TRUE;

  if (type && *type && opt)
	{
	  if (opt->type_icon_name_fn)
		icon = opt->type_icon_name_fn(type, opt->stream_closure);

	  if (!PL_strcasecmp(type, APPLICATION_OCTET_STREAM))
		type_desc = PL_strdup(XP_GetString(MK_MSG_UNSPECIFIED_TYPE));
	  else if (opt->type_description_fn)
		type_desc = opt->type_description_fn(type, opt->stream_closure);
	}

  /* Kludge to use a prettier name for AppleDouble and AppleSingle objects.
   */
  if (type && (!PL_strcasecmp(type, MULTIPART_APPLEDOUBLE) ||
			   !PL_strcasecmp(type, MULTIPART_HEADER_SET) ||
			   !PL_strcasecmp(type, APPLICATION_APPLEFILE)))
	{
	  PR_Free(type);
	  type = PL_strdup(XP_GetString(MK_MSG_MIME_MAC_FILE));
	  PR_FREEIF(icon);
	  icon = PL_strdup("internal-gopher-binary");
	}

# define PUT_STRING(S) do { \
		 status = MimeHeaders_grow_obuffer(hdrs, PL_strlen(S)+1); \
		 if (status < 0) goto FAIL; \
		 PL_strcpy (hdrs->obuffer, S); \
		 status = MimeHeaders_write(opt, hdrs->obuffer, \
									PL_strlen(hdrs->obuffer)); \
		 if (status < 0) goto FAIL; } while (0)

  PUT_STRING ("<TABLE CELLPADDING=2 CELLSPACING=0 BORDER=1><TR>");
  if (MimeHeaders_getShowAttachmentColors())
  {
      /* For IMAP MIME parts on demand */
	  partColor = downloaded ? "#00CC00" : "#FFCC00";

	  PUT_STRING ("<TD NOWRAP WIDTH=\"5\" BGCOLOR=\"");
	  PUT_STRING (partColor);
	  PUT_STRING ("\"><TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>");
	  PUT_STRING ("<TR><TD NOWRAP VALIGN=CENTER></TD></TR></TABLE></TD>");
  }
  PUT_STRING ("<TD NOWRAP>");

  if (icon)
	{
	  PUT_STRING("<TABLE BORDER=0 CELLPADDING=0 CELLSPACING=0>"
				 "<TR><TD NOWRAP VALIGN=CENTER>");

	  if (lname_url)
		{
		  PUT_STRING("<A HREF=\"");
		  PUT_STRING(lname_url);
		  PUT_STRING("\" PRIVATE>");
		}
	  PUT_STRING("<IMG SRC=\"");
	  PUT_STRING(icon);
	  PUT_STRING("\" BORDER=0 ALIGN=MIDDLE ALT=\"\">");

	  if (lname_url)
		PUT_STRING("</A>");

	  PUT_STRING("</TD><TD VALIGN=CENTER>");
	}

  if (lname_url)
	{
	  PUT_STRING("<A HREF=\"");
	  PUT_STRING(lname_url);
	  PUT_STRING("\" PRIVATE>");
	}
  if (lname)
	PUT_STRING (lname);
  if (lname_url)
	PUT_STRING("</A>");

  if (icon)
	PUT_STRING("</TD></TR></TABLE>");

  PUT_STRING ("</TD><TD>");

  if (opt->headers == MimeHeadersAll)
	{
	  status = MimeHeaders_write_headers_html (hdrs, opt, TRUE);
	  if (status < 0) return status;
	}
  else
	{
#define MHTML_HEADER_TABLE       "<TABLE CELLPADDING=0 CELLSPACING=0 BORDER=0>"

	  char *name = MimeHeaders_get_name(hdrs);
	  PUT_STRING (MHTML_HEADER_TABLE);

	  if (name)
		{
		  const char *name_hdr = MimeHeaders_localize_header_name("Name", opt);
		  PUT_STRING("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>");
		  PUT_STRING(name_hdr);
		  PUT_STRING(": </TH><TD>");
		  PUT_STRING(name);
		  PR_FREEIF(name);
		  PUT_STRING("</TD></TR>");
		}

	  if (type)
		{
		  const char *type_hdr = MimeHeaders_localize_header_name("Type", opt);
		  PUT_STRING("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>");
		  PUT_STRING(type_hdr);
		  PUT_STRING(": </TH><TD>");
		  if (type_desc)
			{
			  PUT_STRING(type_desc);
			  PUT_STRING(" (");
			}
		  PUT_STRING(type);
		  if (type_desc)
			PUT_STRING(")");
		  PR_FREEIF(type);
		  PR_FREEIF(type_desc);
		  PUT_STRING("</TD></TR>");
		}

	  enc = (encoding
			 ? PL_strdup(encoding)
			 : MimeHeaders_get(hdrs, HEADER_CONTENT_TRANSFER_ENCODING,
							   PR_TRUE, PR_FALSE));
	  if (enc)
		{
		  const char *enc_hdr = MimeHeaders_localize_header_name("Encoding",
																 opt);
		  PUT_STRING("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>");
		  PUT_STRING(enc_hdr);
		  PUT_STRING(": </TH><TD>");
		  PUT_STRING(enc);
		  PR_FREEIF(enc);
		  PUT_STRING("</TD></TR>");
		}

	  desc = MimeHeaders_get(hdrs, HEADER_CONTENT_DESCRIPTION, PR_FALSE, PR_FALSE);
	  if (!desc)
		{
		  desc = MimeHeaders_get(hdrs, HEADER_X_SUN_DATA_DESCRIPTION,
								 PR_FALSE, PR_FALSE);

		  /* If there's an X-Sun-Data-Description, but it's the same as the
			 X-Sun-Data-Type, don't show it.
		   */
		  if (desc)
			{
			  char *loser = MimeHeaders_get(hdrs, HEADER_X_SUN_DATA_TYPE,
											PR_FALSE, PR_FALSE);
			  if (loser && !PL_strcasecmp(loser, desc))
				PR_FREEIF(desc);
			  PR_FREEIF(loser);
			}
		}

	  if (desc)
		{
		  const char *desc_hdr= MimeHeaders_localize_header_name("Description",
																 opt);
		  PUT_STRING("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>");
		  PUT_STRING(desc_hdr);
		  PUT_STRING(": </TH><TD>");
		  PUT_STRING(desc);
		  PR_FREEIF(desc);
		  PUT_STRING("</TD></TR>");
		}

	  if (!downloaded)
	    {
		  const char *downloadStatus_hdr = XP_GetString(MK_MIMEHTML_DOWNLOAD_STATUS_HEADER);
		  const char *downloadStatus = XP_GetString(MK_MIMEHTML_DOWNLOAD_STATUS_NOT_DOWNLOADED);
		  PUT_STRING("<TR><TH VALIGN=BASELINE ALIGN=RIGHT NOWRAP>");
		  PUT_STRING(downloadStatus_hdr);
		  PUT_STRING(": </TH><TD>");
		  PUT_STRING(downloadStatus);
		  PUT_STRING("</TD></TR>");		  
	    }

	  PUT_STRING ("</TABLE>");
	}
  if (body) PUT_STRING(body);
  PUT_STRING ("</TD></TR></TABLE></CENTER>");
# undef PUT_STRING

 FAIL:
  PR_FREEIF(type);
  PR_FREEIF(desc);
  PR_FREEIF(enc);
  PR_FREEIF(icon);
  PR_FREEIF(type_desc);
  MimeHeaders_compact (hdrs);
  return status;





/* Returns PR_TRUE if we should show colored tags on attachments.
   Useful for IMAP MIME parts on demand, because it shows a different
   color for undownloaded parts. */
static PRBool
MimeHeaders_getShowAttachmentColors()
{
	static int32 gotPref = PR_FALSE;
	static int32 showColors = PR_FALSE;
	if (!gotPref)
	{
		PREF_GetIntPref("mailnews.color_tag_attachments", &showColors);
		gotPref = PR_TRUE;
	}
	return showColors;
}

