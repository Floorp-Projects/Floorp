/* $Id: FindDialog.cpp,v 1.1 1998/09/25 18:01:25 ramiro%netscape.com Exp $
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
 * The Initial Developer of this code under the NPL is Kalle
 * Dalheimer.  Further developed by Warwick Allison, Kalle Dalheimer,
 * Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard Nord and
 * Paul Olav Tvete.  Copyright (C) 1998 Warwick Allison, Kalle
 * Dalheimer, Eirik Eng, Matthias Ettrich, Arnt Gulbrandsen, Haavard
 * Nord and Paul Olav Tvete.  All Rights Reserved.
 */

#include <client.h>
#include <intl_csi.h>
#include <xpgetstr.h>

#include "FindDialog.h"

#include "QtContext.h"
#include "finding.h"

#include <qlayout.h>
#include <qpushbt.h>
#include <qchkbox.h>
#include <qframe.h>
#include <qlabel.h>
#include <qlined.h>
#include <qtbuttonrow.h>
#include <qmainwindow.h>


unsigned char *
fe_ConvertFromLocaleEncoding(int16 charset, unsigned char *str); // from locale.cpp

FindDialog::FindDialog( QtContext* cx, QWidget* parent,
			const char* name ) :
    QDialog( parent, name, false ),
    context( cx )
{
    QVBoxLayout* vlayout = new QVBoxLayout( this );

    QHBoxLayout* editlayout = new QHBoxLayout();
    vlayout->addLayout( editlayout );

    QLabel* label = new QLabel( tr( "Find:" ), this );
    label->setFixedSize( label->sizeHint() );
    editlayout->addWidget( label );

    edit = new QLineEdit( this );
    edit->setFixedHeight( edit->sizeHint().height() );
    edit->setFocus();
    editlayout->addWidget( edit, 1 );

    QHBoxLayout* cblayout = new QHBoxLayout();
    vlayout->addLayout( cblayout );

    sensitiveCB = new QCheckBox( tr( "Case Sensitive" ), this );
    sensitiveCB->setFixedSize( sensitiveCB->sizeHint() );
    cblayout->addWidget( sensitiveCB );

    backwardsCB = new QCheckBox( tr( "Find Backwards" ), this );
    backwardsCB->setFixedSize( backwardsCB->sizeHint() );
    cblayout->addWidget( backwardsCB );

    QFrame* line = new QFrame( this, "line", 0, true );
    line->setFrameStyle( QFrame::HLine | QFrame::Sunken );
    line->setFixedHeight( 12 ); // Arnt said so...
    vlayout->addWidget( line );

    QtButtonRow* buttonrow = new QtButtonRow( this );

    QPushButton* findPB = new QPushButton( tr( "Find" ), buttonrow );
    connect( findPB, SIGNAL( clicked() ),
	     this, SLOT( find() ) );
    findPB->setDefault( true );

    QPushButton* clearPB = new QPushButton( tr( "Clear" ), buttonrow );
    connect( clearPB, SIGNAL( clicked() ),
	     this, SLOT( clear() ) );

    QPushButton* closePB = new QPushButton( tr( "Close" ), buttonrow );
    connect( closePB, SIGNAL( clicked() ),
	     this, SLOT( hide() ) );

    vlayout->addWidget( buttonrow );
    vlayout->activate();
    resize( 1, 1 );
}


void FindDialog::refresh()
{
    fe_FindData *find_data = &context->find_data;

    find_data->context_to_find = context->mwContext();

    char * tmp;

    find_data->string = edit->text();
    INTL_CharSetInfo c;
    c = LO_GetDocumentCharacterSetInfo( context->mwContext() );
    tmp = (char *) fe_ConvertFromLocaleEncoding( INTL_GetCSIWinCSID(c),
						 (unsigned char*)find_data->string.data() );
    if( tmp ) {
	find_data->string = tmp;
    } else {
	find_data->string = "";
    }

    find_data->case_sensitive_p = sensitiveCB->isChecked();
    find_data->backward_p = backwardsCB->isChecked();
    /* For mail/news contexts, the Search in Header/Body is loaded into the
       fe_FindData in the valueChangeCallback */
}


void FindDialog::find()
{
  fe_FindData* find_data = &context->find_data;

  MWContext *context_to_find;
  QWidget* mainw;
  bool hasRetried = false;
  CL_Layer *layer;

  XP_ASSERT(find_data);

  mainw = context->topLevelWidget();

  /* reload search parameters */
  refresh();

  // Use the associated context unless there is one defined.
  context_to_find = context->mwContext();
  if( find_data->context_to_find )
    context_to_find = find_data->context_to_find;

  if( find_data->string.isEmpty() ) {
      FE_Alert( context->mwContext(), tr( "Nothing to search for." ) );
      return;
  }

  if( find_data->find_in_headers ) {
      XP_ASSERT( find_data->context->type == MWContextMail ||
		 find_data->context->type == MWContextNews );
      if ( find_data->context->type == MWContextMail ||
	   find_data->context->type == MWContextNews ) {
	  int status = -1;		/* ###tw  Find needs to be hooked up in a brand
					   new way now### */
	  /*###tw      int status = MSG_DoFind(fj->context, fj->string, fj->case_sensitive_p); */
	  if (status < 0) {
	      /* mainw could be the find_data->shell. If status < 0 (find failed)
	       * backend will bring the find window down. So use the context to
	       * display the error message here.
	       */
	      FE_Alert( find_data->context, XP_GetString( status ) );
	      return;
	  }
	  return;
      }
  }

  /* but I think you will want this in non-Gold too! */
  /* And with QtMozilla, you get it ... */
  /*
   *    Start the search from the current selection location. Bug #29854.
   */
  LO_GetSelectionEndpoints( context_to_find,
			    &find_data->start_element,
			    &find_data->end_element,
			    &find_data->start_pos,
			    &find_data->end_pos,
			    &layer );
 AGAIN:

  if( LO_FindText( context_to_find, find_data->string.data(),
		   &find_data->start_element, &find_data->start_pos,
		   &find_data->end_element, &find_data->end_pos,
		   find_data->case_sensitive_p, !find_data->backward_p ) )
      {
	  int32 x, y;
	  LO_SelectText ( context_to_find,
			  find_data->start_element, find_data->start_pos,
			  find_data->end_element, find_data->end_pos,
			  &x, &y);

	  /* If the found item is not visible on the screen, scroll to it.
	     If we need to scroll, attempt to position the destination
	     coordinate in the middle of the window.
	  */
	  if (x >= CONTEXT_DATA (context_to_find)->documentXOffset() &&
	      x <= (CONTEXT_DATA (context_to_find)->documentXOffset() +
		    CONTEXT_DATA (context_to_find)->scrollWidth()))
	      x = CONTEXT_DATA (context_to_find)->documentXOffset();
	  else
	      x = x - (CONTEXT_DATA (context_to_find)->scrollWidth() / 2);
	
	  if (y >= CONTEXT_DATA (context_to_find)->documentYOffset() &&
	      y <= (CONTEXT_DATA (context_to_find)->documentYOffset() +
		    CONTEXT_DATA (context_to_find)->scrollHeight()))
	      y = CONTEXT_DATA (context_to_find)->documentYOffset();
	  else
	      y = y - (CONTEXT_DATA (context_to_find)->scrollHeight() / 2);
	
	  if (x + CONTEXT_DATA (context_to_find)->scrollWidth()
	      > CONTEXT_DATA (context_to_find)->documentWidth())
	      x = (CONTEXT_DATA (context_to_find)->documentWidth() -
		   CONTEXT_DATA (context_to_find)->scrollWidth());
	
	  if (y + CONTEXT_DATA (context_to_find)->scrollHeight()
	      > CONTEXT_DATA (context_to_find)->documentHeight())
	      y = (CONTEXT_DATA (context_to_find)->documentHeight() -
		   CONTEXT_DATA (context_to_find)->scrollHeight());
	
	  if (x < 0) x = 0;
	  if (y < 0) y = 0;
	
	  if( context->topLevelWidget() )
	      context->documentSetContentsPos( x, y );
      } else {
	  if (hasRetried) {
	      FE_Alert( context->mwContext(), tr( "Search string not found." ) );
	      return;
	  } else {
	      if( FE_Confirm( context->mwContext(),
			      ( find_data->backward_p
				? tr( "Beginning of document reached; continue from end?" )
				: tr( "End of document reached; continue from beginning?" ) ) ) ) {
		  find_data->start_element = 0;
		  find_data->start_pos = 0;
		  hasRetried = true;
		  goto AGAIN;
	      }
	      else
		  return;
	  }
      }
  return;
}


void FindDialog::clear()
{
    edit->setText( "" );
}
