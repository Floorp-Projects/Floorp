 /*
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "csid.h"
#include "resgui.h"

/*	Conversion Table That Convert From Unicode to Script Text */
type 'UFRM' {
		array uint {
		unsigned integer;	/* Just an uint16 */
	};
};
/*	Conversion Table That Convert From  Script Text to Unicode */
type 'UTO ' {
		array uint {
		unsigned integer;	/* Just an uint16 */
	};
};


/* a Priorized List of csid that will be used to render unicode */
type CSIDLIST_RESTYPE {
	integer = $$CountOf(table);
	wide array table	{
		unsigned integer;	/* csid  */
	};
};

#define RES_CS_MAC_ICELANDIC	(CS_MAC_ROMAN | 0x1000)	/* ftang- fix me after 4.0 */

resource 'UFRM' ( RES_CS_MAC_ICELANDIC, "macicela.uf- RES_CS_MAC_ICELANDIC", purgeable) {{ 
#include "macicela.uf"
}};
resource 'UFRM' ( CS_MAC_ROMAN, "macroman.uf- CS_MAC_ROMAN", purgeable) {{ 
#include "macroman.uf"
}};
resource 'UFRM' ( CS_SJIS, "sjis.uf- CS_LATIN1", purgeable) {{ 
#include "sjis.uf"
}};
resource 'UFRM' ( CS_MAC_CE, "macce.uf- CS_MAC_CE", purgeable) {{ 
#include "macce.uf"
}};
resource 'UFRM' ( CS_BIG5, "big5.uf- CS_BIG5", purgeable) {{ 
#include "big5.uf"
}};
resource 'UFRM' ( CS_GB_8BIT, "gb2312.uf- CS_GB_8BIT", purgeable) {{ 
#include "gb2312.uf"
}};
resource 'UFRM' ( CS_KSC_8BIT, "ksc5601.uf- CS_KSC_8BIT", purgeable) {{ 
#include "u20kscgl.uf"
}};
resource 'UFRM' ( CS_DINGBATS, "macdingb.uf- CS_DINGBATS", purgeable) {{ 
#include "macdingb.uf"
}};
resource 'UFRM' ( CS_SYMBOL, "macsymbo.uf- CS_SYMBOL", purgeable) {{ 
#include "macsymbo.uf"
}};
resource 'UFRM' ( CS_MAC_CYRILLIC, "maccyril.uf- CS_MAC_CYRILLIC", purgeable) {{ 
#include "maccyril.uf"
}};
resource 'UFRM' ( CS_MAC_GREEK, "macgreek.uf- CS_MAC_GREEK", purgeable) {{ 
#include "macgreek.uf"
}};
resource 'UFRM' ( CS_MAC_TURKISH, "macturki.uf- CS_MAC_TURKISH", purgeable) {{ 
#include "macturki.uf"
}};


resource 'UTO '  ( RES_CS_MAC_ICELANDIC, "macicela.ut- RES_CS_MAC_ICELANDIC", purgeable) {{ 
#include "macicela.ut"
}};
resource 'UTO '  ( CS_MAC_ROMAN, "macroman.ut- CS_MAC_ROMAN", purgeable) {{ 
#include "macroman.ut"
}};
resource 'UTO '  ( CS_SJIS, "sjis.ut- CS_LATIN1", purgeable) {{ 
#include "sjis.ut"
}};
resource 'UTO '  ( CS_MAC_CE, "macce.ut- CS_MAC_CE", purgeable) {{ 
#include "macce.ut"
}};
resource 'UTO '  ( CS_BIG5, "big5.ut- CS_BIG5", purgeable) {{ 
#include "big5.ut"
}};
resource 'UTO '  ( CS_GB_8BIT, "gb2312.ut- CS_GB_8BIT", purgeable) {{ 
#include "gb2312.ut"
}};
resource 'UTO '  ( CS_KSC_8BIT, "ksc5601.ut- CS_KSC_8BIT", purgeable) {{ 
#include "u20kscgl.ut"
}};
resource 'UTO ' ( CS_DINGBATS, "macdingb.ut- CS_DINGBATS", purgeable) {{ 
#include "macdingb.ut"
}};
resource 'UTO ' ( CS_SYMBOL, "macsymbo.ut- CS_SYMBOL", purgeable) {{ 
#include "macsymbo.ut"
}};
resource 'UTO ' ( CS_MAC_CYRILLIC, "maccyril.ut- CS_MAC_CYRILLIC", purgeable) {{ 
#include "macdingb.ut"
}};
resource 'UTO ' ( CS_MAC_GREEK, "macgreek.ut- CS_MAC_GREEK", purgeable) {{ 
#include "macgreek.ut"
}};
resource 'UTO ' ( CS_MAC_TURKISH, "macturki.ut- CS_MAC_TURKISH", purgeable) {{ 
#include "macturki.ut"
}};

resource CSIDLIST_RESTYPE (CSIDLIST_RESID, "Roman/CE/Cy/Gr/J/TC/SC/K/Symbol/Dingbats/Tr", purgeable) {{
    CS_MAC_ROMAN,
    CS_MAC_CE,
    CS_MAC_CYRILLIC,
    CS_MAC_GREEK,
    CS_SJIS,
    CS_BIG5,
    CS_GB_8BIT,
    CS_KSC_8BIT,
    CS_SYMBOL,
    CS_DINGBATS,
    CS_MAC_TURKISH
}};
resource CSIDLIST_RESTYPE (CSIDLIST_RESID+1, "Roman/CE/Cy/Gr/TC/SC/K/J/Symbol/Dingbats/Tr", purgeable) {{
    CS_MAC_ROMAN,
    CS_MAC_CE,
    CS_MAC_CYRILLIC,
    CS_MAC_GREEK,
    CS_BIG5,
    CS_GB_8BIT,
    CS_KSC_8BIT,
    CS_SJIS,
    CS_SYMBOL,
    CS_DINGBATS,
    CS_MAC_TURKISH
}};
resource CSIDLIST_RESTYPE (CSIDLIST_RESID+2, "Roman/CE/Cy/Gr/SC/K/J/TC/Symbol/Dingbats/Tr", purgeable) {{
    CS_MAC_ROMAN,
    CS_MAC_CE,
    CS_MAC_CYRILLIC,
    CS_MAC_GREEK,
    CS_GB_8BIT,
    CS_KSC_8BIT,
    CS_SJIS,
    CS_BIG5,
    CS_SYMBOL,
    CS_DINGBATS,
    CS_MAC_TURKISH
}};
resource CSIDLIST_RESTYPE (CSIDLIST_RESID+3, "Roman/CE/Cy/Gr/K/J/TC/SC/Symbol/Dingbats/Tr", purgeable) {{
    CS_MAC_ROMAN,
    CS_MAC_CE,
    CS_MAC_CYRILLIC,
    CS_MAC_GREEK,
    CS_KSC_8BIT,
    CS_SJIS,
    CS_BIG5,
    CS_GB_8BIT,
    CS_SYMBOL,
    CS_DINGBATS,
    CS_MAC_TURKISH
}};
