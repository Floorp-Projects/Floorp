#include "global.h"

#include "nsIProfile.h"
#include "nsMemory.h"

#include "BrowserFrame.h"
#include "MailFrame.h"
#include "GeckoProtocolHandler.h"

class FooCallback : public GeckoChannelCallback
{
public:
    virtual nsresult GetData(
        nsIURI *aURI,
        nsIChannel *aChannel,
        nsACString &aContentType,
        void **aData,
        PRUint32 *aSize)
    {
        nsCAutoString spec;
        nsCAutoString txt("<html><body>Hello, your URL was \'");
        aURI->GetSpec(spec);
        txt.Append(spec);
        txt.Append("\'</body></html>");
        aContentType.Assign("text/html");

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

class EmbedApp : public wxApp
{
    virtual bool OnInit();
    virtual int OnExit();

    DECLARE_EVENT_TABLE()
    void OnQuit(wxCommandEvent &event);
    void OnAbout(wxCommandEvent &event);
    void OnMail(wxCommandEvent &event);
};

BEGIN_EVENT_TABLE(EmbedApp, wxApp)
    EVT_MENU(XRCID("menu_quit"),  EmbedApp::OnQuit)
    EVT_MENU(XRCID("menu_about"), EmbedApp::OnAbout)
    EVT_MENU(XRCID("menu_mail"),  EmbedApp::OnMail)
END_EVENT_TABLE()

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_APP(EmbedApp)

bool EmbedApp::OnInit()
{
    wxImage::AddHandler(new wxGIFHandler);
    wxXmlResource::Get()->InitAllHandlers();
    InitXmlResource();

    if (NS_FAILED(NS_InitEmbedding(nsnull, nsnull)))
    {
        return FALSE;
    }


    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    rv = profileService->CreateNewProfile(L"wxEmbed", nsnull, nsnull, PR_FALSE);
    if (NS_FAILED(rv)) return FALSE;
    rv = profileService->SetCurrentProfile(L"wxEmbed");
    if (NS_FAILED(rv)) return FALSE;


    // Create the main frame window
    BrowserFrame* frame = new BrowserFrame(NULL);
    frame->Show(TRUE);

    GeckoProtocolHandler::RegisterHandler("foo", "Test handler", new FooCallback);

    SetTopWindow(frame);

    return TRUE;
}

int EmbedApp::OnExit()
{
    // Shutdown the profile
    nsresult rv;
    nsCOMPtr<nsIProfile> profileService = 
             do_GetService(NS_PROFILE_CONTRACTID, &rv);
    if (NS_SUCCEEDED(rv))
    {
        profileService->ShutDownCurrentProfile(nsIProfile::SHUTDOWN_PERSIST); 
    }
    NS_TermEmbedding();
    return 0;
}

void EmbedApp::OnQuit(wxCommandEvent & WXUNUSED(event))
{
    GetTopWindow()->Close(TRUE);
}

void EmbedApp::OnAbout(wxCommandEvent & WXUNUSED(event))
{
    (void)wxMessageBox(_T("wxEmbed"), _T("About wxEmbed"));
}

void EmbedApp::OnMail(wxCommandEvent & WXUNUSED(event))
{
    // Create the mail frame window
    MailFrame * frame = new MailFrame(NULL);
    frame->Show(TRUE);
}

