/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * ***** END LICENSE BLOCK *****
 * This Original Code has been modified by IBM Corporation. Modifications made by IBM 
 * described herein are Copyright (c) International Business Machines Corporation, 2000.
 * Modifications to Mozilla code or documentation identified per MPL Section 3.3
 *
 * Date             Modified by     Description of modification
 * 04/20/2000       IBM Corp.      OS/2 VisualAge build.
 */
 
/*
 * This interface is implemented by libmime. This interface is used by 
 * a Content-Type handler "Plug In" (i.e. vCard) for accessing various 
 * internal information about the object class system of libmime. When 
 * libmime progresses to a C++ object class, this would probably change.
 */
#ifndef nsMimeConverter_h_
#define nsMimeConverter_h_

#include "nsISupports.h"
#include "prtypes.h"
#include "nsIMimeConverter.h"
#include "nsICharsetConverterManager.h"
#include "nsCOMPtr.h"

class nsMimeConverter : public nsIMimeConverter {
public: 
  nsMimeConverter();
  virtual ~nsMimeConverter();
       
  /* this macro defines QueryInterface, AddRef and Release for this class */
  NS_DECL_ISUPPORTS 

  // These methods are all implemented by libmime to be used by 
  // content type handler plugins for processing stream data. 

  // Decode routine
  NS_IMETHOD DecodeMimeHeader(const char *header, 
                              char **decodedString,
                              const char *default_charset = 0,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE);

  // Decode routine (also converts output to unicode)
  // On success, decodedString is never null
  NS_IMETHOD DecodeMimeHeader(const char *header,
                              PRUnichar **decodedString,
                              const char *default_charset = 0,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE);

  // Decode routine (also converts output to unicode)
  // On success, decodedString is never null
  NS_IMETHOD DecodeMimeHeader(const char *header, 
                              nsAString& decodedString,
                              const char *default_charset = nsnull,
                              PRBool override_charset = PR_FALSE,
                              PRBool eatContinuations = PR_TRUE);

  // Encode routine
  NS_IMETHOD EncodeMimePartIIStr(const char    *header, 
                                 PRBool        structured, 
                                 const char    *mailCharset, 
                                 PRInt32       fieldnamelen,
                                 PRInt32       encodedWordSize, 
                                 char          **encodedString);

  // Encode routine (utf-8 input)
  NS_IMETHOD EncodeMimePartIIStr_UTF8(const char    *header, 
                                      PRBool        structured, 
                                      const char    *mailCharset, 
                                      PRInt32       fieldnamelen,
                                      PRInt32       encodedWordSize, 
                                      char          **encodedString);

  NS_IMETHOD B64EncoderInit(nsresult (*PR_CALLBACK output_fn) (const char *buf, PRInt32 size, void *closure), 
                                void *closure, MimeEncoderData **returnEncoderData);

  NS_IMETHOD QPEncoderInit (nsresult (*PR_CALLBACK output_fn) (const char *buf, 
                                PRInt32 size, void *closure), void *closure, 
                                MimeEncoderData ** returnEncoderData);

  NS_IMETHOD UUEncoderInit (char *filename, nsresult (*PR_CALLBACK output_fn) 
                               (const char *buf, PRInt32 size, void *closure), void *closure, 
                               MimeEncoderData ** returnEncoderData);

  NS_IMETHOD EncoderDestroy(MimeEncoderData *data, PRBool abort_p);

  NS_IMETHOD EncoderWrite (MimeEncoderData *data, const char *buffer, PRInt32 size, PRInt32 *written);

}; 

#endif /* nsMimeConverter_h_ */
