#include "global.h"

#include "MailFrame.h"
#include "GeckoProtocolHandler.h"

#include "nsIURI.h"

const char msg1[] =
"Attention: Sir, \n"
"Good day. I am ALEXANDER NENE, Solicitor and Notary Public, The Personal Attorney to MR HENRI CARLTON, The president of DIAMOND SAFARIESCO.LTD.ACCRA-GHANA who is a National of your country. On the 21st of April 2000, my client, his wife and their only son were Involved in a car accident along ACCRA/KUMASI Express Road. \n"
"Unfortunately, they all lost their lives in the event of the accident, since then I have made several enquiries to locate any of my clients extended relatives and this has also proved unsuccessful.After these several unsuccessful attempts, I decided to trace his Relatives over the Internet, to locate any member of his family but of no avail, hence I contacted youI contacted you to assist in repatriating the money and property left Behind before they get confiscated or declare unserviceable by the bank where my client lodged this huge deposits. Particularly, the Bank where the deceased had an account valued at about 28.3 million dollars.Conseqently, the bank issued me a notice to provide the next of kin or Have the account confiscated within a short time. Since I have been Unsuccessful in locating the relatives for over some years now, I seek your consent to present you as the next of kin of the deceased since you share the same surname so that the proceeds of this account va!\n"
"lued at 48.3 million dollars can be paid to you for both of us to share the money; 70% to me and 25% to you, while 5% should be for expenses or tax as your government may require. I have all necessary legal documents that can be used to backup the claim.All I require is your honest cooperation to enable us see this deal \n"
"Through. I guarantee that this will be executed under a legitimate arrangement that will protect you from any breach of the law. Please get in touch with me through my email to enable us discuss further. \n"
"Thanks for you kind co-operation \n"
"ALEXANDER NENE \n"
"\n"
"\n"
"___________________________________________________\n"
"GO.com Mail                                    \n"
"Get Your Free, Private E-mail at http://mail.go.com\n";


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
        nsCAutoString txt(msg1);
        aContentType.Assign("text/plain");

        size_t size = txt.Length();
        *aData = (void *) nsMemory::Alloc(size);
        if (!*aData)
            return NS_ERROR_OUT_OF_MEMORY;
        memset(*aData, 0, size);
        memcpy((char *) *aData, txt.get(), size);
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

    CreateStatusBar();
}

void MailFrame::OnArticleClick(wxListEvent &event)
{
    if (mWebbrowser)
    {
        wxString url = "wxmail:" + event.GetIndex();
        if (!url.IsEmpty())
        {
            nsCOMPtr<nsIWebNavigation> webNav = do_QueryInterface(mWebbrowser);
            webNav->LoadURI(NS_ConvertASCIItoUCS2(url.c_str()).get(),
                                   nsIWebNavigation::LOAD_FLAGS_NONE,
                                   nsnull,
                                   nsnull,
                                   nsnull);
        }
    }
}

