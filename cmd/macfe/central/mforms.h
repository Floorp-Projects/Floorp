/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

// mforms.h - Defines form views


#include <LButton.h>
#include <LCaption.h>
#include <LPane.h>
#include <LStdControl.h>
#include <LListBox.h>
#include <LTextEdit.h>
#include <LGARadioButton.h>
#include <LGACheckbox.h>
#include <LGAPushButton.h>

#include <LAttachment.h> // mjc

#include "CSimpleTextView.h"	// For CFormBigText
//#include "VEditField.h"
#include "CTSMEditField.h"

#include "CKeyUpReceiver.h" // mixin for forms that need to receive key up events

#include "PascalString.h"
#include "cstring.h"
#include "PopupBox.h"

#include "libmocha.h"

// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// CLayerClickDispatchAttachment
//
// Attached to a form view to intercept the click and dispatch to layers.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class CLayerClickDispatchAttachment : public LAttachment
{
	public:
							CLayerClickDispatchAttachment();
		
	protected:

		virtual void		ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);
				
		virtual void		MouseDown(const SMouseDownEvent	&inMouseDown);
		
		LControl*			mOwningControl;
};


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// CLayerClickCallbackMixin
//
// Abstract class which a form view must implement if it has a CLayerClickDispatchAttachment.
// ClickSelfLayer is called from the html view when it receives a layer event which
// is targeted at a form.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
class CLayerClickCallbackMixin
{
	public:
		virtual void		ClickSelfLayer(const SMouseDownEvent &inMouseDown) = 0;
};


// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ
// CLayerKeyPressDispatchAttachment
//
// Attached to a form view to intercept the keypress and dispatch to layers.
// ÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑÑ

class LFormElement;

class CLayerKeyPressDispatchAttachment : public LAttachment
{
	public:
							CLayerKeyPressDispatchAttachment();
		
	protected:

		virtual void		ExecuteSelf(
									MessageT			inMessage,
									void*				ioParam);
				
		virtual void		KeyPress(const EventRecord& inKeyEvent);
		
		LFormElement*		mOwner;
};

class StSetupFormDrawing
{
private:
	LPane * fForm;	// We assume that form's superview is CHyperView
	RGBColor fFore;	// Old foreground
	RGBColor fBack;	// Old background
public:
	StSetupFormDrawing(LPane * formElement);
	~StSetupFormDrawing();
};

class LFormElement;

/*****************************************************************************
 * FE_Data structure in LO_FormElementStruct
 *****************************************************************************/
struct FormFEData	{
	LPane 			* fPane;
	LFormElement	* fHost;
	LCommander		* fCommander;
	
	FormFEData();
	~FormFEData();
};



class LFormElement 
{
public:	
	LFormElement();
	virtual			~LFormElement();
	virtual void	InitFormElement(MWContext * context, LO_FormElementStruct *lo_element);
	
	// Set to true for non-focusable elements so that their state changes
	// are appropriately reflected
	//
	void			SetReflectOnChange(Boolean reflect) { fReflectOnChange = reflect; }
	
	void			SetLayoutElement(LO_FormElementStruct *lo_element) { fLayoutElement = (LO_Element *) lo_element; }
	LO_Element*		GetLayoutElement() { return fLayoutElement; } // mjc
	MWContext*		GetContext() { return fContext; }
	int16			GetWinCSID() const;
	
	virtual void	ReflectData();	// reflect to XP data structures
	
	virtual void 	MochaFocus(Boolean doFocus, Boolean switchTarget=true);
	// 97-06-14 pkc -- virtual callback called from mocha focus callback
	virtual void	PostMochaFocusCallback() {}
		
	virtual void 	MochaSelect();
	virtual void	MochaChanged();
	virtual void	MochaSubmit();
	virtual void	SetFEData( FormFEData * data ) { fFEData = data; if (data) fPane = data->fPane; }
	
	void			MarkForDeath();
	inline Boolean	IsMarkedForDeath()	{ return fMarkedForDeath; }
	
	void			ClearInFocusCallAlready() { fInFocusCallAlready = false; }

	static inline LFormElement*	GetTargetedElement() { return sTargetedFormElement; }

	void*		fTimer;

protected:
	LPane	   *fPane;		// the pane that represents this element
	LO_Element *fLayoutElement;
	FormFEData *fFEData;	// Same thing pointed to by FEDATAPANE
							// We need a private copy because we might need to access
							// it after fLayoutElement has been freed
	MWContext  *fContext;
	
	unsigned	fMochaChanged 		: 1;
	unsigned	fReflectOnChange	: 1;	// for elements that don't focus
	unsigned	fInFocusCallAlready	: 1;
	unsigned	fMarkedForDeath		: 1;
	
	Boolean		fPreviousFocus;
	static LFormElement *sTargetedFormElement;
};


// This is a stack based class that tells the currently focused form element
// that it has lost focus and then restores focus to it when destructed.
//
// This is needed because an action may invoke a mocha script that relies
// on the current value of a focussed form field. But form field values
// aren't reflected until focus is lost.  So we fake up losing focus.
//
// For example, the user changes a text field, and clicks on a button 
// that invokes a script that uses the field's value.  Because the field
// doesn't lose focus when the button is clicked, the value the script 
// sees could be wrong.  So when clicking on the button, we must fake
// the loss of focus for the text field.
//
// We do this rather than just reflect the focussed element's value directly
// because other platforms actually transfer focus in these cases.
//
class StTempFormBlur
{
public:
	StTempFormBlur();
	~StTempFormBlur();

private:

	LFormElement*	fSavedFocus;
};



/*****************************************************************************
 * class CWhiteEditField
 * Draws on a white background
 *****************************************************************************/
class CWhiteEditField : public CTSMEditField, public LFormElement {
public:
	CWhiteEditField(LStream *inStream);
	
	virtual void	UserChangedText();	// to call MochaChanged
	
	
	virtual Boolean HandleKeyPress(const EventRecord& inKeyEvent);

protected:
	virtual void	BeTarget();			// to call MochaFocus
	virtual void	DontBeTarget();		// to call MochaFocus
	// Draws background to white
	void DrawSelf();
	virtual void	ClickSelf(const	SMouseDownEvent& event);	// to call MochaSelect
		
};

/*****************************************************************************
 * class CFormLittleText
 * EditField that broadcasts msg_SubmitText when return is pressed inside it.
 * As with all forms, ioParam of the broadcast is 'this'
 * Does not broadcast by default, must call SetBroadcast (just like layout interface)
 *****************************************************************************/
class CFormLittleText: public CWhiteEditField, public LBroadcaster, public CKeyUpReceiver
{
public:
								enum { class_ID = 'ftxt' };

								CFormLittleText( LStream* inStream );

	virtual void				FindCommandStatus(
									CommandT			inCommand,
									Boolean				&outEnabled,
									Boolean				&outUsesMark,
									Char16				&outMark,
									Str255				outName);
	
	// ¥¥ Form interface
	void						SetBroadcast( Boolean doBroadcast ) { fDoBroadcast = doBroadcast; }
	void						SetLayoutForm( LO_FormElementStruct* form )	{ fLOForm = form; }
	LO_FormElementStruct*		GetLayoutForm() { return fLOForm; }
	
	// Sets the number of visible characters, resizes accordingly
	void						SetVisibleChars( Int16 visChars );	
	// Broadcast 'this'
	virtual void				BroadcastValueMessage();
	// Broadcast msg_EditSubmit on return
	virtual Boolean				HandleKeyPress(const EventRecord& inKeyEvent);
	virtual StringPtr	GetDescriptor(
								Str255				outDescriptor) const;
	virtual void		SetDescriptor(
								ConstStr255Param	inDescriptor);
protected:
	virtual void				BeTarget();

	LO_FormElementStruct*		fLOForm;			// Layout form*
	Boolean						fDoBroadcast;		// True if we should broadcast 
};

/*****************************************************************************
 * class CFormBigText
 * Large edit text field. Draws full white background;
 *****************************************************************************/

class CFormBigText : public CSimpleTextView, public LFormElement, public CKeyUpReceiver {
public:
	enum { class_ID = 'fbtx' };

// ¥¥ Constructors/destructors
	CFormBigText(LStream *inStream);
	
	virtual	~CFormBigText();
	
// ¥¥ Misc
	virtual Boolean ObeyCommand(CommandT inCommand, void *ioParam);
	
	virtual Boolean		FocusDraw(LPane* inSubPane = nil);
	
	virtual Boolean	HandleKeyPress(const EventRecord& inKeyEvent);
	// we hook into the text engine as an attachment so that we know when the text changes
//	virtual void	ListenToMessage(MessageT inMessage, void *ioParam);	// to call MochaChanged
	
	virtual void	UserChangedText(); //  to call MochaChanged
	
	virtual void	ClickSelf(const	SMouseDownEvent& event);			// to call MochaSelect
	virtual void 	DontBeTarget();										// MochaFocus
	
	//virtual Boolean HandleKeyPress(const EventRecord& inKeyEvent);

protected:
	virtual void BeTarget();
	void DrawSelf();// Draws background to white
};

/*****************************************************************************
 * class CFormList
 * List used in forms.
 * A wrapper around very bare PPlant's LListBox
 * Has functions for simple element addition, and resizing
 ****************************************************************************/
class CFormList : public LListBox, public LFormElement {
	Int32	fLongestItem;		// String length of the longest item
public:
	enum { class_ID = 'flst' };

// ¥¥ Constructors/destructors

	CFormList(LStream * inStream);
	~CFormList();

// ¥¥ Forms interface
	void SetSelectMultiple(Boolean multipleSel);
	
	// Add an item to the list - must call AddRow first
	void AddItem (const cstring& itemName, SInt16 inRow);
	
	// Add rows
	void SyncRows( UInt16 nRows );
	
	void SetTextTraits();
	
	// Selecting items
	void SetSelect(int item, Boolean selected);
	void SelectNone();
	
	// Are they selected
	Boolean IsSelected(int item);
	// Resizes the list to the width of the longest item, and visRows
	void ShrinkToFit(Int32 visRows);
// ¥¥ drawing, making sure that background is white
	virtual Boolean		FocusDraw(LPane* inSubPane = nil);	// Sets background to white
	
	
	virtual Boolean	HandleKeyPress(const EventRecord	&inKeyEvent);
	virtual void	ClickSelf(const SMouseDownEvent& event);
	
protected:
	virtual void	BeTarget();		// MochaFocus
	virtual void	DontBeTarget();	// MochaFocus
	virtual void	DrawSelf();
	void			MakeSelection( LArray& list );
	Boolean			CheckSelection( LArray& list );
	virtual void	ActivateSelf();
	virtual void	DeactivateSelf();
	
protected:	//	Added from CCustomListBox (Developer 21)
	virtual void 		DrawElement (const short lMessage, const Boolean lSelect, const Rect *lRect,
							         const void *lElement, const short lDataLen) ;
	virtual void		DrawElementSelf (const Rect *lRect, const void *lElement, const short lDataLen) ;

private: //	Added from CCustomListBox (Developer 21) 	
  	friend pascal void  LDefProc (short lMessage, Boolean lSelect, Rect *lRect,
					   			  Cell lCell, unsigned short lDataOffset, unsigned short lDataLen, ListHandle lHandle) ;

  	static ListDefUPP	callerLDEFUPP ;
  	
  	enum {
  		callerLDEFResID = 1100 
  	} ;	
};

/*****************************************************************************
 * class CFormButton
 * Button used in forms.
 * Broadcasts itself (this) instead of value when pressed.
 * Also resizes itself to fit the width of the title
 ****************************************************************************/

class CFormButton : public LStdButton, public LFormElement, public CLayerClickCallbackMixin {

public:
	enum { class_ID = 'fbut' };

// ¥¥ Constructors/destructors

	CFormButton(LStream * inStream);

// ¥¥ Misc
	// After the name is set, resizes the control to fit around it
	virtual void SetDescriptor(ConstStr255Param	inDescriptor);
	// Broadcasts 'this' as an ioParam.
	virtual void BroadcastValueMessage();

// Mocha hot spot handling
	virtual void		EventMouseUp(const EventRecord& inEvent);
	virtual void 		ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void		HotSpotResult(Int16 inHotSpot);
	void				HotSpotResultCallback(Int16 inHotSpot);

protected:
	virtual void		DrawSelf();
	virtual void		DrawTitle();
	virtual void		HotSpotAction(Int16 inHotSpot, Boolean inCurrInside, Boolean inPrevInside);
	virtual Boolean		TrackHotSpot(Int16 inHotSpot, Point inPoint, Int16 inModifiers);

};

/*****************************************************************************
 * class CGAFormPushButton
 * Button used in forms.
 * Broadcasts itself (this) instead of value when pressed.
 * Also resizes itself to fit the width of the title
 ****************************************************************************/

class CGAFormPushButton : public LGAPushButton, public LFormElement, public CLayerClickCallbackMixin {

public:
	enum { class_ID = 'Gfpb' };

	typedef LGAPushButton super;
	
// ¥¥ Constructors/destructors

	CGAFormPushButton(LStream * inStream);

// ¥¥ Misc
	// After the name is set, resizes the control to fit around it
	virtual void SetDescriptor(ConstStr255Param	inDescriptor);
	// Broadcasts 'this' as an ioParam.
	virtual void BroadcastValueMessage();

// Mocha hot spot handling
	virtual void		EventMouseUp(const EventRecord& inEvent);
	virtual void 		ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void		HotSpotResult(Int16 inHotSpot);
	void				HotSpotResultCallback(Int16 inHotSpot);

protected:
	virtual void		DrawButtonTitle( Int16	inDepth );

};

/*****************************************************************************
 * class FormsGlypher
 * Popup Glypher used in forms.
 * Displays a FORM_TYPE_SELECT_ONE structure
 ****************************************************************************/

struct lo_FormElementSelectData_struct;
class FormsPopup: public StdPopup, public LFormElement {
public:

	// 97-06-15 pkc -- different states for reflecting focus to mocha
	enum EFocusState{
		FocusState_None = 0,
		FocusState_FirstCall,
		FocusState_SecondCall,
		FocusState_ThirdCall,
		FocusState_FourthCall,
		FocusState_FifthCall
	};
					FormsPopup (CGAPopupMenu * target, lo_FormElementSelectData_struct * selections);
	// interface
	short			GetCount ();
	CStr255			GetText (short item);
	virtual void	ExecuteSelf (MessageT message, void *param);

	virtual void	PostMochaFocusCallback();

protected:
		// do ScriptCode Menu Trick on IM:MacTbxEss Page3-46
	virtual void	SetMenuItemText(MenuHandle aquiredMenu, int item, CStr255& itemText);
	virtual	void 	DrawTruncTextBox (CStr255 text, const Rect& box);
	virtual Boolean	NeedCustomPopup() const;
private:
	virtual	Boolean	NeedDrawTextInOurOwn() const;
private:
	lo_FormElementSelectData_struct * fSelections;
	EFocusState fFocusState;
	LFormElement* fSavedFocus;
	SInt32 fOldValue;
};

/*****************************************************************************
 * class CFormRadio
 * Radio buttons used in forms. Have white background
 ****************************************************************************/
class CFormRadio: public LStdRadioButton, public LFormElement, public CLayerClickCallbackMixin {
	static RgnHandle sRadioRgn;
	void	SetupClip();
public:
	enum { class_ID = 'frad' };

// ¥¥ Constructors/destructors

	CFormRadio(LStream * inStream);

// ¥¥ Setting up the drawing environment
	virtual Boolean TrackHotSpot(Int16	inHotSpot,Point inPoint, Int16 inModifiers);
	virtual void SetValue(Int32	inValue);
	
// Mocha
	virtual void	EventMouseUp(const EventRecord& inEvent);
	virtual void 	ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void	HotSpotResult(Int16 inHotSpot);
			void	HotSpotResultCallback(Int16 inHotSpot, 
								LO_FormElementStruct* oldRadio,
								Int16 savedValue,
								MWContext * context);

protected:
	virtual void DrawSelf();
};

/*****************************************************************************
 * class CGAFormRadio
 * Radio buttons used in forms. Have white background
 ****************************************************************************/
class CGAFormRadio: public LGARadioButton, public LFormElement, public CLayerClickCallbackMixin
{
public:
	enum { class_ID = 'Gfrb' };
	
	typedef LGARadioButton super;

	
					CGAFormRadio(LStream* inStream);

	virtual Boolean TrackHotSpot(
									Int16	inHotSpot,
									Point	inPoint,
									Int16 inModifiers);
	virtual void	SetValue(Int32	inValue);
	
// Mocha
	virtual void	EventMouseUp(const EventRecord& inEvent);
	virtual void 	ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void	HotSpotResult(Int16 inHotSpot);
			void	HotSpotResultCallback(Int16 inHotSpot, 
								LO_FormElementStruct* oldRadio,
								Int16 savedValue,
								MWContext * context);
								
protected:
	virtual void	DrawSelf();
};

/*****************************************************************************
 * class CFormCheckbox
 * Checkboxes used in forms. Have white background.
 * Very much like CFormRadio
 ****************************************************************************/
class CFormCheckbox: public LStdCheckBox, public LFormElement, public CLayerClickCallbackMixin {
	static RgnHandle sCheckboxRgn;
	void	SetupClip();
public:
	enum { class_ID = 'fchk' };

// ¥¥ Constructors/destructors

	CFormCheckbox(LStream * inStream);

// ¥¥ Setting up the drawing environment
	virtual Boolean TrackHotSpot(Int16	inHotSpot,Point inPoint, Int16 inModifiers);
	virtual void SetValue(Int32	inValue);

	virtual void	EventMouseUp(const EventRecord& inEvent);
	virtual void 	ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void	HotSpotResult(Int16 inHotSpot);
	void			HotSpotResultCallback( Int16 inHotSpot, Int16 savedValue );

protected:
	virtual void DrawSelf();
};

/*****************************************************************************
 * class CGAFormCheckbox
 * Checkboxes used in forms. Have white background.
 * Very much like CFormRadio
 ****************************************************************************/
class CGAFormCheckbox: public LGACheckbox, public LFormElement, public CLayerClickCallbackMixin
{
public:
	enum { class_ID = 'Gfcb' };
	
	typedef LGACheckbox super;
	
					CGAFormCheckbox(LStream* inStream);

	virtual Boolean TrackHotSpot(
									Int16	inHotSpot,
									Point	inPoint,
									Int16 inModifiers);
	virtual void	SetValue(Int32	inValue);
	
// Mocha
	virtual void	EventMouseUp(const EventRecord& inEvent);
	virtual void 	ClickSelfLayer(const SMouseDownEvent	&inMouseDown); // mjc
	virtual void	HotSpotResult(Int16 inHotSpot);
	void			HotSpotResultCallback( Int16 inHotSpot, Int16 savedValue );
								
protected:
	virtual void	DrawSelf();
};

/*****************************************************************************
 * class CFormFile
 * File upload buttons used in forms
 * This element has all the smarts. It knows how to:
 * Resize
 * Select files
 ****************************************************************************/
class CFormFileEditField;
class CFormFile : public LView , public LListener, public LFormElement {
public:
	enum { class_ID = 'ffil' };

// ¥¥ Constructors/destructors

	CFormFile(LStream * inStream);
	virtual void	FinishCreateSelf();
	void	SetVisibleChars(Int16 numOfChars);
	void	GetFontInfo(Int32& width, Int32& height, Int32& baseline);
// ¥¥ Access
	Boolean GetFileSpec(FSSpec & spec);
	void	SetFileSpec(FSSpec & spec);

// ¥¥ÊMisc
	virtual void ListenToMessage(MessageT inMessage, void *ioParam);

	CFormFileEditField * fEditField;

private:

	FSSpec fSpec;	// Our file specs
	Boolean fHasFile;
	LStdButton * fButton;
};

/*****************************************************************************
 * CFormFileEditField
 * Text field of the file upload widget.
 * according to the specs it needs to be non-editable, only action you
 * can do inside it is cut/delete key.
 * On a click, it is always highlited
 ****************************************************************************/
class CFormFileEditField : public LEditField	{
public:
	enum { class_ID = 'ffie' };
	friend class CFormFile;
// ¥¥ Constructors/destructors

	CFormFileEditField(LStream * inStream);

// ¥¥ÊMisc -- overrides so that you can only click and delete
	virtual	void SpendTime(const EventRecord &/*inMacEvent*/) {};  // Don't blink cursor
	virtual Boolean	HandleKeyPress(const EventRecord&	inKeyEvent);
	virtual void FindCommandStatus(CommandT	inCommand,
							Boolean		&outEnabled,
							Boolean		&outUsesMark,
							Char16		&outMark,
							Str255		outName);
	virtual void UserChangedText();

protected:
	virtual void ClickSelf(const SMouseDownEvent	&inMouseDown);
	virtual void DrawSelf();
};

