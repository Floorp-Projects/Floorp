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

#ifndef _MIMECMS_H_
#define _MIMECMS_H_

#include "mimecryp.h"
#include "nsICMS.h"

/* The MimeEncryptedCMS class implements a type of MIME object where the
   object is passed through a CMS decryption engine to decrypt or verify
   signatures.  That module returns a new MIME object, which is then presented
   to the user.  See mimecryp.h for details of the general mechanism on which
   this is built.
 */

typedef struct MimeEncryptedCMSClass MimeEncryptedCMSClass;
typedef struct MimeEncryptedCMS      MimeEncryptedCMS;

struct MimeEncryptedCMSClass {
  MimeEncryptedClass encrypted;

  /* Callback used to access the SEC_PKCS7ContentInfo of this object. */
  void (*get_content_info) (MimeObject *self,
							nsICMSMessage **content_info_ret,
							char **sender_email_addr_return,
							PRInt32 *decode_error_ret,
              PRInt32 *verify_error_ret,
              PRBool * ci_is_encrypted);
};

extern MimeEncryptedCMSClass mimeEncryptedCMSClass;

struct MimeEncryptedCMS {
  MimeEncrypted encrypted;		/* superclass variables */
};

#endif /* _MIMEPKCS_H_ */
