/* $Id: printing.cpp,v 1.2 1998/09/25 18:06:15 ramiro%netscape.com Exp $
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

#include "QtBrowserContext.h"

#include <fe_proto.h>
#include <xlate.h>
#include <intl_csi.h>
#include <xpgetstr.h>
#include <np.h>
#include <shist.h>

#include <math.h>

#include <qprinter.h>
#include <qprndlg.h>
#include <qmainwindow.h>

static void QTFE_InitializePrintSetup( QPrinter*, PrintSetup* );
void ps_pipe_close( PrintSetup* p );
void ps_file_close( PrintSetup* p );
void fe_synchronous_url_exit (URL_Struct *url, int status,
				  MWContext *context);
void fe_RaiseSynchronousURLDialog (MWContext *context, QWidget* parent,
				   const char *title);
int fe_AwaitSynchronousURL( MWContext* context );

extern int QTFE_ERROR_OPENING_FILE;
extern int QTFE_ERROR_OPENING_PIPE;
extern int QTFE_DIALOGS_PRINTING;

void QtBrowserContext::cmdPrint()
{
    if ( printer.setup() ) {
	// This is from fe_print_cb. Kalle.
	History_entry* hist = SHIST_GetCurrent ( &mwContext()->hist );
	URL_Struct* url = SHIST_CreateWysiwygURLStruct( mwContext(), hist );
	if( url )
	    print( url, printer.outputToFile(), printer.outputFileName() );
	else
	    FE_Alert( mwContext(), tr( "No document has yet been loaded in this window." ) );
    }
}


void QtBrowserContext::print( URL_Struct* url, bool print_to_file,
			      const char* filename )
{
    // This code is adapted from fe_Print. Kalle.

    SHIST_SavedData saved_data;
    PrintSetup p;
    INTL_CharSetInfo c = LO_GetDocumentCharacterSetInfo( mwContext() );
    char mimecharset[48];

    QTFE_InitializePrintSetup( &printer, &p );

    if( print_to_file ) {
	if( !filename ) {
	    FE_Alert( mwContext(), tr( "No output file specified." ) );
	    return;
	}
	
	/* If the file exists, confirm overwriting it. */
	XP_StatStruct st;
	char buf[2048];
#if defined(_CC_MSVC_)
	if( !_stat( filename, &st ) ) {
#else
	if( !stat( filename, &st ) ) {
#endif
	    PR_snprintf( buf, sizeof( buf ),
			 tr( "Overwrite existing file %s?" ),
			 filename );
	    if( !FE_Confirm( mwContext(), buf ) )
		return;
	}

	p.out = fopen( filename, "w" );
	if( !p.out ) {
	    char buf[2048];
	    PR_snprintf( buf, sizeof( buf ),
			 XP_GetString( QTFE_ERROR_OPENING_FILE ),
			 filename );
	    perror( buf );
	    return;
	}
    } else {
	if( !strlen( printer.printProgram() ) ) {
	    FE_Alert( mwContext(), tr( "No print command specified." ) );
	    return;
	}
#if defined(XP_WIN)
	p.out = fopen( "c:/qsprint.ps", "w" );
#else
	p.out = popen( printer.printProgram(), "w" );
#endif
	if( !p.out ) {
	    char buf[2048];
	    PR_snprintf( buf, sizeof( buf ),
			 XP_GetString( QTFE_ERROR_OPENING_PIPE ),
			 printer.printProgram() );
	    perror( buf );
	    return;
	}
    }

    /* Make sure layout saves the current state. */
    LO_SaveFormData( mwContext() );

    /* Hold on to the saved data. */
    XP_MEMCPY( &saved_data, &url->savedData, sizeof( SHIST_SavedData ) );

    /* Make damn sure the form_data slot is zero'd or else all
     * hell will break loose. (Original comment)
     *
     * Whatever that means. (Kalle)
     */
    XP_MEMSET( &url->savedData, 0, sizeof( SHIST_SavedData ) );
    NPL_PreparePrint( mwContext(), &url->savedData );

    INTL_CharSetIDToName( INTL_GetCSIWinCSID( c ), mimecharset );
	
    //#warning We might have to do something with the resources here (see fe_Print). Kalle.


//
// I commented this out cause I could not build with gcc 2.7.2 -ramiro
//
// FIXME
	assert( 0 );

//    p.otherFontName = 0;

    if( print_to_file )
	p.completion = ps_file_close;
    else
	p.completion = ps_pipe_close;

    p.carg = mwContext();

    fe_RaiseSynchronousURLDialog( mwContext(), browserWidget, XP_GetString( QTFE_DIALOGS_PRINTING ) );
    XL_TranslatePostscript( mwContext(), url, &saved_data, &p );
    fe_AwaitSynchronousURL( mwContext() );
}


#if defined(XP_WIN)
inline double rint(double v)
{
    return ( v > 0 ) ? int(v+0.5) : int(v-0.5);
}
#endif

static void QTFE_InitializePrintSetup( QPrinter* printer, PrintSetup* p )
{
    XL_InitializePrintSetup( p );

    p->reverse = ( printer->pageOrder() == QPrinter::LastPageFirst ) ?
	true : false;
    p->color = ( printer->colorMode() == QPrinter::Color ) ?
	true : false;
    p->landscape = ( printer->orientation() == QPrinter::Landscape ) ?
	true : false;
    // fat chance  --Arnt #warning Add n-up here after Arnt has implemented it.

    p->bigger = 0;

    switch( printer->pageSize() ) {
    case QPrinter::A4:
	p->width  = (int)rint(210 * 0.039 * 72);
	p->height = (int)rint(297 * 0.039 * 72);
	break;
    case QPrinter::B5:
	p->width  = (int)rint(182 * 0.039 * 72);
	p->height = (int)rint(257 * 0.039 * 72);
	break;
    case QPrinter::Letter:
	p->width  = (int)rint(216 * 0.039 * 72);
	p->height = (int)rint(279 * 0.039 * 72);
	break;
    case QPrinter::Legal:
	p->width  = (int)rint(216 * 0.039 * 72);
	p->height = (int)rint(356 * 0.039 * 72);
	break;
    case QPrinter::Executive:
	p->width  = (int)rint(191 * 0.039 * 72);
	p->height = (int)rint(254 * 0.039 * 72);
	break;
    }
}


void ps_pipe_close( PrintSetup* p )
{
    fe_synchronous_url_exit( p->url, 0, (MWContext*) p->carg );
#if defined(XP_WIN)
    fclose( p->out );
#else
    pclose( p->out );
#endif
}

void ps_file_close( PrintSetup* p )
{
    fe_synchronous_url_exit( p->url, 0, (MWContext*) p->carg );
    fclose( p->out );
}
