#ifndef BROWSERFRAME_H
#define BROWSERFRAME_H

#include "GeckoContainer.h"
#include "GeckoWindow.h"

class BrowserFrame :
    public wxFrame,
    public GeckoContainerUI
{
public :
    BrowserFrame(wxWindow* aParent);

    DECLARE_EVENT_TABLE()

    void OnBrowserUrl(wxCommandEvent &event);
    void OnBrowserGo(wxCommandEvent &event);
    void OnBrowserBack(wxCommandEvent &event);
    void OnBrowserForward(wxCommandEvent &event);
    void OnBrowserReload(wxCommandEvent &event);
    void OnBrowserStop(wxCommandEvent &event);
    void OnBrowserHome(wxCommandEvent &event);

    GeckoWindow            *mGeckoWnd;
    nsCOMPtr<nsIWebBrowser> mWebbrowser;
  
    // GeckoContainerUI overrides
    void UpdateStatusBarText(const PRUnichar* aStatusText);
    void UpdateCurrentURI();
};

#endif