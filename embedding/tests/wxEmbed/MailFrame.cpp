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

#include "MailFrame.h"
#include "GeckoProtocolHandler.h"
#include "nsEmbedString.h"

#include "nsIURI.h"

struct MailMsg
{
    char *mSubject;
    char *mSender;
    char *mDate;
    char *mBody;
};

MailMsg gSampleMessages[] = 
{
    {
        "URGENT ASSITANCE PLS",
        "alexander nene",
        "09/06/2003 14:38",
        "<html><body>"
        "<p>Attention: Sir,<br>\n"
        "<p>Good day. I am ALEXANDER NENE, Solicitor and Notary Public, The Personal Attorney"
        "to MR HENRI CARLTON, The president of DIAMOND SAFARIESCO.LTD.ACCRA-GHANA who is a"
        "National of your country. On the 21st of April 2000, my client, his wife and their "
        "only son were Involved in a car accident along ACCRA/KUMASI Express Road."
        "<p>Unfortunately, they all lost their lives in the event of the accident, since then "
        "I have made several enquiries to locate any of my clients extended relatives and"
        "this has also proved unsuccessful.After these several unsuccessful attempts, I decided"
        "to trace his Relatives over the Internet, to locate any member of his family but of"
        "no avail, hence I contacted youI contacted you to assist in repatriating the money and"
        "property left Behind before they get confiscated or declare unserviceable by the bank"
        "where my client lodged this huge deposits. Particularly, the Bank where the deceased had"
        "an account valued at about 28.3 million dollars.Conseqently, the bank issued me a notice "
        "to provide the next of kin or Have the account confiscated within a short time. Since "
        "I have been Unsuccessful in locating the relatives for over some years now, I seek your "
        "consent to present you as the next of kin of the deceased since you share the same surname "
        "so that the proceeds of this account valued at 48.3 million dollars can be paid to you "
        "for both of us to share the money; 70% to me and 25% to you, while 5% should be for "
        "expenses or tax as your government may require. I have all necessary legal documents that"
        "can be used to backup the claim.All I require is your honest cooperation to enable us see"
        "this deal Through. "
        "<p>I guarantee that this will be executed under a legitimate arrangement"
        "that will protect you from any breach of the law. Please get in touch with me through my"
        "email to enable us discuss further.<br><br> \n"
        "<p>Thanks for you kind co-operation<br><br> \n"
        "<p>ALEXANDER NENE<br><br><br></body></html>"
    },
    {
        "Reminder: Network Outage Tonight",
        "IT Dept",
        "09/06/2003 15:22",
        "<html><body>The network will be going down tonight so please log off "
        "your computers before going home.</body></html>"
    },
    {
        "Expense Reports Due",
        "Finance Dept",
        "09/06/2003 15:40",
        "<html><body>Please submit expense reports ASAP if you want to see payment "
        "in your accounts this month!</body></html>"
    },
    {
        "Flight confirmation",
        "Expediocity",
        "12/06/2003 12:31",
        "<html><body>Thank you for booking with Expediocity, Your flight reservation has been "
        "confirmed and details may be found online <a href=\"http://www.mozilla.org\">here</a>.</body></html>"
    }
};

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

        long i;
        if (sscanf(path.get(), "//%ld", &i) == 1)
        {
            txt = gSampleMessages[i].mBody;
        }
        else
        {
            return NS_ERROR_FAILURE;
        }

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

BEGIN_EVENT_TABLE(MailFrame, GeckoFrame)
    EVT_LIST_ITEM_SELECTED(XRCID("articles"), MailFrame::OnArticleClick)
END_EVENT_TABLE()

MailFrame::MailFrame(wxWindow* aParent)
{
    wxXmlResource::Get()->LoadFrame(this, aParent, wxT("mail_frame"));

    SetName("mail");

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

        for (long i = 0; i < sizeof(gSampleMessages) / sizeof(gSampleMessages[0]); i++)
        {
            articleListCtrl->InsertItem(i, gSampleMessages[i].mSubject);
            articleListCtrl->SetItem(i, 1, gSampleMessages[i].mSender);
            articleListCtrl->SetItem(i, 2, gSampleMessages[i].mDate);
        }
    }

    SetupDefaultGeckoWindow();

    wxWindow *hdrPanel = FindWindowById(XRCID("mail_header_panel"), this);
    if (hdrPanel)
    {
        hdrPanel->Show(FALSE);
    }


    CreateStatusBar();
}

void MailFrame::OnArticleClick(wxListEvent &event)
{
    if (mWebBrowser)
    {
        long idx = event.GetIndex();
        wxString url = wxString::Format("wxmail://%ld", idx);
        if (!url.IsEmpty())
        {
            wxStaticText *txtFrom = (wxStaticText *) FindWindowById(XRCID("mail_from"), this);
            if (txtFrom)
                txtFrom->SetLabel(gSampleMessages[idx].mSender);
            wxStaticText *txtDate = (wxStaticText *) FindWindowById(XRCID("mail_date"), this);
            if (txtDate)
                txtDate->SetLabel(gSampleMessages[idx].mDate);
            wxStaticText *txtSubject = (wxStaticText *) FindWindowById(XRCID("mail_subject"), this);
            if (txtSubject)
                txtSubject->SetLabel(gSampleMessages[idx].mSubject);

            wxWindow *hdrPanel = FindWindowById(XRCID("mail_header_panel"), this);
            if (hdrPanel)
                hdrPanel->Show(TRUE);

            nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebBrowser);
            webNav->LoadURI(NS_ConvertASCIItoUCS2(url.c_str()).get(),
                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                   nsnull,
                                   nsnull,
                                   nsnull);
        }
    }
}

