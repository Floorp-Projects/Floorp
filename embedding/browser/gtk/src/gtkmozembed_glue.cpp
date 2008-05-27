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
 * The Original Code is Mozilla embedding.
 *
 * The Initial Developer of the Original Code is
 * Benjamin Smedberg <benjamin@smedbergs.us>.
 *
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Mozilla Foundation <http://www.mozilla.org/>. All Rights Reserved.
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

// This file is an implementation file, meant to be #included in a
// single C++ file of an embedding application. It is called after
// XPCOMGlueStartup to glue the gtkmozembed functions.

#include "gtkmozembed.h"
#include "gtkmozembed_internal.h"
#include "nsXPCOMGlue.h"

#ifndef XPCOM_GLUE
#error This file only makes sense when XPCOM_GLUE is defined.
#endif

#define GTKMOZEMBED_FUNCTIONS \
  GTKF(gtk_moz_embed_get_type) \
  GTKF(gtk_moz_embed_new) \
  GTKF(gtk_moz_embed_push_startup) \
  GTKF(gtk_moz_embed_pop_startup) \
  GTKF(gtk_moz_embed_set_path) \
  GTKF(gtk_moz_embed_set_comp_path) \
  GTKF(gtk_moz_embed_set_profile_path) \
  GTKF(gtk_moz_embed_load_url) \
  GTKF(gtk_moz_embed_stop_load) \
  GTKF(gtk_moz_embed_can_go_back) \
  GTKF(gtk_moz_embed_can_go_forward) \
  GTKF(gtk_moz_embed_go_back) \
  GTKF(gtk_moz_embed_go_forward) \
  GTKF(gtk_moz_embed_render_data) \
  GTKF(gtk_moz_embed_open_stream) \
  GTKF(gtk_moz_embed_append_data) \
  GTKF(gtk_moz_embed_close_stream) \
  GTKF(gtk_moz_embed_get_link_message) \
  GTKF(gtk_moz_embed_get_js_status) \
  GTKF(gtk_moz_embed_get_title) \
  GTKF(gtk_moz_embed_get_location) \
  GTKF(gtk_moz_embed_reload) \
  GTKF(gtk_moz_embed_set_chrome_mask) \
  GTKF(gtk_moz_embed_get_chrome_mask) \
  GTKF(gtk_moz_embed_single_get_type) \
  GTKF(gtk_moz_embed_single_get)

#define GTKMOZEMBED_FUNCTIONS_INTERNAL \
  GTKF(gtk_moz_embed_get_nsIWebBrowser) \
  GTKF(gtk_moz_embed_get_title_unichar) \
  GTKF(gtk_moz_embed_get_js_status_unichar) \
  GTKF(gtk_moz_embed_get_link_message_unichar) \
  GTKF(gtk_moz_embed_set_directory_service_provider) \
  GTKF(gtk_moz_embed_set_app_components)

#define GTKF(fname) fname##Type fname;

GTKMOZEMBED_FUNCTIONS
GTKMOZEMBED_FUNCTIONS_INTERNAL

#undef GTKF

#define GTKF(fname) { #fname, (NSFuncPtr*) &fname },

static const nsDynamicFunctionLoad GtkSymbols[] = {
GTKMOZEMBED_FUNCTIONS
  { nsnull, nsnull }
};

static const nsDynamicFunctionLoad GtkSymbolsInternal[] = {
GTKMOZEMBED_FUNCTIONS_INTERNAL
  { nsnull, nsnull }
};
#undef GTKF

static nsresult
GTKEmbedGlueStartup()
{
  return XPCOMGlueLoadXULFunctions(GtkSymbols);
}

static nsresult
GTKEmbedGlueStartupInternal()
{
  return XPCOMGlueLoadXULFunctions(GtkSymbolsInternal);
}

