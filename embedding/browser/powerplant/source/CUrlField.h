
#include <LEditText.h>


// CUrlField:
// A text edit field that broadcasts its PaneID on Return or Enter.



class CUrlField : public LEditText
{
private:
	typedef LEditText Inherited;

public:
	enum { class_ID = FOUR_CHAR_CODE('UrlF') };


						CUrlField(LStream*	inStream);

	virtual				~CUrlField();

	virtual Boolean		HandleKeyPress(
								const EventRecord	&inKeyEvent);

	virtual void		ClickSelf(
								const SMouseDownEvent	&inMouseDown);
};
