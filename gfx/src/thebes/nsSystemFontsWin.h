
#ifndef _NS_SYSTEMFONTSWIN_H_
#define _NS_SYSTEMFONTSWIN_H_

#include <nsFont.h>
#include <nsIDeviceContext.h>

class nsSystemFontsWin
{
public:
    nsSystemFontsWin(float aPixelsToTwips);

    nsresult CopyLogFontToNSFont(HDC* aHDC, const LOGFONT* ptrLogFont,
				 nsFont* aFont, PRBool aIsWide = PR_FALSE) const;
    nsresult GetSysFontInfo(HDC aHDC, nsSystemFontID anID, nsFont* aFont) const;
    nsresult GetSystemFont(nsSystemFontID anID, nsFont *aFont) const;
};

#endif /* _NS_SYSTEMFONTSWIN_H_ */

