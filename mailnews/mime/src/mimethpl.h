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
 * The Original Code is mozilla.org libmime code.
 *
 * The Initial Developer of the Original Code is
 * Ben Bucksch <http://www.bucksch.org/1/projects/mozilla/>.
 * Portions created by the Initial Developer are Copyright (C) 2002
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

/* The MimeInlineTextHTMLAsPlaintext class converts HTML->TXT->HTML, i.e.
   HTML to Plaintext and the result to HTML again.
   This might sound crazy, maybe it is, but it is for the "View as Plaintext"
   option, if the sender didn't supply a plaintext alternative (bah!).
 */

#ifndef _MIMETHPL_H_
#define _MIMETHPL_H_

#include "mimetpla.h"
#include "nsString.h"
#include "nsReadableUtils.h"


typedef struct MimeInlineTextHTMLAsPlaintextClass MimeInlineTextHTMLAsPlaintextClass;
typedef struct MimeInlineTextHTMLAsPlaintext      MimeInlineTextHTMLAsPlaintext;

struct MimeInlineTextHTMLAsPlaintextClass {
  MimeInlineTextPlainClass plaintext;
};

extern MimeInlineTextHTMLAsPlaintextClass mimeInlineTextHTMLAsPlaintextClass;

struct MimeInlineTextHTMLAsPlaintext {
  MimeInlineTextPlain  plaintext;
  nsString             *complete_buffer;  // Gecko parser expects wide strings
};

#endif /* _MIMETHPL_H_ */
