/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "rosetta_mailnews.h"
#include "nsMsgSend.h"
#include "nsMsgSendPart.h"
#include "nsIPref.h"
#include "nsMsgCompPrefs.h"
#include "nsMsgCreate.h"

static NS_DEFINE_CID(kPrefCID, NS_PREF_CID);

#if 0
static void
msg_delete_attached_files(struct nsMsgAttachedFile *attachments)
{
	struct nsMsgAttachedFile *tmp;
	if (!attachments) return;
	for (tmp = attachments; tmp->orig_url; tmp++) {
		PR_FREEIF(tmp->orig_url);
		PR_FREEIF(tmp->type);
		PR_FREEIF(tmp->real_name);
		PR_FREEIF(tmp->encoding);
		PR_FREEIF(tmp->description);
		PR_FREEIF(tmp->x_mac_type);
		PR_FREEIF(tmp->x_mac_creator);
		if (tmp->file_spec) 
			delete tmp->file_spec;
	}
	PR_FREEIF(attachments);
}
#endif /* 0 */

/**************

PRInt32
CreateVcardAttachment()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	nsMsgCompPrefs pCompPrefs;
	char* name;
	int status = 0;

	if (!m_haveAttachedVcard && AB_UserHasVCard() ) // don't attach a vcard if the user does not have a vcard
	{

		char * vCard = NULL;
		char * filename = NULL;
		AB_LoadIdentityVCard(&vCard);
		if (vCard)
		{
			AB_ExportVCardToTempFile (vCard, &filename);
			if (vCard)
				PR_Free(vCard); // free our allocated VCardString...
			char buf [ 2 * kMaxFullNameLength ];
			if (pCompPrefs.GetUserFullName()) 
				name = PL_strdup (pCompPrefs.GetUserFullName());
			// write out a content description string
#ifdef UNREADY_CODE
			PR_snprintf(buf, sizeof(buf), XP_GetString (NS_ADDR_BOOK_CARD), name);
#endif
			PR_FREEIF(name);


			char* temp = WH_FileName(filename, xpFileToPost);
			char * fileurl = NULL;
			if (temp)
			{
				fileurl = XP_PlatformFileToURL (temp);
				PR_Free(temp);
			}
			else
				return -1;	

			// Send the vCard out with a filename which distinguishes this user. e.g. jsmith.vcf
			// The main reason to do this is for interop with Eudora, which saves off 
			// the attachments separately from the message body
			char *vCardFileName = NULL;
			char *mailIdentityUserEmail = NULL;
			char *atSign = NULL;
      if (NS_SUCCEEDED(rv) && prefs)
  			prefs->CopyCharPref("mail.identity.useremail", &mailIdentityUserEmail);
			if (mailIdentityUserEmail)
			{
				atSign = PL_strchr(mailIdentityUserEmail, '@');
				if (atSign) *atSign = 0;
				vCardFileName = PR_smprintf ("%s.vcf", mailIdentityUserEmail);
				PR_Free(mailIdentityUserEmail);
			}
			if (!vCardFileName)
			{
				vCardFileName = PL_strdup("vcard.vcf");
				if (!vCardFileName)
					return NS_OUT_OF_MEMORY;
			}

			char * origurl = XP_PlatformFileToURL (vCardFileName);
			int datacount = 0, filecount = 0;
			for (nsMsgAttachmentData *tmp1 = m_attachData; tmp1 && tmp1->url; tmp1++) datacount++;
			for (nsMsgAttachedFile *tmp = m_attachedFiles; tmp && tmp->orig_url; tmp++) filecount++;

			nsMsgAttachmentData *alist;
			if (datacount) {
				alist = (nsMsgAttachmentData *)
				PR_REALLOC(m_attachData, (datacount + 2) * sizeof(nsMsgAttachmentData));
			}
			else {
				alist = (nsMsgAttachmentData *)
					PR_Malloc((datacount + 2) * sizeof(nsMsgAttachmentData));
			}
			if (!alist)
				return NS_OUT_OF_MEMORY;
			m_attachData = alist;
			memset (m_attachData + datacount, 0, 2 * sizeof (nsMsgAttachmentData));
			m_attachData[datacount].url = fileurl;
			m_attachData[datacount].real_type = PL_strdup(vCardMimeFormat);
			m_attachData[datacount].description = PL_strdup (buf);
			m_attachData[datacount].real_name = PL_strdup (vCardFileName);
			m_attachData[datacount + 1].url = NULL;
			
			nsMsgAttachedFile *aflist;
			if (filecount) {
				aflist = (struct nsMsgAttachedFile *)
				PR_REALLOC(m_attachedFiles, (filecount + 2) * sizeof(nsMsgAttachedFile));
			}
			else {
				aflist = (struct nsMsgAttachedFile *)
					PR_Malloc((filecount + 2) * sizeof(nsMsgAttachedFile));
			}

			if (!aflist)
				return NS_OUT_OF_MEMORY;

			m_attachedFiles = aflist;
			memset (m_attachedFiles + filecount, 0, 2 * sizeof (nsMsgAttachedFile));
			m_attachedFiles[filecount].orig_url = origurl;
			m_attachedFiles[filecount].file_name = filename;
			m_attachedFiles[filecount].type = PL_strdup(vCardMimeFormat);
			m_attachedFiles[filecount].description = PL_strdup (buf);
			m_attachedFiles[filecount].real_name = PL_strdup (vCardFileName);
			m_attachedFiles[filecount + 1].orig_url = NULL;

			m_haveAttachedVcard = PR_TRUE;

			PR_Free(vCardFileName);
		}
	}
	return status;
}



char*
FigureBcc(PRBool newsBcc)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	nsMsgCompPrefs pCompPrefs;
	char* result = NULL;
	PRBool useBcc, mailBccSelf, newsBccSelf = PR_FALSE;

	if (newsBcc)
  {
    if (NS_SUCCEEDED(rv) && prefs)
    {
  		prefs->GetBoolPref("news.use_default_cc", &useBcc);
    }
  }
	else
  {
    if (NS_SUCCEEDED(rv) && prefs)
    {
  		prefs->GetBoolPref("mail.use_default_cc", &useBcc);
    }
  }
		
  prefs->GetBoolPref("mail.cc_self", &mailBccSelf);
  prefs->GetBoolPref("news.cc_self", &newsBccSelf);

	if (useBcc || mailBccSelf || newsBccSelf )
	{
		const char* tmp = useBcc ?
			GetPrefs()->GetDefaultHeaderContents(
				newsBcc ? MSG_NEWS_BCC_HEADER_MASK : MSG_BCC_HEADER_MASK) : NULL;

		if (! (mailBccSelf || newsBccSelf) ) 
    {
			result = PL_strdup(tmp ? tmp : "");
		} 
    else if (!tmp || !*tmp) 
    {
			result = PL_strdup(pCompPrefs.GetUserEmail());
		} 
    else 
    {
			result = PR_smprintf("%s, %s", pCompPrefs.GetUserEmail(), tmp);
		}
	}
	return result;
}

void
InitializeHeaders(MWContext* old_context, const nsIMsgCompFields* fields)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 
	nsMsgCompPrefs pCompPrefs;

	PR_ASSERT(m_fields == NULL);
	PR_ASSERT(m_initfields == NULL);

	const char *real_addr = pCompPrefs.GetUserEmail();
	char *real_return_address;
	const char* sig;
	PRBool forward_quoted;
	forward_quoted = PR_FALSE;

	m_fields = new nsMsgCompFields;
	if (!m_fields)
		return;
	m_fields->AddRef();
	if (fields)
		m_fields->Copy((nsIMsgCompFields*)fields);
	m_fields->SetOwner(this);

	m_oldContext = old_context;
	// hack for forward quoted.  Checks the attachment field for a cookie
	// string indicating that this is a forward quoted operation.  If a cookie
	// is found, the attachment string is slid back down over the cookie.  This
	// will put the original string back in tact. 

	const char* attachment = m_fields->GetAttachments();

	if (attachment && *attachment) {
		if (!PL_strncmp(attachment, MSG_FORWARD_COOKIE,
						PL_strlen(MSG_FORWARD_COOKIE))) {
			attachment += PL_strlen(MSG_FORWARD_COOKIE);
			forward_quoted = PR_TRUE;      // set forward with quote flag 
			m_fields->SetAttachments((char *)attachment, NULL);
			attachment = m_fields->GetAttachments();
		}
	}

	m_status = -1;

	if (MISC_ValidateReturnAddress(old_context, real_addr) < 0) {
		return;
	}

//JFD
//	real_return_address = MIME_MakeFromField(old_context->win_csid);
// real_return_address = (char *)pCompPrefs.GetUserEmail();

  PR_ASSERT (m_context->type == MWContextMessageComposition);
	PR_ASSERT (XP_FindContextOfType(0, MWContextMessageComposition));
	PR_ASSERT (!m_context->msg_cframe);


	PRInt32 count = m_fields->GetNumForwardURL();
	if (count > 0) {
		// if forwarding one or more messages
		PR_ASSERT(*attachment == '\0');
		nsMsgAttachmentData *alist = (struct nsMsgAttachmentData *)
			PR_Malloc((count + 1) * sizeof(nsMsgAttachmentData));
		if (alist) {
			memset(alist, 0, (count + 1) * sizeof(*alist));
			for (count--; count >= 0; count--) {
				alist[count].url = (char*) m_fields->GetForwardURL(count);
				alist[count].real_name = (char*) m_fields->GetForwardURL(count);
			}
			SetAttachmentList(alist);
			// Don't call msg_free_attachment_list because we are not duplicating
			// url & real_name
			PR_Free(alist);;
		}
	} else if (*attachment) {
		// forwarding a single url
		// typically a web page
		nsMsgAttachmentData *alist;
		count = 1;
		alist = (struct nsMsgAttachmentData *)
			PR_Malloc((count + 1) * sizeof(nsMsgAttachmentData));
		if (alist) {
			memset(alist, 0, (count + 1) * sizeof(*alist));
			alist[0].url = (char *)attachment;
			alist[0].real_name = (char *)attachment;
			SetAttachmentList(alist);
			// Don't call msg_free_attachment_list because we are not duplicating
			// url & real_name
			PR_Free(alist);
		}
	}	// else if (*attachment)

	if (*attachment) {
		if (*attachment != '(') {
			m_defaultUrl = PL_strdup(attachment);
		}
	}
	else if (old_context) {
		History_entry *h = SHIST_GetCurrent(&old_context->hist);
		if (h && h->address) {
			m_defaultUrl = PL_strdup(h->address);
		}
		if (m_defaultUrl)
		{
			MSG_Pane *msg_pane = MSG_FindPane(old_context,
											  MSG_MESSAGEPANE);
			if (msg_pane)
				m_fields->SetHTMLPart((char *)msg_pane->GetHTMLPart(), NULL);
		}
	}

	if (!*m_fields->GetFrom()) {
		m_fields->SetFrom(real_return_address, NULL);
	}

	// Guess what kind of reply this is based on the headers we passed in.
	 

	const char* newsgroups = m_fields->GetNewsgroups();
	const char* to = m_fields->GetTo();
	const char* cc = m_fields->GetCc();
	const char* references = m_fields->GetReferences();

	if (count > 0 || *attachment) {
		// if an attachment exists and the forward_quoted flag is set, this
		   is a forward quoted operation. 
		if (forward_quoted) {
			m_replyType = MSG_ForwardMessageQuoted;
			// clear out the attachment list for forward quoted messages. 
			SetAttachmentList(NULL);
			m_pendingAttachmentsCount = 0;
		} else {
			m_replyType = MSG_ForwardMessageAttachment;
		}
	} else if (*references && *newsgroups && (*to || *cc)) {
		m_replyType = MSG_PostAndMailReply;
	} else if (*references && *newsgroups) {
		m_replyType = MSG_PostReply;
	} else if (*references && *cc) {
		m_replyType = MSG_ReplyToAll;
	} else if (*references && *to) {
		m_replyType = MSG_ReplyToSender;
	} else if (*newsgroups) {
		m_replyType = MSG_PostNew;
	} else {
		m_replyType = MSG_MailNew;
	}


#ifdef UNREADY_CODE
	HJ77855
#endif

	if (!*m_fields->GetOrganization()) {
		m_fields->SetOrganization((char *)pCompPrefs.GetOrganization(), NULL);
	}

	if (!*m_fields->GetReplyTo()) {
		m_fields->
			SetReplyTo((char *)GetPrefs()->
					   GetDefaultHeaderContents(MSG_REPLY_TO_HEADER_MASK), NULL);
	}
	if (!*m_fields->GetFcc()) 
	{
		PRBool useDefaultFcc = PR_TRUE;
		// int prefError =   
    if (NS_SUCCEEDED(rv) && prefs)
    {
      prefs->GetBoolPref(*newsgroups ? "news.use_fcc" : "mail.use_fcc",
            &useDefaultFcc);
    }

		if (useDefaultFcc)
		{
			m_fields->SetFcc((char *)GetPrefs()->
				GetDefaultHeaderContents(*newsgroups ? 
					 MSG_NEWS_FCC_HEADER_MASK : MSG_FCC_HEADER_MASK), NULL);
		}
	}
	if (!*m_fields->GetBcc()) {
		char* bcc = FigureBcc(*newsgroups);
		m_fields->SetBcc(bcc, NULL);
		PR_FREEIF(bcc);
	}

	m_fields->SetFcc((char *)CheckForLosingFcc(m_fields->GetFcc()), NULL);

	{
		const char *body = m_fields->GetDefaultBody();
		if (body && *body)
		{
			m_fields->AppendBody((char *)body);
			m_fields->AppendBody(MSG_LINEBREAK);
			//  m_bodyEdited = PR_TRUE; 
		}
	}

	sig = FE_UsersSignature ();
	if (sig && *sig) {
	    m_fields->AppendBody(MSG_LINEBREAK);
		//If the sig doesn't begin with "--" followed by whitespace or a
		// newline, insert "-- \n" (the pseudo-standard sig delimiter.) 
		if (sig[0] != '-' || sig[1] != '-' ||
			(sig[2] != ' ' && sig[2] != CR && sig[2] != LF)) {
			m_fields->AppendBody("-- " MSG_LINEBREAK);
		}
		m_fields->AppendBody((char *)sig);
	}

	PR_FREEIF (real_return_address);


	if (m_context)
		FE_SetDocTitle(m_context, (char*) GetWindowTitle());


	m_initfields = new nsMsgCompFields;
	if (m_initfields) {
		m_initfields->AddRef();
		m_fields->Copy((nsIMsgCompFields*)m_fields);
		m_initfields->SetOwner(this);
	}
}

PRBool 
ShouldAutoQuote() {
      if (m_haveQuoted) return PR_FALSE;
	if (m_replyType == MSG_ForwardMessageQuoted ||
		    GetPrefs()->GetAutoQuoteReply()) {
		switch (m_replyType) {
		case MSG_ForwardMessageQuoted:
		case MSG_PostAndMailReply:
		case MSG_PostReply:
		case MSG_ReplyToAll:
		case MSG_ReplyToSender:
			return PR_TRUE;

        default:
            break;
		}
	}
	return PR_FALSE;
}


PRBool nsMsgCompose::SanityCheckNewsgroups (const char *newsgroups)
{
	// This function just does minor syntax checking on the names of newsgroup 
	// to make sure they conform to Son Of 1036: 
	// http://www.stud.ifi.uio.no/~larsi/notes/son-of-rfc1036.txt
	//
	// It isn't really possible to tell whether the group actually exists,
	// so we're not going to try.
	//
	// According to the URL given above, uppercase characters A-Z are not
	// allowed in newsgroup names, but when we tried to enforce that, we got
	// bug reports from people who were trying to post to groups with capital letters

	PRBool valid = PR_TRUE;
	if (newsgroups)
	{
		while (*newsgroups && valid)
		{
			if (!(*newsgroups >= 'a' && *newsgroups <= 'z') && !(*newsgroups >= 'A' && *newsgroups <= 'Z'))
			{
				if (!(*newsgroups >= '0' && *newsgroups <= '9'))
				{
					switch (*newsgroups)
					{
					case '+':
					case '-':
					case '/':
					case '_':
					case '=':
					case '?':
					case '.':
						break; // valid char
					case ' ':
					case ',':
						break; // ok to separate names in list
					default:
						valid = PR_FALSE;
					}
				}
			}
			newsgroups++;
		}
	}
	return valid;
}

int
MungeThroughRecipients(PRBool* someNonHTML,
											 PRBool* groupNonHTML)
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	PRBool foo;
	if (!someNonHTML) someNonHTML = &foo;
	if (!groupNonHTML) groupNonHTML = &foo;
	*someNonHTML = PR_FALSE;
	*groupNonHTML = PR_FALSE;
	int status = 0;
	char* names = NULL;
	char* addresses = NULL;
	const char* groups;
	char* name = NULL;
	char* end;
	PRBool match = PR_FALSE;
	m_host = NULL;				// Pure paranoia, in case we some day actually
								// have a UI that lets people change this.

	static PRInt32 masks[] = {
		MSG_TO_HEADER_MASK,
		MSG_CC_HEADER_MASK,
		MSG_BCC_HEADER_MASK
	};
	char* domainlist = NULL;

	delete m_htmlrecip;
	m_htmlrecip = new nsMsgHTMLRecipients();
	if (!m_htmlrecip) return NS_OUT_OF_MEMORY;

  PRUint32 i;
	for (i=0 ; i < sizeof(masks) / sizeof(masks[0]) ; i++) {
		const char* orig = m_fields->GetHeader(masks[i]);
		if (!orig || !*orig) continue;
		char* value = NULL;
		value = PL_strdup(orig);
		if (!value) {
			status = NS_OUT_OF_MEMORY;
			goto FAIL;
		}
		
		int num  = 0 // JFD = MSG_ParseRFC822Addresses(value, &names, &addresses);
		PR_Free(value);
		value = NULL;
		char* addr = NULL;
		for (int j=0 ; j<num ; j++) {
			if (addr) {
				addr = addr + PL_strlen(addr) + 1;
				name = name + PL_strlen(name) + 1;
			} else {
				addr = addresses;
				name = names;
			}
			if (!addr || !*addr) continue;

			// Need to check for a address book entry for this person and if so,
      // we have to see if this person can receive MHTML mail.
      match = AB_GetHTMLCapability(this, addr);

      char* at = PL_strchr(addr, '@');
			char* tmp = MSG_MakeFullAddress(name, addr);
			status = m_htmlrecip->AddOne(tmp, addr, Address, match);
			if (status < 0) goto FAIL;
			PR_Free(tmp);
			tmp = NULL;

			if (!at) {
				// ###tw We got to decide what to do in these cases.  But
				// for now, I'm just gonna ignore them.  Which is probably
				// exactly the wrong thing.  Fortunately, these cases are
				// now very rare, as we have code that inserts a default
				// domain.
				continue;
			}
			if (!domainlist) {
				if (NS_SUCCEEDED(rv) && prefs)
          prefs->CopyCharPref("mail.htmldomains", &domainlist);
			}
			char* domain = at + 1;
			for (;;) {
				char* dot = PL_strchr(domain, '.');
				if (!dot) break;
				PRInt32 domainlength = PL_strlen(domain);
				char* ptr;
				char* endptr = NULL;
				PRBool found = PR_FALSE;
				for (ptr = domainlist ; ptr && *ptr ; ptr = endptr) {
					endptr = PL_strchr(ptr, ',');
					int length;
					if (endptr) {
						length = endptr - ptr;
						endptr++;
					} else {
						length = PL_strlen(ptr);
					}
					if (length == domainlength) {
						if (PL_strncasecmp(domain, ptr, length) == 0) {
							found = PR_TRUE;
							match = PR_TRUE;
							break;
						}
					}
				}
#ifdef UNREADY_CODE
				char* tmp = PR_smprintf("%s@%s",
										XP_GetString(NS_MSG_EVERYONE),
										domain);
#endif
				if (!tmp) return NS_OUT_OF_MEMORY;
				status = m_htmlrecip->AddOne(domain, tmp, Domain, found);
				PR_Free(tmp);
				if (status < 0) goto FAIL;
				domain = dot + 1;
			}
			if (!match) *someNonHTML = PR_TRUE;
		}
	}

	groups = m_fields->GetHeader(MSG_NEWSGROUPS_HEADER_MASK);
	if (groups && *groups && !m_host)
	{
		m_host = InferNewsHost (groups);
		if (!m_host)
			goto FAIL;
	}

	end = NULL;
	for ( ; groups && *groups ; groups = end) {
		end = PL_strchr(groups, ',');
		if (end) *end = '\0';
		name = PL_strdup(groups);
		if (end) *end++ = ',';
		if (!name) {
			status = NS_OUT_OF_MEMORY;
			goto FAIL;
		}
		char* group = XP_StripLine(name);
		match = m_host->IsHTMLOKGroup(group);

		status = m_htmlrecip->AddOne(group, group, Newsgroup, match);
		if (status < 0) goto FAIL;
		char* tmp = PL_strdup(group);
		if (!tmp) {
			status = NS_OUT_OF_MEMORY;
			goto FAIL;
		}
		
		for (;;) {
			PRBool found = m_host->IsHTMLOKTree(tmp);

			char* desc = PR_smprintf("%s.*", tmp);
			if (!desc) {
				status = NS_OUT_OF_MEMORY;
				goto FAIL;
			}
			status = m_htmlrecip->AddOne(tmp, desc, GroupHierarchy, found);

			PR_Free(desc);
			if (status < 0) {
				PR_Free(tmp);
				tmp = NULL;
				goto FAIL;
			}
			if (found) match = PR_TRUE;

			char* p = PL_strrchr(tmp, '.');
			if (p) *p = '\0';
			else break;
		}
		PR_Free(tmp);
		tmp = NULL;
		if (!match) {
			*someNonHTML = PR_TRUE;
			*groupNonHTML = PR_TRUE;
		}
	}

 FAIL:
	PR_FREEIF(names);
	PR_FREEIF(domainlist);
	PR_FREEIF(addresses);
	PR_FREEIF(name);
	return status;
}



MSG_HTMLComposeAction
DetermineHTMLAction()
{
  nsresult rv;
  NS_WITH_SERVICE(nsIPref, prefs, kPrefCID, &rv); 

	PRBool someNonHTML, groupNonHTML;
	int status;

	MSG_HTMLComposeAction result = GetHTMLAction();


	if (result == MSG_HTMLAskUser) {
		// Well, before we ask, see if we can figure out what to do for
		// ourselves.

		status = MungeThroughRecipients(&someNonHTML, &groupNonHTML);
		if (status < 0) return MSG_HTMLAskUser; // ###
		if (!someNonHTML) return MSG_HTMLSendAsHTML;
		if (HasNoMarkup()) {
			// No point in sending this message as HTML; send it plain.
			return MSG_HTMLConvertToPlaintext;
		}
		// See if a preference has been set to tell us what to do.  Note that
		// we do not honor that preference for newsgroups, only for e-mail
		// addresses.
		if (!groupNonHTML) {
			PRInt32 value = 0;
      if (NS_SUCCEEDED(rv) && prefs)
        prefs->GetIntPref("mail.default_html_action", &value);
			if (value >= 0) {
				switch (value) {
				case 1:				// Force plaintext.
					return MSG_HTMLConvertToPlaintext;
				case 2:				// Force HTML.
					return MSG_HTMLSendAsHTML;
				case 3:				// Force multipart/alternative.
					return MSG_HTMLUseMultipartAlternative;
				}
			}
		}
	}
	return result;
}	
**/

