
#include <LWindow.h>
#include <LListener.h>

#ifndef nsError_h
#include "nsError.h"
#endif

#ifndef nsCom_h__
#include "nsCom.h"
#endif

#ifndef nsCOMPtr_h___
#include "nsCOMPtr.h"
#endif

#ifndef nscore_h___
#include "nscore.h"
#endif

#ifndef nsIWidget_h__
#include "nsIWidget.h"
#endif

class CBrowserShell;
class CWebBrowserChrome;
class LEditText;
class LStaticText;
class CThrobber;
class LBevelButton;
class nsIDocumentLoader;
class nsIURI;
class nsIChannel;

// CBrowserWindow:
// A simple browser window that hooks up a CBrowserShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


class CBrowserWindow :	public LWindow,
						      public LListener,
						      public LBroadcaster
{
private:
	typedef LWindow Inherited;

   friend class CWebBrowserChrome;	

public:
	enum { class_ID = FOUR_CHAR_CODE('BroW') };

						      CBrowserWindow();
						      CBrowserWindow(LStream*	inStream);

	virtual				   ~CBrowserWindow();


   virtual void         FinishCreate();
	virtual void		   FinishCreateSelf();
	virtual void		   ResizeFrameBy(SInt16		inWidthDelta,
             								  SInt16		inHeightDelta,
             								  Boolean	inRefresh);
   virtual void         ShowSelf();

	virtual void		   ListenToMessage(MessageT		inMessage,
    								             void*			ioParam);

	virtual Boolean		ObeyCommand(CommandT			inCommand,
      								      void				*ioParam);
								
  virtual void          FindCommandStatus(PP_PowerPlant::CommandT	inCommand,
                                         	Boolean					&outEnabled,
                                         	Boolean					&outUsesMark,
                                         	PP_PowerPlant::Char16	&outMark,
                                         	Str255					outName);

   NS_METHOD            GetWidget(nsIWidget** aWidget);
protected:

      // Called by both constructors
   NS_METHOD            CommonConstruct();
   
   // -----------------------------------
   // Methods called by CWebBrowserChrome
   // -----------------------------------
   
   NS_METHOD            SetStatus(const PRUnichar* aStatus);
   NS_METHOD            SetOverLink(const PRUnichar* aStatus)
                        { return SetStatus(aStatus); }
                        
   NS_METHOD            SetLocation(const nsString& aLocation);
                        
   NS_METHOD				BeginDocumentLoad(nsIDocumentLoader *aLoader, nsIURI *aURL, const char *aCommand);
   NS_METHOD				EndDocumentLoad(nsIDocumentLoader *loader, nsIChannel *aChannel, PRUint32 aStatus);

	NS_METHOD            OnStatusNetStart(nsIChannel *aChannel);
	NS_METHOD            OnStatusNetStop(nsIChannel *aChannel);
	NS_METHOD            OnStatusDNS(nsIChannel *aChannel);
   
protected:
   nsCOMPtr<nsIWidget>  mWindow;

	CBrowserShell*		   mBrowserShell;
	CWebBrowserChrome*   mBrowserChrome;
	LEditText*			   mURLField;
	LStaticText*		   mStatusBar;
	CThrobber*           mThrobber;
	LBevelButton			*mBackButton, *mForwardButton, *mStopButton;
};


