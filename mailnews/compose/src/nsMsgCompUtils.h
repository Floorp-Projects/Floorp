/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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

#ifndef _nsMsgCompUtils_H_
#define _nsMsgCompUtils_H_

#include "nscore.h"
#include "nsMsgSend.h"
#include "nsFileSpec.h"
#include "nsMsgCompFields.h"
#include "nsIMsgSend.h"

class nsIPrompt; 

#define ANY_SERVER "anyfolder://"

NS_BEGIN_EXTERN_C

//
// Create a file spec or file name using the name passed
// in as a template
//
nsFileSpec  *nsMsgCreateTempFileSpec(const char *tFileName);
char        *nsMsgCreateTempFileName(const char *tFileName);


//
// Various utilities for building parts of MIME encoded 
// messages during message composition
//
nsresult    mime_sanity_check_fields (
                            const char *from,
                            const char *reply_to,
                            const char *to,
                            const char *cc,
                            const char *bcc,
                            const char *fcc,
                            const char *newsgroups,
                            const char *followup_to,
                            const char * /*subject*/,
                            const char * /*references*/,
                            const char * /*organization*/,
                            const char * /*other_random_headers*/);

char        *mime_generate_headers (nsMsgCompFields *fields,
                                    const char *charset,
                                    nsMsgDeliverMode deliver_mode, nsIPrompt * aPrompt, PRInt32 *status);

char        *mime_make_separator(const char *prefix);
char        *mime_gen_content_id(PRUint32 aPartNum, const char *aEmailAddress);

char        *mime_generate_attachment_headers (const char *type,
                           const char *encoding,
                           const char *description,
                           const char *x_mac_type,
                           const char *x_mac_creator,
                           const char *real_name,
                           const char *base_url,
                           PRBool digest_p,
                           nsMsgAttachmentHandler *ma,
                           const char *charset,
                           const char *content_id,
                           PRBool     aBodyDocument);

char        *msg_generate_message_id (nsIMsgIdentity*);

char        *RFC2231ParmFolding(const char *parmName, const char *charset, 
                                const char *language, const char *parmValue);

PRBool      mime_7bit_data_p (const char *string, PRUint32 size);

char        *mime_fix_header_1 (const char *string, PRBool addr_p, PRBool news_p);
char        *mime_fix_header (const char *string);
char        *mime_fix_addr_header (const char *string);
char        *mime_fix_news_header (const char *string);

PRBool      mime_type_requires_b64_p (const char *type);
PRBool      mime_type_needs_charset (const char *type);

char        *msg_make_filename_qtext(const char *srcText, PRBool stripCRLFs);

// Rip apart the URL and extract a reasonable value for the `real_name' slot.
void        msg_pick_real_name (nsMsgAttachmentHandler *attachment, const PRUnichar *proposedName, const char *charset);

//
// Informational calls...
//
void        nsMsgMIMESetConformToStandard (PRBool conform_p);
PRBool      nsMsgMIMEGetConformToStandard (void);

//
// network service type calls...
//
nsresult    nsMsgNewURL(nsIURI** aInstancePtrResult, const char * aSpec);
PRBool      nsMsgIsLocalFile(const char *url);
char        *nsMsgGetLocalFileFromURL(const char *url);
char        *nsMsgPlatformFileToURL (nsFileSpec aFileSpec);

char        *nsMsgParseURLHost(const char *url);

char        *GenerateFileNameFromURI(nsIURI *aURL);

char        *GetFolderNameFromURLString(char *aUrl);
char        *nsMsgParseSubjectFromFile(nsFileSpec* fileSpec); 

//
// Folder calls...
//
char        *GetFolderURIFromUserPrefs(nsMsgDeliverMode   aMode,
                                       nsIMsgIdentity *identity);
                                       
// File calls...
nsresult ConvertBufToPlainText(nsString &aConBuf, PRBool formatflowed = PR_FALSE);

// Check if we should use format=flowed 
PRBool UseFormatFlowed(const char *charset);


NS_END_EXTERN_C


#endif /* _nsMsgCompUtils_H_ */

