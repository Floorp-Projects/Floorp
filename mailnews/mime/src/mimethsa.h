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

/* The MimeInlineTextHTMLSanitized class cleans up HTML

   This class pushes the HTML that we get from the
   sender of the message through a sanitizer (mozISanitizingHTMLSerializer),
   which lets only allowed tags through. With the appropriate configuration,
   this protects from most of the security and visual-formatting problems
   that otherwise usually come with HTML (and which partly gave HTML in email
   the bad reputation that it has).

   However, due to the parsing and serializing (and later parsing again)
   required, there is an inherent, significant performance hit, when doing the
   santinizing here at the MIME / HTML source level. But users of this class
   will most likely find it worth the cost. See also the comment in
   mozISanitizingHTMLSerializer.h.
 */

#ifndef _MIMETHSA_H_
#define _MIMETHSA_H_

#include "mimethtm.h"
#include "nsString.h"
#include "nsReadableUtils.h"

typedef struct MimeInlineTextHTMLSanitizedClass MimeInlineTextHTMLSanitizedClass;
typedef struct MimeInlineTextHTMLSanitized      MimeInlineTextHTMLSanitized;

struct MimeInlineTextHTMLSanitizedClass {
  MimeInlineTextHTMLClass html;
};

extern MimeInlineTextHTMLSanitizedClass mimeInlineTextHTMLSanitizedClass;

struct MimeInlineTextHTMLSanitized {
  MimeInlineTextHTML    html;
  nsString             *complete_buffer;  // Gecko parser expects wide strings
};

#endif /* _MIMETHPL_H_ */
