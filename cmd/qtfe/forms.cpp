/* $Id: forms.cpp,v 1.2 1998/10/20 02:40:45 cls%seawood.org Exp $
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
 * Copyright (C) 1998 Netscape Communications Corporation.  Portions
 * created by Warwick Allison, Kalle Dalheimer, Eirik Eng, Matthias
 * Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav Tvete are
 * Copyright (C) 1998 Warwick Allison, Kalle Dalheimer, Eirik Eng,
 * Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and Paul Olav
 * Tvete.  All Rights Reserved.
 */

#include <qlined.h>
#include <qmlined.h>
#include <qradiobt.h>
#include <qpushbt.h>
#include <qcombo.h>
#include <qchkbox.h>
#include <qlistbox.h>
#include <qlabel.h>
#include <qlistview.h>
#include <qkeycode.h>
#include <qdir.h>
#if defined(XP_UNIX)
#include <xpform.h>
#endif
#include <libevent.h>
#include "dirview.h"
#include "QtBrowserContext.h"


#if defined(XP_WIN)
/*
  Since xpform isn't part of the Windows distribution of Mozilla,
  we include the necessary sources here.

  Then we just don't use them, but leave them here for reference.
*/
#include "xpassert.h"

LO_FormElementData*
XP_GetFormElementData(LO_FormElementStruct *form)
{
  return form->element_data;
}

LO_TextAttr*
XP_GetFormTextAttr(LO_FormElementStruct *form)
{
  return form->text_attr;
}

uint16
XP_GetFormEleAttrmask(LO_FormElementStruct *form)
{
  return form->ele_attrmask;
}

int32
XP_FormGetType(LO_FormElementData *form)
{
  return form->type;
}

void
XP_FormSetFEData(LO_FormElementData *form,
				 void* fe_data)
{
  form->ele_minimal.FE_Data = fe_data;
}


void*
XP_FormGetFEData(LO_FormElementData *form)
{
  return form->ele_minimal.FE_Data;
}

/* Methods that work for Hidden, Submit, Reset, Button, and Readonly
   form elements. */
PA_Block
XP_FormGetValue(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_HIDDEN
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY);

  return form->ele_minimal.value;
}

Bool
XP_FormGetDisabled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT
			|| form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY);

  if (form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT
			|| form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX
			|| form->type == FORM_TYPE_SUBMIT
			|| form->type == FORM_TYPE_RESET
			|| form->type == FORM_TYPE_BUTTON
			|| form->type == FORM_TYPE_READONLY)
    return form->ele_minimal.disabled;
  else
    return FALSE;
}

/* Methods that work on text or text area form elements */
PA_Block
XP_FormGetDefaultText(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);

  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  return form->ele_text.default_text;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.default_text;
	default:
	  return NULL;
	}
}

PA_Block
XP_FormGetCurrentText(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);

  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  return form->ele_text.current_text;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.current_text;
	default:
	  return NULL;
	}
}

void
XP_FormSetCurrentText(LO_FormElementData *form,
					  PA_Block current_text)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);

  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_FILE:
	case FORM_TYPE_READONLY:
	  form->ele_text.current_text = current_text;
	  break;
	case FORM_TYPE_TEXTAREA:
	  form->ele_textarea.current_text = current_text;
	  break;
	}
}

Bool
XP_FormGetReadonly(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_TEXTAREA
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY);

  switch(form->type)
	{
	case FORM_TYPE_TEXT:
	case FORM_TYPE_PASSWORD:
	case FORM_TYPE_READONLY:
	  return form->ele_text.readonly;
	case FORM_TYPE_TEXTAREA:
	  return form->ele_textarea.readonly;
	default:
	  return FALSE;
	}
}

/* text specific methods. */
int32
XP_FormTextGetSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);

  return form->ele_text.size;
}

int32
XP_FormTextGetMaxSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXT
			|| form->type == FORM_TYPE_PASSWORD
			|| form->type == FORM_TYPE_READONLY
			|| form->type == FORM_TYPE_FILE);

  return form->ele_text.max_size;
}

/* text area specific methods. */
void
XP_FormTextAreaGetDimensions(LO_FormElementData *form,
							 int32 *rows,
							 int32 *cols)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXTAREA);

  if (rows)
	*rows = form->ele_textarea.rows;
  if (cols)
	*cols = form->ele_textarea.cols;
}

uint8
XP_FormTextAreaGetAutowrap(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_TEXTAREA);

  return form->ele_textarea.auto_wrap;
}

/* radio/checkbox specific methods */
Bool
XP_FormGetDefaultToggled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  return form->ele_toggle.default_toggle;
}

Bool
XP_FormGetElementToggled(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  return form->ele_toggle.toggled;
}

void
XP_FormSetElementToggled(LO_FormElementData *form,
						 Bool toggled)
{
  XP_ASSERT(form->type == FORM_TYPE_RADIO
			|| form->type == FORM_TYPE_CHECKBOX);

  form->ele_toggle.toggled = toggled;
}

/* select specific methods */
int32
XP_FormSelectGetSize(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.size;
}

Bool
XP_FormSelectGetMultiple(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.multiple;
}

Bool
XP_FormSelectGetOptionsValid(LO_FormElementData *form) /* XXX ? */
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.options_valid;
}

int32
XP_FormSelectGetOptionsCount(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_SELECT_ONE
			|| form->type == FORM_TYPE_SELECT_MULT);

  return form->ele_select.option_cnt;
}

lo_FormElementOptionData*
XP_FormSelectGetOption(LO_FormElementData *form,
					   int option_index)
{
  XP_ASSERT((form->type == FORM_TYPE_SELECT_ONE
			 || form->type == FORM_TYPE_SELECT_MULT)
			&& option_index >= 0
			&& option_index < form->ele_select.option_cnt);

  return &((lo_FormElementOptionData*)form->ele_select.options)[option_index];
}

/* option specific methods */
Bool
XP_FormOptionGetDefaultSelected(lo_FormElementOptionData *option_element)
{
  return option_element->def_selected;
}

Bool
XP_FormOptionGetSelected(lo_FormElementOptionData *option_element)
{
  return option_element->selected;
}

void
XP_FormOptionSetSelected(lo_FormElementOptionData *option_element,
						 Bool selected)
{
  option_element->selected = selected;
}

/* object */
LO_JavaAppStruct*
XP_FormObjectGetJavaApp(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_OBJECT);

  return form->ele_object.object;
}

void
XP_FormObjectSetJavaApp(LO_FormElementData *form,
						LO_JavaAppStruct *java_app)
{
  XP_ASSERT(form->type == FORM_TYPE_OBJECT);

  form->ele_object.object = java_app;
}

/* keygen */
PA_Block
XP_FormKeygenGetChallenge(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.challenge;
}

PA_Block
XP_FormKeygenGetKeytype(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.key_type;
}

PA_Block
XP_FormKeygenGetPQG(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.pqg;
}

char*
XP_FormKeygenGetValueStr(LO_FormElementData *form)
{
  XP_ASSERT(form->type == FORM_TYPE_KEYGEN);

  return form->ele_keygen.value_str;
}

/* setter routines? */
#endif // XP_WIN



class QtFormElement : QObject {
    LO_FormElementStruct *form;
    QtContext* context;
protected:
    LO_FormElementData* elementData() const { return form->element_data; }

public:
    QtFormElement( LO_FormElementStruct *form_struct, QtContext* cx ) :
	form(form_struct),
	context(cx)
    {
    }
    void set(LO_FormElementStruct *form_struct, QtContext* cx)
    {
	form = form_struct;
        context = cx;
    }

    void getSize()
    {
	int bw = borderWidth();
	int w = width();
	int h = height();
	form->width = w + bw*2;
	form->height = h + bw*2;
	form->baseline = baseLine() + bw;
	widget()->resize(w,h);
    }

    virtual int borderWidth() const
    { return 5; }
    virtual int width() const
    { return widget()->sizeHint().width(); }
    virtual int height() const
    { return widget()->sizeHint().height(); }
    int baseLine() const
    {
	QFontMetrics fm = widget()->fontMetrics();
	return height() - fm.descent() - baseLineAdjust();
    }
    virtual int baseLineAdjust() const
    {
	return 6; // magic look-good value
    }

    virtual void freeStorage()
    {
    }

    virtual void getValue()
    {
    }

    virtual void reset()
    {
    }

    virtual void setToggle( bool ) { }

    virtual bool submitOnClick() const { return FALSE; }
    virtual bool submitOnEnter() const { return FALSE; }

    virtual QWidget* widget() const
    {
	return 0;
    }

    // Stuff for submission
    void isSubmit()
    {
	widget()->installEventFilter(this);
    }

protected:
    int type() const
    {
	return elementData()->type;
    }

    lo_FormElementObjectData* object() const
        { return (lo_FormElementObjectData*)elementData(); }
    lo_FormElementKeygenData* keygen() const
        { return (lo_FormElementKeygenData*)elementData(); }

private:
    bool eventFilter( QObject *, QEvent* );
};


class QtFormToggleElement : public QtFormElement {
protected:
    lo_FormElementToggleData* toggle() const
        { return (lo_FormElementToggleData*)elementData(); }
public:
    QtFormToggleElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
    }
};

class QtFormRadioElement : public QtFormToggleElement {
    QRadioButton *w;
public:
    QtFormRadioElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormToggleElement(form, cx)
    {
	w = new QRadioButton(cx->contentWidget());
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual void getValue()
    {
	toggle()->toggled = w->isChecked();
    }

    virtual void setToggle( bool y )
    {
	w->setChecked( y );
    }

    virtual int baseLineAdjust() const
    {
	return 3; // magic look-good value
    }
};

class QtFormCheckBoxElement : public QtFormToggleElement {
    QCheckBox *w;
public:
    QtFormCheckBoxElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormToggleElement(form, cx)
    {
	w = new QCheckBox(cx->contentWidget());
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual void getValue()
    {
	toggle()->toggled = w->isChecked();
    }

    virtual void setToggle( bool y )
    {
	w->setChecked( y );
    }

    virtual int baseLineAdjust() const
    {
	return 3; // magic look-good value
    }
};

class QtFormTextlikeElement : public QtFormElement {
protected:
    lo_FormElementTextData* text() const
        { return (lo_FormElementTextData*)elementData(); }

    QtFormTextlikeElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
    }

    virtual const char* currentText()
    {
	return "";
    }

    virtual void getValue()
    {
	const char* t = currentText();
	text()->current_text = PA_Block(qstrdup(t));
	if (text()->max_size > 0 && (int)strlen(t) >= text()->max_size )
	    text()->current_text[text()->max_size] = 0;
    }
};

class QtFormButtonElement : public QtFormElement {
    QPushButton *w;

    lo_FormElementMinimalData_struct* minimal() const
        { return (lo_FormElementMinimalData_struct*)elementData(); }
public:
    QtFormButtonElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	w = new QPushButton(cx->contentWidget());
	w->setText( (char*)minimal()->value );
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual bool submitOnClick() const { return TRUE; }
};

class QtFormSubmitElement : public QtFormButtonElement {
public:
    QtFormSubmitElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormButtonElement(form, cx)
    {
	isSubmit();
    }
    virtual bool submitOnClick() const { return TRUE; }
};

class QtFormResetElement : public QtFormButtonElement {
public:
    QtFormResetElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormButtonElement(form, cx)
    {
    }
};

class QtJotter : public QFrame {
    QPixmap pm;
    QPoint ppos;
public:
    QtJotter( QWidget* parent ) :
	QFrame(parent)
    {
	setFrameStyle( Panel | Sunken );
    }
    void resizeEvent(QResizeEvent*)
    {
	int fw = frameWidth();
	QPixmap newpm(width()-fw*2, height()-fw*2);
	if ( !pm.isNull() )
	    bitBlt(&newpm, 0, 0, &pm);
	pm = newpm;
    }
    void paintEvent(QPaintEvent*)
    {
	int fw = frameWidth();
	bitBlt( this, fw, fw, &pm );
    }
    void mousePressEvent(QMouseEvent* e)
    {
	ppos = e->pos();
    }
    void mouseMoveEvent(QMouseEvent* e)
    {
	QPoint off(frameWidth(),frameWidth());
	QPainter p1(this);
	p1.drawLine(ppos,e->pos());
	QPainter p2(&pm);
	p2.drawLine(ppos-off,e->pos()-off);
	ppos = e->pos();
    }
};

class QtFormJotElement : public QtFormElement {
    QtJotter *w;
public:
    QtFormJotElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	w = new QtJotter(cx->contentWidget());
    }

    virtual QWidget* widget() const
    {
	return w;
    }
};

class QtFormSelectElement : public QtFormElement {
protected:
    lo_FormElementSelectData* select() const
        { return (lo_FormElementSelectData*)elementData(); }
    lo_FormElementOptionData& option(int i) const
	{ return ((lo_FormElementOptionData*)(select()->options))[i]; }

    QtFormSelectElement(LO_FormElementStruct *form, QtContext* cx) :
        QtFormElement(form, cx)
    {
    }
};

class QtFormSelectOneElement : public QtFormSelectElement {
    QComboBox *w;
public:
    QtFormSelectOneElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormSelectElement(form, cx)
    {
	w = new QComboBox(cx->contentWidget());
	int nitems = select()->option_cnt;
	int vlines = select()->size;
	if ( vlines <= 0 ) vlines = nitems;
	for (int i=0; i < nitems; i++)
	    w->insertItem((char*)option(i).text_value);
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual void getValue()
    {
	for (int i=0; i < select()->option_cnt; i++)
	    option(i).selected = (w->currentItem() == i);
    }

    virtual void reset()
    {
	for (int i=0; i < select()->option_cnt; i++)
	    if ( option(i).def_selected ) {
		w->setCurrentItem(i);
		break;
	    }
    }
};

class QtFormSelectMultElement : public QtFormSelectElement {
    QListBox *w;
public:
    QtFormSelectMultElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormSelectElement(form, cx)
    {
	w = new QListBox(cx->contentWidget());
	w->setMultiSelection( select()->multiple );
	int nitems = select()->option_cnt;
	int vlines = select()->size;
	if ( vlines <= 0 ) vlines = nitems;
	for (int i=0; i < nitems; i++)
	    w->insertItem((char*)option(i).text_value);
	w->setFixedVisibleLines( vlines );
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual void getValue()
    {
	for (int i=0; i < select()->option_cnt; i++)
	    option(i).selected = w->isSelected(i);
    }

    virtual void reset()
    {
	for (int i=0; i < select()->option_cnt; i++)
	    w->setSelected(i, option(i).def_selected);
    }
};

class QtFormTextAreaElement : public QtFormElement {
    QMultiLineEdit *w;
    lo_FormElementTextareaData* textarea() const
        { return (lo_FormElementTextareaData*)elementData(); }
public:
    QtFormTextAreaElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	w = new QMultiLineEdit(cx->contentWidget());
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual void getValue()
    {
	textarea()->current_text = PA_Block(qstrdup(w->text()));
    }

    virtual void freeStorage()
    {
	if ( textarea()->current_text != textarea()->default_text ) {
	    delete textarea()->current_text;
	    textarea()->current_text = 0;
	}
    }

    virtual void reset()
    {
	freeStorage();
	w->setText( (char*)textarea()->default_text );
	textarea()->current_text = textarea()->default_text;
    }

    virtual int width() const
    {
	QFontMetrics fm = w->fontMetrics();
	int c=textarea()->cols; if (!c) c=20;
	return c * fm.width("X");
    }

    virtual int height() const
    {
	w->setFixedVisibleLines(textarea()->rows);
	return w->height();
    }

    virtual int baseLineAdjust() const
    {
	return 5; // magic look-good value
    }
};

class QtFormPasswordElement : public QtFormTextlikeElement {
protected:
    QLineEdit *w;
public:
    QtFormPasswordElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormTextlikeElement(form, cx)
    {
	w = new QLineEdit(cx->contentWidget());
	w->setEchoMode(QLineEdit::Password);
	if ( text()->max_size > 0 )
	    w->setMaxLength(text()->max_size);
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual const char* currentText()
    {
	return w->text();
    }

    virtual void freeStorage()
    {
	if ( text()->current_text != text()->default_text ) {
	    delete text()->current_text;
	    text()->current_text = 0;
	}
    }

    virtual int width() const
    {
	QFontMetrics fm = w->fontMetrics();
	int c=text()->size; if (!c) c=20;
	return c * fm.width("X")
	    + (w->frame() ? 2 : 0)*2 + 3 // magic from qlineedit.cpp
	    - fm.minLeftBearing()
	    - fm.minRightBearing();
    }

    virtual void reset()
    {
	w->setText( (char*)text()->default_text );
    }


    virtual int baseLineAdjust() const
    {
	return 5; // magic look-good value
    }

    virtual bool submitOnEnter() const { return TRUE; }
};

class QtFormTextElement : public QtFormPasswordElement {
public:
    QtFormTextElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormPasswordElement(form, cx)
    {
	w->setEchoMode(QLineEdit::Normal);
    }
};

class QtFormHiddenElement : public QtFormTextElement {
public:
    QtFormHiddenElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormTextElement(form, cx)
    {
	warning("Hidden should not be created by BE");
    }
};

class QtFormIsIndexElement : public QtFormTextElement {
public:
    QtFormIsIndexElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormTextElement(form, cx)
    {
	warning("IsIndex should not be created by BE");
    }
};

class QtFormImageElement : public QtFormElement {
    QLabel *w;
public:
    QtFormImageElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	warning("Image should not be created by BE");
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual bool submitOnClick() const { return TRUE; }
};

class QtFormFileElement : public QtFormTextlikeElement {

    // This is really just showing-off.  We could just use a boring
    // lineedit + browse button like other browsers usually do.

    QListView *w;
    QString current;
public:
    QtFormFileElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormTextlikeElement(form, cx)
    {
	// Just to show off, we use the Directory example program.
	w = new QListView(cx->contentWidget());
	w->addColumn( "Name" );
	w->addColumn( "Type" );
	Directory * root = new Directory( w );
	root->setOpen( TRUE );
    }

    virtual QWidget* widget() const
    {
	return w;
    }

    virtual const char* currentText()
    {
	// This is a bit ugly...
	QListViewItem * c = w->currentItem();
	current = "";
	if (c) {
	    // We know it's parent is a Directory, we put it there.
	    Directory* d = (Directory*)c->parent();
	    if (d)
		current = d->fullName() + c->text(0);
	}
//debug("FILE: \"%s\"",(const char*)current);

	return QDir::convertSeparators(current);
    }

    virtual int width() const { return 400; }
    virtual int height() const { return 300; }
};

class QtFormKeyGenElement : public QtFormElement {
    QLabel *w;
public:
    QtFormKeyGenElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	warning("Keygen should not be created by BE");
    }

    virtual QWidget* widget() const
    {
	return w;
    }
};

class QtFormReadOnlyElement : public QtFormTextElement {
public:
    QtFormReadOnlyElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormTextElement(form, cx)
    {
	debug("What is a READONLY?");
    }
};

class QtFormObjectElement : public QtFormElement {
    QLabel *w;
public:
    QtFormObjectElement(LO_FormElementStruct *form, QtContext* cx) :
	QtFormElement(form, cx)
    {
	warning("Object should not be created by BE");
    }

    virtual QWidget* widget() const
    {
	return w;
    }
};


static
QtFormElement* element(LO_FormElementStruct *form)
{
    LO_FormElementData *form_data = form->element_data;
    if (!form_data) return 0;
    return (QtFormElement*)form_data->ele_minimal.FE_Data;
}


/* From ./forms.c: */
void QtContext::getFormElementInfo(LO_FormElementStruct *form)
{
    //    printf( "QTFE_GetFormElementInfo %p \n", form );

    LO_FormElementData *form_data = form->element_data;
    if ( !form_data ) return;

    QtFormElement *fel = (QtFormElement*)form_data->ele_minimal.FE_Data;
    if ( !fel ) {
	switch ( form_data->type ) {
	  case FORM_TYPE_NONE:
	    return;
	    break;
	  case FORM_TYPE_TEXT:
	    fel = new QtFormTextElement( form, this );
	    break;
	  case FORM_TYPE_RADIO:
	    fel = new QtFormRadioElement( form, this );
	    break;
	  case FORM_TYPE_CHECKBOX:
	    fel = new QtFormCheckBoxElement( form, this );
	    break;
	  case FORM_TYPE_HIDDEN:
	    fel = new QtFormHiddenElement( form, this );
	    break;
	  case FORM_TYPE_SUBMIT:
	    fel = new QtFormSubmitElement( form, this );
	    break;
	  case FORM_TYPE_RESET:
	    fel = new QtFormResetElement( form, this );
	    break;
	  case FORM_TYPE_PASSWORD:
	    fel = new QtFormPasswordElement( form, this );
	    break;
	  case FORM_TYPE_BUTTON:
	    fel = new QtFormButtonElement( form, this );
	    break;
	  case FORM_TYPE_JOT:
	    fel = new QtFormJotElement( form, this );
	    break;
	  case FORM_TYPE_SELECT_ONE:
	    fel = new QtFormSelectOneElement( form, this );
	    break;
	  case FORM_TYPE_SELECT_MULT:
	    fel = new QtFormSelectMultElement( form, this );
	    break;
	  case FORM_TYPE_TEXTAREA:
	    fel = new QtFormTextAreaElement( form, this );
	    break;
	  case FORM_TYPE_ISINDEX:
	    fel = new QtFormIsIndexElement( form, this );
	    break;
	  case FORM_TYPE_IMAGE:
	    fel = new QtFormImageElement( form, this );
	    break;
	  case FORM_TYPE_FILE:
	    fel = new QtFormFileElement( form, this );
	    break;
	  case FORM_TYPE_KEYGEN:
	    fel = new QtFormKeyGenElement( form, this );
	    break;
	  case FORM_TYPE_READONLY:
	    fel = new QtFormReadOnlyElement( form, this );
	    break;
	  case FORM_TYPE_OBJECT:
	    fel = new QtFormObjectElement( form, this );
	    break;
	}
	form_data->ele_minimal.FE_Data = fel;
    } else {
	fel->set(form, this);
    }

    LO_TextAttr *a = form->text_attr;
    if (a) {
	QWidget* w=fel->widget();
	w->setFont(getFont(a));
	QColor fg(a->fg.red, a->fg.green, a->fg.blue);
	QColor bg(a->bg.red, a->bg.green, a->bg.blue);

	// Qt doesn't currently work well with 100% white or black
	// backgrounds, but they are common on the web, so we adjust.
	//
	QColor topsh = bg.light();
	QColor botsh = bg.dark();
	int h,s,v;
	bg.hsv(&h,&s,&v);
	if ( v > 200 ) {
	    topsh.setHsv(h,s,200);
	} else if ( v < 90 ) {
	    topsh.setHsv(h,s,90);
	    botsh.setHsv(h,s,60);
	}

        QColorGroup g(fg,
		    bg,
		    topsh,
		    botsh,
		    bg,
		    fg,
		    contentWidget()->colorGroup().base());

	w->setPalette(QPalette(g,g,g));
    }
    fel->reset();
    fel->getSize();
}

/* From ./forms.c: */
void QtContext::getFormElementValue(LO_FormElementStruct *, bool, bool)
{
}

/* XXX - wtf does submit_p do? */
void QtBrowserContext::getFormElementValue(LO_FormElementStruct *form,
						 bool delete_p, bool submit_p)
{
    element(form)->getValue();
    if ( delete_p ) {
	// How MUCH do we delete of it?
	showSubWidget( element(form)->widget(), FALSE );
    }
}

/* From ./forms.c: */
void QtContext::resetFormElement(LO_FormElementStruct *form)
{
    element(form)->reset();
}

/* From ./forms.c: */
void QtContext::setFormElementToggle(LO_FormElementStruct *form,
			  bool state)
{
    element(form)->setToggle(state);
}

/* From ./forms.c: */
void QtContext::formTextIsSubmit(LO_FormElementStruct *form)
{
    element(form)->isSubmit();
}

/* From ./forms.c: */
void QtContext::displayFormElement(int iLocation,
					   LO_FormElementStruct *form)
{
    debug( "QTFE_DisplayFormElement %d, %p", iLocation, form);
}

/* From ./forms.c: */
void QtBrowserContext::displayFormElement(int /*iLocation*/,
					   LO_FormElementStruct *form)
{
    // The back-end calls this too early some times (eg. when laying
    // out tables).

    QtFormElement *e = element(form);
    if ( e && form->ele_attrmask & LO_ELE_INVISIBLE ) {
	QWidget * w = e->widget();
	// We are already taking document[XY]Offset() into account.
	// Mozilla puts all the logic for dealing with the window-system
	// co-ordinate range limitations of the window system.
	int x = form->x + form->x_offset + e->borderWidth() + getXOrigin();
	int y = form->y + form->y_offset + e->borderWidth() + getYOrigin();
	w->resize( form->width-e->borderWidth()*2,
		   form->height-e->borderWidth()*2 );
	moveSubWidget( w, x, y );
    }
}

/* From ./forms.c: */
void QtContext::freeFormElement(LO_FormElementData *form_data)
{
    // take it out and delete it - easier than loads of destructors.
    QtFormElement *e = (QtFormElement*)form_data->ele_minimal.FE_Data;
    if ( e ) {
	QWidget * w = e->widget();
	delete e;
	delete w;
    }
    form_data->ele_minimal.FE_Data = 0;
}



void
submitForm(MWContext *context, LO_Element *element,
                              int32 /*event*/, void *closure,
                              ETEventStatus status)
{
    QtContext *cx = QtContext::qt(context);
    if (status != EVENT_OK) return;

    LO_FormSubmitData *data;
    if ( element->type == LO_IMAGE && closure ) {
	QPoint *p = (QPoint*)closure;
	data = LO_SubmitImageForm (context, &element->lo_image, p->x(), p->y() );
    } else {
	data = LO_SubmitForm (context, (LO_FormElementStruct *) element);
    }
    if (!data) return;

    URL_Struct *url = NET_CreateURLStruct ((char *) data->action, NET_DONT_RELOAD);

    NET_AddLOSubmitDataToURLStruct(data, url);

    // Add the referer to the URL.
    History_entry *he = SHIST_GetCurrent (&context->hist);
    if (url->referer)
	free (url->referer);
    url->referer = he->address ? XP_STRDUP (he->address) : 0;

    cx->getURL(url);
    LO_FreeSubmitData (data);
}

static
void
send_submit(MWContext *context, LO_Element *element,
                              int32 /*event*/, void *closure,
                              ETEventStatus /*status*/)
{
    JSEvent *js_event = XP_NEW_ZAP(JSEvent);
    js_event->type = EVENT_SUBMIT;
    ET_SendEvent(context, element, js_event, submitForm, closure);
}



bool QtFormElement::eventFilter( QObject *o, QEvent *e )
{
    // ### This code is a bit too eager!
    ASSERT(o==widget());
    JSEvent *js_event = XP_NEW_ZAP(JSEvent);
    bool send = FALSE;
    switch (e->type()) {
    case Event_MouseButtonPress:
    case Event_MouseButtonRelease:
      {
	QMouseEvent *me = (QMouseEvent*)e;
	if (me->button() == LeftButton) js_event->which = 1;
	else if (me->button() == MidButton) js_event->which = 2;
	else if (me->button() == RightButton) js_event->which = 3;
	send = submitOnClick();
      } break;
    case Event_MouseMove:
	break;
    case Event_KeyPress:
    case Event_KeyRelease:
      {
	QKeyEvent* ke = (QKeyEvent*)e;
	js_event->which = ke->ascii();
	send = submitOnEnter() && ( ke->key() == Key_Return || ke->key() == Key_Enter );
      } break;
    default:
	break;
    }
    if ( send ) {
	// Mozilla says: "send me a click, dammit!"
	js_event->type = EVENT_CLICK;
	ET_SendEvent(context->mwContext(), (LO_Element *)form, js_event,
		      send_submit, 0);
    }
    return FALSE;
}
