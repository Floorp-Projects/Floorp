#ifndef MAILFRAME_H
#define MAILFRAME_H

#include "GeckoContainer.h"
#include "GeckoWindow.h"

class MailFrame :
    public wxFrame,
    public GeckoContainerUI
{
public :
    MailFrame(wxWindow* aParent);

    DECLARE_EVENT_TABLE()

    void OnArticleClick(wxListEvent &event);

    GeckoWindow            *mGeckoWnd;
    nsCOMPtr<nsIWebBrowser> mWebbrowser;
  };

#endif