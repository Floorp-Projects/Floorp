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
 *
 *   Adam Lock <adamlock@netscape.com>
 *
 * ***** END LICENSE BLOCK ***** */

#include "global.h"

#include "MailFrame.h"
#include "GeckoProtocolHandler.h"

#include "nsIURI.h"

const char msg1[] =
"<html><body>"
"Attention: Sir,<br>\n"
"Good day. I am ALEXANDER NENE, Solicitor and Notary Public, The Personal Attorney<br>"
"to MR HENRI CARLTON, The president of DIAMOND SAFARIESCO.LTD.ACCRA-GHANA who is a<br>"
"National of your country. On the 21st of April 2000, my client, his wife and their <br>"
"only son were Involved in a car accident along ACCRA/KUMASI Express Road.<br>\n"
"Unfortunately, they all lost their lives in the event of the accident, since then<br>"
"I have made several enquiries to locate any of my clients extended relatives and<br>"
"this has also proved unsuccessful.After these several unsuccessful attempts, I decided<br>"
"to trace his Relatives over the Internet, to locate any member of his family but of<br>"
"no avail, hence I contacted youI contacted you to assist in repatriating the money and<br>"
"property left Behind before they get confiscated or declare unserviceable by the bank<br>"
"where my client lodged this huge deposits. Particularly, the Bank where the deceased had<br>"
"an account valued at about 28.3 million dollars.Conseqently, the bank issued me a notice<br>"
"to provide the next of kin or Have the account confiscated within a short time. Since<br>"
"I have been Unsuccessful in locating the relatives for over some years now, I seek your<br>"
"consent to present you as the next of kin of the deceased since you share the same surname<br>"
" so that the proceeds of this account valued at 48.3 million dollars can be paid to you<br>"
"for both of us to share the money; 70% to me and 25% to you, while 5% should be for<br>"
"expenses or tax as your government may require. I have all necessary legal documents that<br>"
"can be used to backup the claim.All I require is your honest cooperation to enable us see<br>"
"this deal Through. I guarantee that this will be executed under a legitimate arrangement<br>"
"that will protect you from any breach of the law. Please get in touch with me through my<br>"
"email to enable us discuss further.<br><br> \n"
"Thanks for you kind co-operation<br><br> \n"
"ALEXANDER NENE<br><br><br></body></html>";


const char msg2[] =
"<html><body>The network will be going down tonight so please log off your computers before going home.</body></html>";

const char msg3[] =
"<html><body>Please submit expense reports ASAP if you want to see payment in your accounts this month!</body></html>";

static bool gMailChannelCallbackRegistered = FALSE;

class MailChannelCallback : public GeckoChannelCallback
{
public:
    virtual nsresult GetData(
        nsIURI *aURI,
        nsIChannel *aChannel,
        nsACString &aContentType,
        void **aData,
        PRUint32 *aSize)
    {
        nsCAutoString txt;
        aContentType.Assign("text/html");

        nsCAutoString path;
        aURI->GetPath(path);
        if (path.Equals("//0"))
            txt = msg1;
        else if (path.Equals("//1"))
            txt = msg2;
        else
            txt = msg3;

        size_t size = txt.Length();
        *aData = (void *) nsMemory::Alloc(size + 1);
        if (!*aData)
            return NS_ERROR_OUT_OF_MEMORY;
        memset(*aData, 0, size + 1);
        memcpy(*aData, txt.get(), size);
        *aSize = size;
        return NS_OK;
    }
};

BEGIN_EVENT_TABLE(MailFrame, wxFrame)
    EVT_LIST_ITEM_SELECTED(XRCID("articles"), MailFrame::OnArticleClick)
END_EVENT_TABLE()

MailFrame::MailFrame(wxWindow* aParent) :
    mGeckoWnd(NULL)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("mail_frame"));

    SetIcon(wxICON(appicon));

    if (!gMailChannelCallbackRegistered)
    {
        GeckoProtocolHandler::RegisterHandler("wxmail", "wxMail handler", new MailChannelCallback);
        gMailChannelCallbackRegistered = TRUE;
    }

    // Set up the article pane
    wxListCtrl *articleListCtrl = (wxListCtrl *) FindWindowById(XRCID("articles"), this);
    if (articleListCtrl)
    {
        articleListCtrl->InsertColumn(0, "Subject", wxLIST_FORMAT_LEFT, 200);
        articleListCtrl->InsertColumn(1, "Sender", wxLIST_FORMAT_LEFT, 100);
        articleListCtrl->InsertColumn(2, "Date", wxLIST_FORMAT_LEFT, 100);
        long idx;
        idx = articleListCtrl->InsertItem(0, "URGENT ASSITANCE PLS");
        articleListCtrl->SetItem(idx, 1, "alexander nene");
        articleListCtrl->SetItem(idx, 2, "09/06/2003 14:38");
        idx = articleListCtrl->InsertItem(1, "Reminder: Network Outage Tonight");
        articleListCtrl->SetItem(idx, 1, "IT Dept");
        articleListCtrl->SetItem(idx, 2, "09/06/2003 15:22");
        idx = articleListCtrl->InsertItem(2, "Expense Reports Due");
        articleListCtrl->SetItem(idx, 1, "Finance Dept");
        articleListCtrl->SetItem(idx, 2, "09/06/2003 15:40");
    }

    mGeckoWnd = (GeckoWindow *) FindWindowById(XRCID("gecko"), this);
    if (mGeckoWnd)
    {
        GeckoContainer *geckoContainer = new GeckoContainer(this);
        if (geckoContainer)
        {
            mGeckoWnd->SetGeckoContainer(geckoContainer);

            PRUint32 aChromeFlags = nsIWebBrowserChrome::CHROME_ALL;
            geckoContainer->SetChromeFlags(aChromeFlags);
            geckoContainer->SetParent(nsnull);

            wxSize size = mGeckoWnd->GetClientSize();

            // Insert the browser
            geckoContainer->CreateBrowser(0, 0, size.GetWidth(), size.GetHeight(),
                (nativeWindow) mGeckoWnd->GetHWND(), getter_AddRefs(mWebbrowser));

            GeckoContainerUI::ShowWindow(PR_TRUE);

        }
    }

    wxWindow *hdrPanel = FindWindowById(XRCID("mail_header_panel"), this);
    if (hdrPanel)
    {
        hdrPanel->Show(FALSE);
    }


    CreateStatusBar();
}

void MailFrame::OnArticleClick(wxListEvent &event)
{
    if (mWebbrowser)
    {
        wxString url = wxString::Format("wxmail://%ld", event.GetIndex());
        if (!url.IsEmpty())
        {
            wxWindow *hdrPanel = FindWindowById(XRCID("mail_header_panel"), this);
            if (hdrPanel)
            {
                hdrPanel->Show(TRUE);
            }

            nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
            webNav->LoadURI(NS_ConvertASCIItoUCS2(url.c_str()).get(),
                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                   nsnull,
                                   nsnull,
                                   nsnull);
        }
    }
}

