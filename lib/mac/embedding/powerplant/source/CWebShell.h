
#include <LPane.h>
#include <LView.h>
#include <LCommander.h>
#include <LPeriodical.h>
#include "nsMacMessageSink.h"
#include "nsIWebShell.h"
#include "nsIDocumentLoaderObserver.h"


class nsIWidget;
class nsIWebShell;


class CWebShell :	public LView,
					public LCommander,
					public LPeriodical,
					public nsIDocumentLoaderObserver,
					public nsIWebShellContainer
{
private:
	typedef LView Inherited;

public:
	enum { class_ID = FOUR_CHAR_CODE('WebS') };

						CWebShell();
						CWebShell(LStream*	inStream);

	virtual				~CWebShell();

	// LPane
	virtual void		FinishCreateSelf();
	virtual void		ResizeFrameBy(
								SInt16		inWidthDelta,
								SInt16		inHeightDelta,
								Boolean		inRefresh);
	
	virtual void		MoveBy(	SInt32		inHorizDelta,
								SInt32		inVertDelta,
								Boolean		inRefresh);
	virtual void		DrawSelf();	
	virtual void		ClickSelf(
								const SMouseDownEvent	&inMouseDown);
	virtual void		EventMouseUp(
								const EventRecord	&inMacEvent);

	// LView
	virtual Boolean		FocusDraw(
								LPane				*inSubPane = nil);

	virtual void		AdjustCursor(
								Point				inPortPt,
								const EventRecord	&inMacEvent);


	// LCommander
	virtual void		DontBeTarget();
	virtual Boolean		HandleKeyPress(
								const EventRecord	&inKeyEvent);

	// LPeriodical
	virtual	void		SpendTime(
								const EventRecord&		inMacEvent);

	// CWebShell
	void				GetURLString(LStr255& urlString) {urlString = mURL;}
	virtual void		Back();
	virtual void		Forward();
	virtual void		Stop();
	virtual void		LoadURL(LStr255& urlString);
	virtual void		SetThrobber(LPane* throbber)	{mThrobber = throbber;}
	virtual void		SetStatusBar(LPane* statusBar)	{mStatusBar = statusBar;}
	virtual void		DisplayStatus(const PRUnichar* status);


	// nsISupports
	NS_DECL_ISUPPORTS

	// nsIDocumentLoaderObserver
  NS_IMETHOD OnStartDocumentLoad(nsIDocumentLoader* /*loader*/, nsIURI* /*aURL*/, const char* /*aCommand*/)
  													{ return NS_ERROR_NOT_IMPLEMENTED; /* XXX TBI */};

  NS_IMETHOD OnEndDocumentLoad(nsIDocumentLoader* /*loader*/, nsIChannel* /*channel*/, nsresult /*aStatus*/)
														{ return NS_ERROR_NOT_IMPLEMENTED; /* XXX TBI */};

  NS_IMETHOD OnStartURLLoad(nsIDocumentLoader* /*loader*/, nsIChannel* /*channel*/)
                            { return NS_ERROR_NOT_IMPLEMENTED; /* XXX TBI */};

  NS_IMETHOD OnProgressURLLoad(nsIDocumentLoader* loader,
                            nsIChannel* channel, PRUint32 aProgress, 
                          	PRUint32 aProgressMax);

  NS_IMETHOD OnStatusURLLoad(nsIDocumentLoader* loader, nsIChannel* channel, nsString& aMsg);

  NS_IMETHOD OnEndURLLoad(nsIDocumentLoader* /*loader*/, nsIChannel* /*channel*/, nsresult /*aStatus*/)
  													{ return NS_ERROR_NOT_IMPLEMENTED; /* XXX TBI */ };


	// nsIWebShellContainer
	NS_IMETHOD WillLoadURL(nsIWebShell* aShell,
							const PRUnichar* aURL,
							nsLoadType aReason);

	NS_IMETHOD BeginLoadURL(nsIWebShell* aShell,
							const PRUnichar* aURL);

	NS_IMETHOD ProgressLoadURL(nsIWebShell* aShell,
							const PRUnichar* aURL,
							PRInt32 aProgress,
							PRInt32 aProgressMax);

	NS_IMETHOD EndLoadURL(nsIWebShell* aShell,
							const PRUnichar* aURL,
							nsresult aStatus);

	NS_IMETHOD NewWebShell(PRUint32 aChromeMask,
							PRBool aVisible,
							nsIWebShell *&aNewWebShell);

	NS_IMETHOD FindWebShellWithName(const PRUnichar* aName,
							nsIWebShell*& aResult);

	NS_IMETHOD ContentShellAdded(nsIWebShell* aChildShell, nsIContent* frameNode);

	NS_IMETHOD CreatePopup(nsIDOMElement* aElement, nsIDOMElement* aPopupContent, 
                         PRInt32 aXPos, PRInt32 aYPos, 
                         const nsString& aPopupType, const nsString& anAnchorAlignment,
                         const nsString& aPopupAlignment,
                         nsIDOMWindow* aWindow, nsIDOMWindow** outPopup);

	NS_IMETHOD FocusAvailable(nsIWebShell* aFocusedWebShell, PRBool& aFocusTaken);
	NS_IMETHOD CanCreateNewWebShell(PRBool& aResult);
	NS_IMETHOD SetNewWebShellInfo(const nsString& aName, const nsString& anURL, 
                                nsIWebShell* aOpenerShell, PRUint32 aChromeMask,
                                nsIWebShell** aNewShell, nsIWebShell** anInnerShell);
	NS_IMETHOD ChildShellAdded(nsIWebShell** aChildShell, nsIContent* frameNode);


protected:
	// LPane
	virtual void		ActivateSelf();
	virtual void		DeactivateSelf();

	// CWebShell
	virtual void		Init();
	virtual void		HandleMoveOrResize();

public:

protected:
	nsIWidget*			mWindow;
	nsIWebShell*		mWebShell;

	LStr255				mURL;
	LPane*				mThrobber;
	LPane*				mStatusBar;
	Boolean				mLoading;

	static nsMacMessageSink	mMessageSink;

};


