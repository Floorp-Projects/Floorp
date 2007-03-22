/* ***** BEGIN LICENSE BLOCK *****
 * Version: Mozilla-sample-code 1.0
 *
 * Copyright (c) 2002 Netscape Communications Corporation and
 * other contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this Mozilla sample software and associated documentation files
 * (the "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the
 * following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef GECKOWND_H_270B94BB_E0EE_44bd_8983_CC43BFA39996
#define GECKOWND_H_270B94BB_E0EE_44bd_8983_CC43BFA39996

#include "GeckoContainer.h"

class GeckoWindow : public wxPanel
{
    DECLARE_DYNAMIC_CLASS(GeckoWindow);

protected:
    DECLARE_EVENT_TABLE()
    void OnSize(wxSizeEvent &event);
    void OnSetFocus(wxFocusEvent &event);
    void OnKillFocus(wxFocusEvent &event);

    GeckoContainer *mGeckoContainer;

public:
    GeckoWindow();
    virtual ~GeckoWindow();
    void SetGeckoContainer(GeckoContainer *aGeckoContainer);
    GeckoContainer *GetGeckoContainer() const { return mGeckoContainer; }
};

#endif /* GECKOWND_H_270B94BB_E0EE_44bd_8983_CC43BFA39996 */