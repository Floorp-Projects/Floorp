#ifndef __MOZ_FONT_SELECT_H__
#define __MOZ_FONT_SELECT_H__

#include "gdk/gdktypes.h"
#include "gdk/gdkx.h"
#include "gdk/gdkkeysyms.h"

/* available functions */
void 
moz_font_init();

GdkFont *
moz_get_font_with_family(char *fam, 
			 int pts, 
			 char *wgt, 
			 gboolean ita);


GdkFont *
moz_get_font(LO_TextAttr *text_attr);

void
moz_font_release_text_attribute(LO_TextAttr *attr);

#endif
