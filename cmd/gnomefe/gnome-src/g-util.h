
/*
  g-util.c -- misc. utilities.
  Created: Chris Toshok <toshok@hungry.com>, 9-Apr-98.
*/

#ifndef _moz_utils_h
#define _moz_utils_h

#include "g-types.h"
#include "prtypes.h"
#include "ntypes.h"

extern MozView *moz_get_view_of_type(MozView *root, PRUint32 type);

extern MozFrame *find_frame(MWContext *context);
extern MozHTMLView *find_html_view(MWContext *context);
extern MozEditorView *find_editor_view(MWContext *context);
extern MozBookmarkView *find_bookmark_view(MWContext *context);

#endif
