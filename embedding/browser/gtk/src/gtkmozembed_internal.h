/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Christopher Blizzard.
 * Portions created by Christopher Blizzard are Copyright (C)
 * Christopher Blizzard.  All Rights Reserved.
 * 
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
 */

#ifndef gtkmozembed_internal_h
#define gtkmozembed_internal_h

#include <nsIWebBrowser.h>

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

extern void  gtk_moz_embed_get_nsIWebBrowser  (GtkMozEmbed *embed, nsIWebBrowser **retval);
extern PRUnichar *gtk_moz_embed_get_title_unichar (GtkMozEmbed *embed);
extern PRUnichar *gtk_moz_embed_get_js_status_unichar (GtkMozEmbed *embed);
extern PRUnichar *gtk_moz_embed_get_link_message_unichar (GtkMozEmbed *embed);


#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* gtkmozembed_internal_h */

