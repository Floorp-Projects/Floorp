

#include "CWebShell.h"
#include "nsIWidget.h"
#include "nsIWebShell.h"
#include "nsWidgetsCID.h"
#include "nsString.h"
#include "nsIComponentManager.h"
#include "nsRepeater.h"
#include "nsIImageManager.h"
#include "nsISupports.h"
#include "nsIURL.h"


// CWebShell:
// A view that embeds a Raptor WebShell. It creates the WebShell and
// dispatches the OS events to it.


// IMPORTANT:
// - ее root control warning ее
// - Erase on Update should be off


nsMacMessageSink	CWebShell::mMessageSink;

static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
static NS_DEFINE_IID(kWindowCID, NS_WINDOW_CID);
static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);

extern "C" void NS_SetupRegistry();


// ---------------------------------------------------------------------------
//	е CWebShell								Default Constructor		  [public]
// ---------------------------------------------------------------------------

CWebShell::CWebShell()
{
	mURL[0] = 0;
	Init();
}



// ---------------------------------------------------------------------------
//	е CWebShell								Stream Constructor		  [public]
// ---------------------------------------------------------------------------

CWebShell::CWebShell(LStream*	inStream)
	: LView(inStream)
{
	*inStream >> (StringPtr) mURL;
	Init();
}


// ---------------------------------------------------------------------------
//	е ~CWebShell							Destructor				  [public]
// ---------------------------------------------------------------------------

CWebShell::~CWebShell()
{
	if (mWebShell)
		delete mWebShell;

	if (mWindow)
		delete mWindow;
}


// ---------------------------------------------------------------------------
//	е Init
// ---------------------------------------------------------------------------

void
CWebShell::Init()
{
	// Initialize the Registry
	// Note: This can go away when Auto-Registration is implemented in all the Raptor DLLs.
	static Boolean	gFirstTime = true;
	if (gFirstTime)
	{
		gFirstTime = false;
		NS_SetupRegistry();
		nsIImageManager *manager;
		NS_NewImageManager(&manager);
	}

	mWindow = nil;
	mWebShell = nil;
	mThrobber = nil;
	mStatusBar = nil;
	mLoading = false;

	// set the QuickDraw origin
	FocusDraw();

	// create top-level widget
	nsresult rv;
	rv = nsComponentManager::CreateInstance(kWindowCID, nsnull, kIWidgetIID, (void**)&mWindow);
	if (rv != NS_OK)
		return;

	nsWidgetInitData initData;
	Rect portRect = GetMacPort()->portRect;
	nsRect r(portRect.left, portRect.top, portRect.right - portRect.left, portRect.bottom - portRect.top);

	rv = mWindow->Create((nsNativeWidget)GetMacPort(), r, nsnull, nsnull);
	if (rv != NS_OK)
		return;
	mWindow->Show(PR_TRUE);

	// create webshell
	rv = nsComponentManager::CreateInstance(kWebShellCID, nsnull, kIWebShellIID, (void**)&mWebShell);
	if (rv != NS_OK)
		return;

	Rect webRect;
	CalcPortFrameRect(webRect);
	r.SetRect(webRect.left, webRect.top, webRect.right - webRect.left, webRect.bottom - webRect.top);
	PRBool allowPlugins = PR_FALSE;
	rv = mWebShell->Init(mWindow->GetNativeData(NS_NATIVE_WIDGET), 
                       r.x, r.y, r.width, r.height,
                       nsScrollPreference_kNeverScroll, //nsScrollPreference_kAuto, 
                       allowPlugins, PR_FALSE);
	if (rv != NS_OK)
		return;
	mWebShell->SetContainer((nsIWebShellContainer*)this);
	mWebShell->SetObserver((nsIStreamObserver*)this);
	mWebShell->Show();
}


// ---------------------------------------------------------------------------
//	е FinishCreateSelf
// ---------------------------------------------------------------------------

void
CWebShell::FinishCreateSelf()
{
	StartRepeating();

	// load the default page
	nsString urlString;
	urlString.SetString((char*)&mURL[1], mURL[0]);
	mWebShell->LoadURL(urlString.GetUnicode());
}

// ---------------------------------------------------------------------------
//	е ResizeFrameBy
// ---------------------------------------------------------------------------

void
CWebShell::ResizeFrameBy(
	SInt16		inWidthDelta,
	SInt16		inHeightDelta,
	Boolean		inRefresh)
{
	Inherited::ResizeFrameBy(inWidthDelta, inHeightDelta, inRefresh);
	HandleMoveOrResize();
}


// ---------------------------------------------------------------------------
//	е MoveBy
// ---------------------------------------------------------------------------

void
CWebShell::MoveBy(
	SInt32		inHorizDelta,
	SInt32		inVertDelta,
	Boolean		inRefresh)
{
	Inherited::MoveBy(inHorizDelta, inVertDelta, inRefresh);
	HandleMoveOrResize();
}


// ---------------------------------------------------------------------------
//	е HandleMoveOrResize
// ---------------------------------------------------------------------------

void
CWebShell::HandleMoveOrResize()
{
	if (mWebShell && mWindow)
	{
		// set the QuickDraw origin
		FocusDraw();

		// resize the top-level widget and the webshell
		Rect portRect = GetMacPort()->portRect;
		mWindow->Resize(portRect.right - portRect.left, portRect.bottom - portRect.top, PR_FALSE);

		Rect webRect;
		CalcPortFrameRect(webRect);
		nsRect r(webRect.left, webRect.top, webRect.right - webRect.left, webRect.bottom - webRect.top);
		mWebShell->SetBounds(r.x, r.y, r.width, r.height);
	}
}


// ---------------------------------------------------------------------------
//	е DrawSelf
// ---------------------------------------------------------------------------

void
CWebShell::DrawSelf()
{
	EventRecord osEvent;
	osEvent.what = updateEvt;
	mMessageSink.DispatchOSEvent(osEvent, GetMacPort());
}


// ---------------------------------------------------------------------------
//	е ActivateSelf											   [protected]
// ---------------------------------------------------------------------------

void
CWebShell::ActivateSelf()
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	EventRecord osEvent;
	osEvent.what = activateEvt;
	osEvent.modifiers = activeFlag;
	mMessageSink.DispatchOSEvent(osEvent, GetMacPort());
}


// ---------------------------------------------------------------------------
//	е DeactivateSelf										   [protected]
// ---------------------------------------------------------------------------

void
CWebShell::DeactivateSelf()
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	EventRecord osEvent;
	osEvent.what = activateEvt;
	osEvent.modifiers = 0;
	mMessageSink.DispatchOSEvent(osEvent, GetMacPort());
}


// ---------------------------------------------------------------------------
//	е ClickSelf
// ---------------------------------------------------------------------------

void
CWebShell::ClickSelf(
	const SMouseDownEvent	&inMouseDown)
{
	if (!IsTarget())
		SwitchTarget(this);

	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	mMessageSink.DispatchOSEvent((EventRecord&)inMouseDown.macEvent, GetMacPort());
}


// ---------------------------------------------------------------------------
//	е EventMouseUp
// ---------------------------------------------------------------------------

void
CWebShell::EventMouseUp(
	const EventRecord	&inMacEvent)
{
	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	mMessageSink.DispatchOSEvent((EventRecord&)inMacEvent, GetMacPort());
}


// ---------------------------------------------------------------------------
//	е FocusDraw
// ---------------------------------------------------------------------------
// Set the origin to (0, 0) and the clipRect to our port frame rect
// Mostly stolen from LView::FocusDraw().

Boolean
CWebShell::FocusDraw(
	LPane*	/* inSubPane */)
{
									// Check if revealed rect is empty
	Boolean	revealed = (mRevealedRect.left < mRevealedRect.right);
	
	if (this != sInFocusView) {		// Skip if already in focus
		if (EstablishPort()) {		// Set current Mac Port		
				
										// Set up local coordinate system
//			::SetOrigin(mPortOrigin.h, mPortOrigin.v);
			::SetOrigin(0, 0);
			
										// Clip to revealed area of View
			Rect	clippingRect = mRevealedRect;
//			PortToLocalPoint(topLeft(clippingRect));
//			PortToLocalPoint(botRight(clippingRect));
			::ClipRect(&clippingRect);
			
			sInFocusView = this;		// Cache current Focus
			
		} else {
			SignalPStr_("\pFocus View with no GrafPort");
		}
	}
	
	return revealed;
}


// ---------------------------------------------------------------------------
//	е AdjustCursor
// ---------------------------------------------------------------------------
void
CWebShell::AdjustCursor(
	Point				/*inPortPt*/,
	const EventRecord	&/*inMacEvent*/)
{
}


// ---------------------------------------------------------------------------
//	е DontBeTarget
// ---------------------------------------------------------------------------
void
CWebShell::DontBeTarget()
{
	if (mWebShell)
	{
		// set the QuickDraw origin
		FocusDraw();

		// tell form controls that we are losing the focus
		mWebShell->RemoveFocus();
	}
}


// ---------------------------------------------------------------------------
//	е HandleKeyPress
// ---------------------------------------------------------------------------

Boolean
CWebShell::HandleKeyPress(
	const EventRecord	&inKeyEvent)
{
	Boolean keyHandled = true;

	// set the QuickDraw origin
	FocusDraw();

	// dispatch the event
	keyHandled = mMessageSink.DispatchOSEvent((EventRecord&)inKeyEvent, GetMacPort());

	return keyHandled;
}


// ---------------------------------------------------------------------------
//	е SpendTime
// ---------------------------------------------------------------------------

void
CWebShell::SpendTime(
	const EventRecord& inMacEvent)
{
	if (inMacEvent.what == osEvt || inMacEvent.what == nullEvent)
	{
		// set the QuickDraw origin
		FocusDraw();

		// dispatch the event
		mMessageSink.DispatchOSEvent((EventRecord&)inMacEvent, GetMacPort());
	}

	// Enable Raptor background activity
	// Note: This will be moved to nsMacMessageSink very soon.
	// The application will not have to do it anymore.
	Repeater::DoRepeaters(inMacEvent);
	if (inMacEvent.what == nullEvent)
		Repeater::DoIdlers(inMacEvent);
}

#pragma mark -


// ---------------------------------------------------------------------------
//	е Back
// ---------------------------------------------------------------------------

void
CWebShell::Back()
{
	if (mWebShell && (mWebShell->CanBack() == NS_OK))
		mWebShell->Back();
	else
		::SysBeep(1);
}


// ---------------------------------------------------------------------------
//	е Forward
// ---------------------------------------------------------------------------

void
CWebShell::Forward()
{
	if (mWebShell && (mWebShell->CanForward() == NS_OK))
		mWebShell->Forward();
	else
		::SysBeep(1);
}


// ---------------------------------------------------------------------------
//	е Forward
// ---------------------------------------------------------------------------

void
CWebShell::Stop()
{
	if (mWebShell && mLoading)
		mWebShell->Stop();
	else
		::SysBeep(1);
}


// ---------------------------------------------------------------------------
//	е LoadURL
// ---------------------------------------------------------------------------

void
CWebShell::LoadURL(LStr255& urlString)
{
	if (mWebShell)
	{
		mURL = urlString;
		nsString nsURLString;
		nsURLString.SetString((char*)&mURL[1], mURL[0]);
		mWebShell->LoadURL(nsURLString.GetUnicode());
	}
	else
		::SysBeep(1);
}


// ---------------------------------------------------------------------------
//	е DisplayStatus
// ---------------------------------------------------------------------------

void
CWebShell::DisplayStatus(const PRUnichar* status)
{
	if (mStatusBar)
	{
		nsAutoString statusStr = status;

		Str255 aStr;
		aStr[0] = min(statusStr.Length(), 254);
		statusStr.ToCString((char*)&aStr[1], aStr[0] + 1);
		mStatusBar->SetDescriptor(aStr);

		FocusDraw();
	}
}

#pragma mark -


// ---------------------------------------------------------------------------
//	е OnStartRequest
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::OnStartRequest(nsIChannel* /*channel*/, nsISupports */*ctxt*/)
{
	return NS_OK;
}


// ---------------------------------------------------------------------------
//	е OnProgress
// ---------------------------------------------------------------------------
/*
NS_IMETHODIMP CWebShell::OnProgress(nsIURL* aURL, PRUint32 aProgress, PRUint32 aProgressMax)
{
	if (mLoading)
	{
		nsAutoString url;
		if (aURL)
		{
			PRUnichar* str;
			aURL->ToString(&str);
			url = str;
			delete[] str;
		}
		url.Append(": progress ");
		url.Append(aProgress, 10);
		if (0 != aProgressMax)
		{
			url.Append(" (out of ");
			url.Append(aProgressMax, 10);
			url.Append(")");
		}
		DisplayStatus(url.GetUnicode());
	}
	return NS_OK;
}
*/

// ---------------------------------------------------------------------------
//	е OnStatus
// ---------------------------------------------------------------------------
/*
NS_IMETHODIMP CWebShell::OnStatus(nsIURL* aURL, const PRUnichar* aMsg)
{
#pragma unused (aURL)
	DisplayStatus(aMsg);
	return NS_OK;
}
*/

// ---------------------------------------------------------------------------
//	е OnStopRequest
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::OnStopRequest(nsIChannel* /*channel*/, nsISupports */*ctxt*/, nsresult /*status*/, const PRUnichar */*errorMsg*/)
{
	return NS_OK;
}

#pragma mark -


// ---------------------------------------------------------------------------
//	е WillLoadURL
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::WillLoadURL(nsIWebShell* /*aShell*/,
									const PRUnichar* aURL,
									nsLoadType /*aReason*/)
{
	nsAutoString url("Connecting to ");
	url.Append(aURL);

	DisplayStatus(url.GetUnicode());
  return NS_OK;
}


// ---------------------------------------------------------------------------
//	е BeginLoadURL
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::BeginLoadURL(nsIWebShell* /*aShell*/,
									const PRUnichar* aURL)
{
	mLoading = true;

	if (mThrobber)
		mThrobber->Show();

	DisplayStatus(aURL);
	return NS_OK;
}


// ---------------------------------------------------------------------------
//	е ProgressLoadURL
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::ProgressLoadURL(nsIWebShell* /*aShell*/,
									const PRUnichar* /*aURL*/,
									PRInt32 /*aProgress*/,
									PRInt32 /*aProgressMax*/)
{
	return NS_OK;
}


// ---------------------------------------------------------------------------
//	е EndLoadURL
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::EndLoadURL(nsIWebShell* /*aShell*/,
									const PRUnichar* /*aURL*/,
									nsresult /*aStatus*/)
{
	mLoading = false;

	if (mThrobber)
		mThrobber->Hide();

    nsAutoString msg("Done.");
	DisplayStatus(msg.GetUnicode());
	return NS_OK;
}


// ---------------------------------------------------------------------------
//	е NewWebShell
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::NewWebShell(PRUint32 /*aChromeMask*/,
									PRBool /*aVisible*/,
									nsIWebShell *&aNewWebShell)
{
	aNewWebShell = nsnull;
	return NS_ERROR_FAILURE;
}


// ---------------------------------------------------------------------------
//	е FindWebShellWithName
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::FindWebShellWithName(const PRUnichar* /*aName*/,
											nsIWebShell*& aResult)
{
	aResult = nsnull;
	return NS_ERROR_FAILURE;
}


// ---------------------------------------------------------------------------
//	е ContentShellAdded
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::ContentShellAdded(nsIWebShell* /*aChildShell*/, nsIContent* /*frameNode*/)
{
	return NS_ERROR_FAILURE;
}


// ---------------------------------------------------------------------------
//	е CreatePopup
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup)
{
#pragma unused (aElement)
#pragma unused (aPopupContent)
#pragma unused (aXPos)
#pragma unused (aYPos)
#pragma unused (aPopupType)
#pragma unused (anAnchorAlignment)
#pragma unused (aPopupAlignment)
#pragma unused (aWindow)
	*outPopup = nsnull;
	return NS_ERROR_FAILURE;
}



// ---------------------------------------------------------------------------
//	е FocusAvailable
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::FocusAvailable(nsIWebShell* /*aFocusedWebShell*/, PRBool& aFocusTaken)
{
	aFocusTaken = PR_FALSE;
	return NS_OK;
}


// ---------------------------------------------------------------------------
//	е CanCreateNewWebShell
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::CanCreateNewWebShell(PRBool& aResult)
{
	aResult = PR_FALSE;
	return NS_ERROR_FAILURE;
}


// ---------------------------------------------------------------------------
//	е CanCreateNewWebShell
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::SetNewWebShellInfo(const nsString& /*aName*/, const nsString& /*anURL*/, 
                                nsIWebShell* /*aOpenerShell*/, PRUint32 /*aChromeMask*/,
                                nsIWebShell** /*aNewShell*/, nsIWebShell** /*anInnerShell*/)
{
	return NS_ERROR_FAILURE;
}


// ---------------------------------------------------------------------------
//	е ChildShellAdded
// ---------------------------------------------------------------------------

NS_IMETHODIMP CWebShell::ChildShellAdded(nsIWebShell** /*aChildShell*/, nsIContent* /*frameNode*/)
{
	return NS_ERROR_FAILURE;
}


#pragma mark -
// ---------------------------------------------------------------------------
//	е QueryInterface
// ---------------------------------------------------------------------------

static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kIWebShellContainerIID, NS_IWEB_SHELL_CONTAINER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);


NS_IMPL_ADDREF(CWebShell);
NS_IMPL_RELEASE(CWebShell);

nsresult
CWebShell::QueryInterface(const nsIID& aIID, void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  *aInstancePtrResult = NULL;

  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kIWebShellContainerIID)) {
    *aInstancePtrResult = (void*) ((nsIWebShellContainer*)this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIWebShellContainer*)this));
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}
