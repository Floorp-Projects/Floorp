/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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
 
/*
 * This interface is implemented by content type handlers that will be
 * called upon by libmime to process various attachments types. The primary
 * purpose of these handlers will be to represent the attached data in a
 * viewable HTML format that is useful for the user
 *
 * Note: These will all register by their content type prefixed by the
 *       following:  mimecth:text/vcard
 * 
 *       libmime will then use the XPCOM Component Manager to 
 *       locate the appropriate Content Type handler
 */
#ifndef nsIMimeContentTypeHandler_h_
#define nsIMimeContentTypeHandler_h_

typedef struct {
  PRBool      force_inline_display;
} contentTypeHandlerInitStruct;

#include "prtypes.h"
#include "nsISupports.h"
#include "mimecth.h"

// {20DABD99-F8B5-11d2-8EE0-00A024A7D144}
#define NS_IMIME_CONTENT_TYPE_HANDLER_IID \
      { 0x20dabd99, 0xf8b5, 0x11d2,   \
      { 0x8e, 0xe0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

// {20DABDA1-F8B5-11d2-8EE0-00A024A7D144}
#define NS_VCARD_CONTENT_TYPE_HANDLER_CID \
      { 0x20dabda1, 0xf8b5, 0x11d2, \
      { 0x8e, 0xe0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_CALENDAR_CONTENT_TYPE_HANDLER_CID \
      { 0x20dabdac, 0xf8b5, 0x11d2, \
      { 0x0b, 0xe0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_SMIME_CONTENT_TYPE_HANDLER_CID \
      { 0x20dabdac, 0xf8b5, 0x11d2, \
      { 0xFF, 0xe0, 0x0, 0xa0, 0x24, 0xa7, 0xd1, 0x44 } }

#define NS_SIGNED_CONTENT_TYPE_HANDLER_CID \
      { 0x20dabdac, 0xf8b5, 0x11d2, \
      { 0xFF, 0xe0, 0x0, 0xaf, 0x19, 0xa7, 0xd1, 0x44 } }

class nsIMimeContentTypeHandler : public nsISupports {
public: 
  NS_DEFINE_STATIC_IID_ACCESSOR(NS_IMIME_CONTENT_TYPE_HANDLER_IID)

  NS_IMETHOD    GetContentType(char **contentType) = 0;

  NS_IMETHOD    CreateContentTypeHandlerClass(const char *content_type, 
                                              contentTypeHandlerInitStruct *initStruct, 
                                              MimeObjectClass **objClass) = 0;
}; 

#endif /* nsIMimeContentTypeHandler_h_ */
