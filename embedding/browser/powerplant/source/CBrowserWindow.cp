
#include "nsString.h"
#include "nsWidgetsCID.h"
#include "nsIComponentManager.h"
#include "nsComponentManagerUtils.h"
#include "nsIChannel.h"
#include "nsIURI.h"
#include "nsXPIDLString.h"

#include "CBrowserWindow.h"
#include "CBrowserShell.h"
#include "CWebBrowserChrome.h"
#include "CThrobber.h"
#include "ApplIDs.h"
#include "UMacUnicode.h"

#include <LEditText.h>
#include <LStaticText.h>
#include <LWindowHeader.h>
#include <LBevelButton.h>

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
,	paneID_Throbber	   = 'THRB'
};

// CIDs
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);

// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Default Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow() :
   mBrowserShell(NULL), mBrowserChrome(NULL),
   mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
   mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL)
{
	nsresult rv = CommonConstruct();
	if (NS_FAILED(rv))
		Throw_(NS_ERROR_GET_CODE(rv));
}



// ---------------------------------------------------------------------------
//	¥ CBrowserWindow						Stream Constructor		  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::CBrowserWindow(LStream*	inStream)
	: LWindow(inStream),
   mBrowserShell(NULL), mBrowserChrome(NULL),
   mURLField(NULL), mStatusBar(NULL), mThrobber(NULL),
   mBackButton(NULL), mForwardButton(NULL), mStopButton(NULL)
{
	nsresult rv = CommonConstruct();
	if (NS_FAILED(rv))
		Throw_(NS_ERROR_GET_CODE(rv));
}


// ---------------------------------------------------------------------------
//	¥ ~CBrowserWindow						Destructor				  [public]
// ---------------------------------------------------------------------------

CBrowserWindow::~CBrowserWindow()
{
   if (mBrowserChrome)
   {
      mBrowserChrome->BrowserWindow(NULL);
      NS_RELEASE(mBrowserChrome);
   }
}


NS_IMETHODIMP CBrowserWindow::CommonConstruct()
{
   nsresult rv;
   
   // Make the base widget
   mWindow = do_CreateInstance(kWindowCID, &rv);
   NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);
   
   // Make our CWebBrowserChrome
   mBrowserChrome = new CWebBrowserChrome;
   NS_ENSURE_TRUE(mBrowserChrome, NS_ERROR_OUT_OF_MEMORY);
   NS_ADDREF(mBrowserChrome);
   mBrowserChrome->BrowserWindow(this);
   
   return NS_OK;
}


void CBrowserWindow::FinishCreate()
{
   // Initialize the top level widget
   // This needs to be done AFTER the subviews are constructed
   // but BEFORE the subviews do FinishCreateSelf.
   
	Rect portRect = GetMacPort()->portRect;	
	nsRect r(0, 0, portRect.right - portRect.left, portRect.bottom - portRect.top);
	
	nsresult rv = mWindow->Create((nsNativeWidget)GetMacPort(), r, nsnull, nsnull, nsnull, nsnull, nsnull);
	if (NS_FAILED(rv))
	   Throw_(NS_ERROR_GET_CODE(rv));

   Inherited::FinishCreate();   
}


// ---------------------------------------------------------------------------
//	¥ FinishCreateSelf
// ---------------------------------------------------------------------------

void CBrowserWindow::FinishCreateSelf()
{
	mBrowserShell = dynamic_cast<CBrowserShell*>(FindPaneByID(paneID_WebShellView));
	ThrowIfNULL_(mBrowserShell);  // Curtains if we don't have this
	
	// Tell our CBrowserShell about ourself 
	mBrowserShell->SetTopLevelWindow(mBrowserChrome);
	
	// Find our subviews - When we have a way of creating this
	// window with various chrome flags, we may or may not have
	// all of these subviews so don't fail if they don't exist
	mURLField = dynamic_cast<LEditText*>(FindPaneByID(paneID_URLField));
	if (mURLField)
		SwitchTarget(mURLField);

	mStatusBar = dynamic_cast<LStaticText*>(FindPaneByID(paneID_StatusBar));
	mThrobber = dynamic_cast<CThrobber*>(FindPaneByID(paneID_Throbber));
	
	mBackButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_BackButton));
	if (mBackButton)
		mBackButton->Disable();
	mForwardButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_ForwardButton));
	if (mForwardButton)
		mForwardButton->Disable();
	mStopButton = dynamic_cast<LBevelButton*>(FindPaneByID(paneID_StopButton));
	if (mStopButton)
		mStopButton->Disable();

	UReanimator::LinkListenerToControls(this, this, mUserCon);
	StartListening();
	StartBroadcasting();
	
	// Just for demo sake, load a URL	
	LStr255     urlString("http://www.mozilla.org");
	mBrowserShell->LoadURL((Ptr)&urlString[1], urlString.Length());
}


void CBrowserWindow::ResizeFrameBy(SInt16		inWidthDelta,
                				   SInt16		inHeightDelta,
                				   Boolean	    inRefresh)
{
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	
	Rect portRect = GetMacPort()->portRect;	
	mWindow->Resize(portRect.right - portRect.left, portRect.bottom - portRect.top, inRefresh);
}


void CBrowserWindow::ShowSelf()
{
   Inherited::ShowSelf();
   mWindow->Show(PR_TRUE);
}


// ---------------------------------------------------------------------------
//	¥ ListenToMessage
// ---------------------------------------------------------------------------

void CBrowserWindow::ListenToMessage(MessageT		inMessage,
									 void*			ioParam)
{
	ProcessCommand(inMessage, ioParam);
}

// ---------------------------------------------------------------------------
//	¥ ObeyCommand
// ---------------------------------------------------------------------------

Boolean CBrowserWindow::ObeyCommand(CommandT			inCommand,
									void				*ioParam)
{
#pragma unused(ioParam)

	Boolean	cmdHandled = true;
	
	switch (inCommand)
	{
		case paneID_BackButton:
			mBrowserShell->Back();
			break;

		case paneID_ForwardButton:
			mBrowserShell->Forward();
			break;

		case paneID_StopButton:
			mBrowserShell->Stop();
			break;

		case paneID_URLField:
		  	{
			SInt32    urlTextLen;
			mURLField->GetText(nil, 0, &urlTextLen);
        	StPointerBlock  urlTextPtr(urlTextLen, true, false);
			mURLField->GetText(urlTextPtr.Get(), urlTextLen, &urlTextLen);
			mBrowserShell->LoadURL(urlTextPtr.Get(), urlTextLen);
			}
			break;
			
		case CBrowserShell::cmd_Find:
			mBrowserShell->Find();
			break;

		case CBrowserShell::cmd_FindNext:
			mBrowserShell->FindNext();
			break;

	    default:
	       cmdHandled = false;
	       break;
	}

	if (!cmdHandled)
		cmdHandled = Inherited::ObeyCommand(inCommand, ioParam);
	return cmdHandled;
}


// ---------------------------------------------------------------------------
//		¥ FindCommandStatus
// ---------------------------------------------------------------------------
//	This function enables menu commands.
//  Although cmd_Find and cmd_FindNext are handled by CBrowserShell, we handle them
//  here because we want them to be enabled even when the CBrowserShell is not in
//  the target chain. 

void
CBrowserWindow::FindCommandStatus(
	PP_PowerPlant::CommandT	inCommand,
	Boolean					&outEnabled,
	Boolean					&outUsesMark,
	PP_PowerPlant::Char16	&outMark,
	Str255					outName)
{

	switch (inCommand)
	{
		case CBrowserShell::cmd_Find:
			outEnabled = true;
			break;

		case CBrowserShell::cmd_FindNext:
			outEnabled = mBrowserShell->CanFindNext();
			break;

		default:
			PP_PowerPlant::LCommander::FindCommandStatus(inCommand, outEnabled,
												outUsesMark, outMark, outName);
			break;
	}
}


NS_METHOD CBrowserWindow::GetWidget(nsIWidget** aWidget)
{
   NS_ENSURE_ARG_POINTER(aWidget);

   *aWidget = mWindow;
   NS_IF_ADDREF(*aWidget);
   
   return NS_OK;
}


//*****************************************************************************
//***    Chrome Interraction
//*****************************************************************************

NS_METHOD CBrowserWindow::SetStatus(const PRUnichar* aStatus)
{
   if (mStatusBar)
   {
		nsAutoString statusStr(aStatus);
		Str255 aStr;
		
		UMacUnicode::StringToStr255(statusStr, aStr);
		mStatusBar->SetDescriptor(aStr);
   }
   return NS_OK;
}

NS_METHOD CBrowserWindow::SetLocation(const nsString& aLocation)
{
	if (mURLField)
	{
		Str255 aStr;
		
		UMacUnicode::StringToStr255(aLocation, aStr);
		mURLField->SetDescriptor(aStr);
	}
   return NS_OK;
}

NS_METHOD CBrowserWindow::OnStatusNetStart(nsIChannel *aChannel)
{
	// Stop the throbber
	if (mThrobber)
		mThrobber->Start();

	if (mStopButton)
		mStopButton->Enable();

	// Inform any other interested parties
	// Actually, all of the above stuff should done through
	// broadcasters and listeners. But for demo's sake this
	// better shows what's happening.
	LBroadcaster::BroadcastMessage(msg_OnStartLoadDocument, 0);
   
   return NS_OK;
}

NS_METHOD CBrowserWindow::OnStatusNetStop(nsIChannel *aChannel)
{
	// Stop the throbber
	if (mThrobber)
		mThrobber->Stop();
		
	// Enable back, forward, stop
	if (mBackButton)
		mBrowserShell->CanGoBack() ? mBackButton->Enable() : mBackButton->Disable();
	if (mForwardButton)
		mBrowserShell->CanGoForward() ? mForwardButton->Enable() : mForwardButton->Disable();
	if (mStopButton)
		mStopButton->Disable();
		
	// Inform any other interested parties
	// Actually, all of the above stuff should done through
	// broadcasters and listeners. But for demo's sake this
	// better shows what's happening.
	LBroadcaster::BroadcastMessage(msg_OnEndLoadDocument, 0);

   return NS_OK;
}

NS_METHOD CBrowserWindow::OnStatusDNS(nsIChannel *aChannel)
{
   return NS_OK;
}
