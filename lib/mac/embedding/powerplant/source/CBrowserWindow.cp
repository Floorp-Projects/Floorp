
#include "CBrowserWindow.h"
#include "CWebShell.h"
#include <LEditText.h>
#include <LStaticText.h>


// CBrowserWindow:
// A simple browser window that hooks up a CWebShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


enum
{
	paneID_BackButton		= 'Back'
,	paneID_ForwardButton	= 'Forw'
,	paneID_StopButton		= 'Stop'
,	paneID_URLField			= 'gUrl'
,	paneID_WebShellView		= 'WebS'
,	paneID_StatusBar		= 'Stat'
,	paneID_ChasingArrows	= 'Arro'
};


// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Default Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow()
{
	mWebShellView = nil;
	mURLField = nil;
	mStatusBar = nil;
}



// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Stream Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LStream*	inStream)
	: LWindow(inStream)
{
	mWebShellView = nil;
	mURLField = nil;
	mStatusBar = nil;
}


// ---------------------------------------------------------------------------
//	¥ ~CBrowserWindow						Destructor				  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::~CBrowserWindow()
{
}


// ---------------------------------------------------------------------------
//	¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void
CBrowserWindow::FinishCreateSelf()
{
	UReanimator::LinkListenerToControls(this, this, wind_SampleBrowserWindow);
	StartListening();

	// display the default url
	mWebShellView = static_cast<CWebShell*>(FindPaneByID(paneID_WebShellView));
	mURLField = dynamic_cast<LEditText*>(FindPaneByID(paneID_URLField));
	if (mWebShellView && mURLField)
	{
		LStr255 defaultURL;
		mWebShellView->GetURLString(defaultURL);
		mURLField->SetDescriptor(defaultURL);
		mURLField->SelectAll();
		SwitchTarget(mURLField);
	}

	// display a default status
	mStatusBar = dynamic_cast<LStaticText*>(FindPaneByID(paneID_StatusBar));
	if (mStatusBar)
		mStatusBar->SetDescriptor("\pWarming up...");

	// let the WebShell know where the status can be displayed
	if (mWebShellView)
	{
		mWebShellView->SetStatusBar(mStatusBar);
		mWebShellView->SetThrobber(FindPaneByID(paneID_ChasingArrows));
	}
}

// ---------------------------------------------------------------------------
//	¥ ListenToMessage
// ---------------------------------------------------------------------------

void
CBrowserWindow::ListenToMessage(
	MessageT		inMessage,
	void*			ioParam)
{
	ProcessCommand(inMessage, ioParam);
}

// ---------------------------------------------------------------------------
//	¥ ObeyCommand
// ---------------------------------------------------------------------------

Boolean
CBrowserWindow::ObeyCommand(
	CommandT			inCommand,
	void				*ioParam)
{
#pragma unused(ioParam)

	Boolean	cmdHandled = false;
	if (mWebShellView)
	{
		switch (inCommand)
		{
			case paneID_BackButton:
				mWebShellView->Back();
				cmdHandled = true;
				break;

			case paneID_ForwardButton:
				mWebShellView->Forward();
				cmdHandled = true;
				break;

			case paneID_StopButton:
				mWebShellView->Stop();
				cmdHandled = true;
				break;

			case paneID_URLField:
				LStr255 urlString;
				mURLField->GetDescriptor(urlString);
				mWebShellView->LoadURL(urlString);
				cmdHandled = true;
				break;
		}
	}

	if (!cmdHandled)
		cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
	return cmdHandled;
}
