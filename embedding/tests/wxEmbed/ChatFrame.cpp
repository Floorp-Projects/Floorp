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

#include "global.h"

#include "wx/strconv.h"

#include "nsIWebNavigation.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLDocument.h"

#include "ChatFrame.h"

BEGIN_EVENT_TABLE(ChatFrame, GeckoFrame)
    EVT_TEXT_ENTER(XRCID("chat"),        ChatFrame::OnChat)
END_EVENT_TABLE()

ChatFrame::ChatFrame(wxWindow* aParent)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("chat_frame"));

    SetName("chat");

    SetIcon(wxICON(appicon));

    SetupDefaultGeckoWindow();

    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
    webNav->LoadURI(NS_ConvertASCIItoUCS2("about:blank").get(),
        nsIWebNavigation::LOAD_FLAGS_NONE, nsnull, nsnull, nsnull);

}

void ChatFrame::OnChat(wxCommandEvent &event)
{
    nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
    nsCOMPtr<nsIDOMDocument> doc;
    webNav->GetDocument(getter_AddRefs(doc));
    nsCOMPtr<nsIDOMHTMLDocument> htmlDoc = do_QueryInterface(doc);
    // doc->CreateElement(getter_AddRefs(element));

    wxTextCtrl *chatCtrl = (wxTextCtrl *) FindWindowById(XRCID("chat"), this);
    if (chatCtrl)
    {
        wxString html("<p>Foo: ");
        html += chatCtrl->GetValue();
        wxMBConv conv;
        nsAutoString htmlU(conv.cWX2WC(html));
        htmlDoc->Writeln(htmlU);
        chatCtrl->SetValue("");
    }
}
