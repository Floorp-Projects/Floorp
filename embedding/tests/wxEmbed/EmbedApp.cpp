/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org Code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Adam Lock <adamlock@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "global.h"

#include "nsIProfile.h"
#include "nsIWindowWatcher.h"
#include "nsMemory.h"
#include "nsEmbedString.h"

#include "BrowserFrame.h"
#include "MailFrame.h"
#include "ChatFrame.h"
#include "EditorFrame.h"

#include "GeckoProtocolHandler.h"
#include "GeckoWindowCreator.h"

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
        *aData = (void *) nsMemory::Alloc(size + 1);
        if (!*aData)
            return NS_ERROR_OUT_OF_MEMORY;
        memset(*aData, 0, size + 1);
        memcpy(*aData, txt.get(), size);
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
    void OnChat(wxCommandEvent &event);
    void OnEditor(wxCommandEvent &event);
};

BEGIN_EVENT_TABLE(EmbedApp, wxApp)
    EVT_MENU(XRCID("menu_quit"),   EmbedApp::OnQuit)
    EVT_MENU(XRCID("menu_about"),  EmbedApp::OnAbout)
    EVT_MENU(XRCID("menu_mail"),   EmbedApp::OnMail)
    EVT_MENU(XRCID("menu_chat"),   EmbedApp::OnChat)
    EVT_MENU(XRCID("menu_editor"), EmbedApp::OnEditor)
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

    GeckoProtocolHandler::RegisterHandler("foo", "Test handler", new FooCallback);

    // Register a window creator for popups
    GeckoWindowCreator *creatorCallback = new GeckoWindowCreator();
    if (creatorCallback)
    {
        nsCOMPtr<nsIWindowCreator> windowCreator(NS_STATIC_CAST(nsIWindowCreator *, creatorCallback));
        if (windowCreator)
        {
            nsCOMPtr<nsIWindowWatcher> wwatch(do_GetService(NS_WINDOWWATCHER_CONTRACTID));
            if (wwatch)
            {
                wwatch->SetWindowCreator(windowCreator);
            }
        }
    }

    // Create the main frame window
    BrowserFrame* frame = new BrowserFrame(NULL);
    frame->Show(TRUE);
    // frame->OnBrowserHome(wxCommandEvent());

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
    wxFrame * frame = NULL;
    
/*
    do {
        frame = (wxFrame *) wxWindow::FindWindowByName("browser", NULL);;
        if (frame)
            frame->Close(TRUE);
    } while (frame);
    frame = (wxFrame *) wxWindow::FindWindowByName("mail", NULL);;
    if (frame)
        frame->Close(TRUE);
    frame = (wxFrame *) wxWindow::FindWindowByName("editor", NULL);;
    if (frame)
        frame->Close(TRUE);
    frame = (wxFrame *) wxWindow::FindWindowByName("chat", NULL);;
    if (frame)
        frame->Close(TRUE);
*/


    GetTopWindow()->Close(TRUE);
}

void EmbedApp::OnAbout(wxCommandEvent & WXUNUSED(event))
{
    (void)wxMessageBox(_T("wxEmbed"), _T("About wxEmbed"));
}

void EmbedApp::OnMail(wxCommandEvent & WXUNUSED(event))
{
    // Create the mail frame window
    MailFrame * frame = (MailFrame *) wxWindow::FindWindowByName("mail", NULL);;
    if (!frame)
        frame = new MailFrame(NULL);
    if (frame)
        frame->Show(TRUE);
}

void EmbedApp::OnChat(wxCommandEvent & WXUNUSED(event))
{
    ChatFrame * frame = (ChatFrame *) wxWindow::FindWindowByName("chat", NULL);
    if (!frame)
        frame = new ChatFrame(NULL);
    if (frame)
        frame->Show(TRUE);
}

void EmbedApp::OnEditor(wxCommandEvent & WXUNUSED(event))
{
    EditorFrame * frame = (EditorFrame *) wxWindow::FindWindowByName("editor", NULL);
    if (!frame)
        frame = new EditorFrame(NULL);
    if (frame)
        frame->Show(TRUE);
}
