/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

pref("editor.author",                       "");

pref("editor.use_custom_colors",            false);
pref("editor.text_color",                   "#000000");
pref("editor.link_color",                   "#0000FF");
pref("editor.active_link_color",            "#000088");
pref("editor.followed_link_color",          "#FF0000");
pref("editor.background_color",             "#FFFFFF");
pref("editor.use_background_image",         false);
localDefPref("editor.background_image",     "");
pref("editor.use_custom_default_colors", 1);

localDefPref("editor.hrule.height",         2);
localDefPref("editor.hrule.width",          100);
pref("editor.hrule.width_percent",          true);
pref("editor.hrule.shading",                true);
pref("editor.hrule.align",                  1); // center

pref("editor.table.maintain_structure", true);
pref("editor.table.delete_key", 0);

pref("editor.prettyprint", true);

pref("editor.htmlWrapColumn", 72);

pref("editor.throbber.url","chrome://editor-region/locale/region.properties");

pref("editor.auto_save",                    false);
pref("editor.auto_save_delay",              10);    // minutes
pref("editor.use_html_editor",              0);
pref("editor.html_editor",                  "");
pref("editor.use_image_editor",             0);
pref("editor.image_editor",                 "");

pref("editor.singleLine.pasteNewlines",     1);

pref("editor.history.url_maximum", 10);

pref("editor.quotesPreformatted",            true);
