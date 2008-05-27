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
 * Christopher Blizzard.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Christopher Blizzard <blizzard@mozilla.org>
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

#ifndef gtkmozembed_internal_h
#define gtkmozembed_internal_h

#include "nsIWebBrowser.h"
#include "nsXPCOM.h"

struct nsModuleComponentInfo;

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

GTKMOZEMBED_API(void,
  gtk_moz_embed_get_nsIWebBrowser, (GtkMozEmbed *embed,
                                    nsIWebBrowser **retval))
GTKMOZEMBED_API(PRUnichar*,
  gtk_moz_embed_get_title_unichar, (GtkMozEmbed *embed))

GTKMOZEMBED_API(PRUnichar*,
  gtk_moz_embed_get_js_status_unichar, (GtkMozEmbed *embed))

GTKMOZEMBED_API(PRUnichar*,
  gtk_moz_embed_get_link_message_unichar, (GtkMozEmbed *embed))

GTKMOZEMBED_API(void,
  gtk_moz_embed_set_directory_service_provider, (nsIDirectoryServiceProvider *appFileLocProvider))

GTKMOZEMBED_API(void,
  gtk_moz_embed_set_app_components, (const nsModuleComponentInfo *aComps,
                                     int aNumComps))

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* gtkmozembed_internal_h */

