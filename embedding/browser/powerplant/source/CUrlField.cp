
#include "CUrlField.h"
#include <LString.h>


// CUrlField:
// A text edit field that broadcasts its PaneID on Return or Enter.




// ---------------------------------------------------------------------------
//	¥ CUrlField								Default Constructor		  [public]
// ---------------------------------------------------------------------------

CUrlField::CUrlField()
{
}



// ---------------------------------------------------------------------------
//	¥ CUrlField								Stream Constructor		  [public]
// ---------------------------------------------------------------------------

CUrlField::CUrlField(LStream*	inStream)
	: LEditText(inStream)
{
}


// ---------------------------------------------------------------------------
//	¥ ~CUrlField							Destructor				  [public]
// ---------------------------------------------------------------------------

CUrlField::~CUrlField()
{
}


// ---------------------------------------------------------------------------
//	¥ HandleKeyPress
// ---------------------------------------------------------------------------
//	Broadcast the paneID when the user hits Return or Enter

Boolean
CUrlField::HandleKeyPress(const EventRecord	&inKeyEvent)
{
	Boolean		keyHandled = true;
	Char16		theChar = (Char16) (inKeyEvent.message & charCodeMask);

	if (theChar == char_Return || theChar == char_Enter)
	{
		Str255 urlString;
		BroadcastMessage(GetPaneID(), (void*)GetDescriptor(urlString));
	}
	else
		keyHandled = Inherited::HandleKeyPress(inKeyEvent);

	return keyHandled;
}


// ---------------------------------------------------------------------------
//	¥ ClickSelf
// ---------------------------------------------------------------------------
// Select everything when a single click gives us the focus

void
CUrlField::ClickSelf(const SMouseDownEvent	&inMouseDown)
{
	Boolean wasTarget = IsTarget();

	Inherited::ClickSelf(inMouseDown);

	if (!wasTarget)
	{
		ControlEditTextSelectionRec	selection;
		GetSelection(selection);
		if (selection.selStart == selection.selEnd)
			SelectAll();
	}
}

