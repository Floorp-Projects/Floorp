/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Pango
 * pango-utils.h: Utilities for internal functions and modules
 *
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
 * The Original Code is the Pango Library (www.pango.org) 
 * 
 * The Initial Developer of the Original Code is Red Hat Software
 * Portions created by Red Hat are Copyright (C) 2000 Red Hat Software.
 * 
 * Contributor(s): 
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Lessor General Public License Version 2 (the 
 * "LGPL"), in which case the provisions of the LGPL are applicable 
 * instead of those above.  If you wish to allow use of your 
 * version of this file only under the terms of the LGPL and not to
 * allow others to use your version of this file under the MPL,
 * indicate your decision by deleting the provisions above and
 * replace them with the notice and other provisions required by
 * the LGPL. If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * LGPL.
*/

#include <stdio.h>
#include <glib.h>

#include "pango-types.h"

char    *pangolite_trim_string(const char *str);
char   **pangolite_split_file_list(const char *str);

gint     pangolite_read_line(FILE *stream, GString *str);

gboolean pangolite_skip_space(const char **pos);
gboolean pangolite_scan_word(const char **pos, GString *out);
gboolean pangolite_scan_string(const char **pos, GString *out);
gboolean pangolite_scan_int(const char **pos, int *out);

char *   pangolite_config_key_get(const char *key);

/* On Unix, return the name of the "pangolite" subdirectory of SYSCONFDIR
 * (which is set at compile time). On Win32, return the Pangolite
 * installation directory (which is set at installation time, and
 * stored in the registry). The returned string should not be
 * g_free'd.
 */
const char *pangolite_get_sysconf_subdirectory(void);

/* Ditto for LIBDIR/pangolite. On Win32, use the same Pangolite
 * installation directory. This returned string should not be
 * g_free'd either.
 */
const char *pangolite_get_lib_subdirectory (void);
