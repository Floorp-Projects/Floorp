
#include <LWindow.h>
#include <LListener.h>

class CWebShell;
class LEditText;
class LStaticText;


// CBrowserWindow:
// A simple browser window that hooks up a CWebShell to a minimal set of controls
// (Back, Forward and Stop buttons + URL field + status bar).


const ResIDT	wind_SampleBrowserWindow = 128;


class CBrowserWindow :	public LWindow,
						public LListener
{
private:
	typedef LWindow Inherited;

public:
	enum { class_ID = FOUR_CHAR_CODE('BroW') };

						CBrowserWindow();
						CBrowserWindow(LStream*	inStream);

	virtual				~CBrowserWindow();


	virtual void		FinishCreateSelf();
	virtual void		ListenToMessage(
								MessageT		inMessage,
								void*			ioParam);

	virtual Boolean		ObeyCommand(
								CommandT			inCommand,
								void				*ioParam);

protected:
	CWebShell*			mWebShellView;
	LEditText*			mURLField;
	LStaticText*		mStatusBar;
};


