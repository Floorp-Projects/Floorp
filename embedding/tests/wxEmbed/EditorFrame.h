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

#ifndef EDITORFRAME_H
#define EDITORFRAME_H

#include "GeckoFrame.h"

#include "nsICommandManager.h"
#include "nsICommandParams.h"

class EditorFrame :
    public GeckoFrame
{
protected:
    DECLARE_EVENT_TABLE()
    void OnEditBold(wxCommandEvent &event);
    void OnEditItalic(wxCommandEvent &event);
    void OnEditUnderline(wxCommandEvent &event);
    void OnEditIndent(wxCommandEvent &event);
    void OnEditOutdent(wxCommandEvent &event);
    void OnUpdateToggleCmd(wxUpdateUIEvent &event);
    void OnEditAlignLeft(wxCommandEvent &event);
    void OnEditAlignRight(wxCommandEvent &event);
    void OnEditAlignCenter(wxCommandEvent &event);
public :
    EditorFrame(wxWindow* aParent);

    nsCOMPtr<nsICommandManager> mCommandManager;

    // GeckoContainerUI overrides
    virtual void UpdateStatusBarText(const PRUnichar* aStatusText);

    void MakeEditable();
    nsresult DoCommand(const char *aCommand, nsICommandParams *aCommandParams);
    void IsCommandEnabled(const char *aCommand, PRBool *retval);
    void GetCommandState(const char *aCommand, nsICommandParams *aCommandParams);
    nsresult ExecuteAttribParam(const char *aCommand, const char *aAttribute);
};

#endif