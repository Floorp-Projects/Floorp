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

#ifndef __NSFONT_H__
#define __NSFONT_H__

#include "nf.h"

/*
 * Uses:
 *	FontBrokerInterfaces (nffbc, nffbp, nffbu)
 */
#include "Mnffbc.h"
#include "Mnffbp.h"
#include "Mnffbu.h"
#include "Mnffp.h"

#include "Mnffmi.h"		// for nffmi_GetValue() nffmi_release()

#include "Mwinfp.h"     // in ns\modules\libfont\src\_jmc  
#include <windows.h>        // for CDC

enum FontModuleReturnCode {
	FONTERR_OK   = 0 ,
	FONTERR_ConvertDCFailed,
	FONTERR_CreateFontMatchInfoFailed,
	FONTERR_CreateObjectFail,
	FONTERR_GetInterfacesFail,
	FONTERR_nffbc_LookupFontFailed,
	FONTERR_NotInitiated,
	FONTERR_RegisterFontDisplayerFail,
	
};
	


class CNetscapeFontModule : public CObject
{
public :
    CNetscapeFontModule::CNetscapeFontModule();
    CNetscapeFontModule::~CNetscapeFontModule();

	// must be called first
	int			CNetscapeFontModule::InitFontModule();  

	// error handling 
	int         getLastError() { return(m_lastError); }

	// general service functions
	struct nfrc		*CNetscapeFontModule::convertDC( HDC hDC );
	struct nffbu	*getBrokerUtilityInterface() { return( m_pFontBrokerUtilityInterface ); }
	struct nffbc	*getFontBrokerConsumerInterface(){ return(m_FontBrokerConsumerInterface); }

	struct nffmi	**CNetscapeFontModule::CreateFMIList(HDC hDC );
	int				CNetscapeFontModule::FreeFMIList( struct nffmi ** fmiList);

	int CNetscapeFontModule::releaseAllFontResource(MWContext *pContext);
	int CNetscapeFontModule::LookupFailed(MWContext *pContext, struct nfrc *prc, struct nffmi * pfmi);
	int	CNetscapeFontModule::WebfontsNeedReload( MWContext *pContext );

	
private :
    int createObjects();
    int getInterfaces();
    int CNetscapeFontModule::registerFontDisplayer();
    int CNetscapeFontModule::releaseDisplayer();
	int	CNetscapeFontModule::UpdateWorkingRC( HDC hDC );

	int	m_fontModuleStatus;
	int	m_lastError;
	enum fontModuleStatus {
		FMSTATUS_NULL = 0,
		FMSTATUS_IntialOK,
	};

	// helpers
public :
	struct nffmi* CNetscapeFontModule::createFMIfromLOGFONTWithAttr( LOGFONT *pLogFont, 
		          int pixelPerPoint, int extraAttributes );

	// the following 3 helpers are identical as in producers\win\winfp.c 
	char * CNetscapeFontModule::convertToFmiCharset( BYTE charSetID );  // used to be LOGFONT *pLogFont
	int CNetscapeFontModule::convertToFmiStyle( LOGFONT *pLogFont );
	int CNetscapeFontModule::convertToFmiPitch( LOGFONT *pLogFont );
	char *CNetscapeFontModule::converGenericFont(char *pExtracted);

public :
    struct cfb *m_pGlobalFontBrokerObject; /* The global FontBroker Object */

    struct nffbp *m_pFontBrokerDisplayerInterface; /* The FontBroker Displayer interface */
    struct nffbc *m_FontBrokerConsumerInterface; /* The FontBroker Consumer interface */
    struct nffbu *m_pFontBrokerUtilityInterface; /* The FontBroker Utility interface */

    //struct cfp *m_pDisplayerObj;                 /* The global FontDisplayer Object */
    struct nffp  *m_pFontDisplayerInterface;      /* The FontDisplayer interface */

private :
	struct nfrc  *m_workingRC;			// avoid creating rc every time

};	// class CNetscapeFontModule


///////////////////////////////////////////////////////////////////////////////////
// class Cyafont : yet another font class
class CyaFont : public CObject
{
public :
	// the standard stuff
	CyaFont::CyaFont();
	CyaFont::~CyaFont();
	BOOL    CyaFont::IsEmptyFont() { return( m_pRenderableFont==NULL?TRUE:FALSE );	 }

	// initializer
	int	CyaFont::CreateNetscapeFontWithFMI(
			MWContext *pContext,
			HDC hDC, struct nffmi *pWantedFmi, double fontHeight );	
	int	CyaFont::CreateNetscapeFont(
		MWContext *pContext,
		HDC		hDC, 
		const char    *FmiName,
		const char    *FmiCharset,
		const char    *FmiEncoding,
		int     FmiWeight,
		int     FmiPitch,
		int     FmiStyle,
		int		FmiUnderline,
		int     FmiOverstrike,
		int     FmiPixelPerPointX,
		int     FmiPixelPerPointY,
		double	fontHeight				// not fmi field
		);

	// the following 2 interfaces are for a quick integration with Navigator
	// new code should use lower level interface.
	int	CyaFont::CreateNetscapeFontWithLOGFONT( 
		MWContext *pContext,
		HDC hDC, LOGFONT *pLogFont, int extraAttributes = 0 );

	// service 
	int		CyaFont::PrepareDrawText( HDC hDC );
	int		CyaFont::EndDrawText( HDC hDC );
	int		drawText( HDC hDC,int xx, int yy, char *str, int strLen);

	// attributes
	int CyaFont::IsFixedFont() { return( m_pitch == nfSpacingProportional )? 0 : 1; }  // return 1 for nfSpacingDontCare
	int CyaFont::GetAscent();
	int CyaFont::GetDescent();
	int CyaFont::GetMeanWidth();
	int CyaFont::GetMaxWidth();
	int CyaFont::GetHeight();
	jint CyaFont::MeasureTextWidth( HDC hDC, jbyte *str, jsize strLen, jint *charLocs, jsize charLoc_len );
	BOOL MeasureTextSize( HDC hDC, jbyte *str, jsize strLen, jint *charLocs, jsize charLoc_len, LPSIZE lpSize);
	int CyaFont::CalculateMeanWidth(  HDC hDC, BOOL bFixed );


private :
	// helper functions
	int	CyaFont::UpdateDisplayRC( HDC hDC );


private :
	struct nfrf		*m_pRenderableFont;
	struct nfrc		*m_displayRC;			// avoid creating rc every time.
#ifndef NO_PERFORMANCE_HACK
	struct rc_data  *m_displayRCData;		// For performance of measure and draw
#endif /* NO_PERFORMANCE_HACK */
	int				m_iMeanWidth;
	int				m_pitch;

#ifdef DEBUG_aliu
	int				selectFlag;
#endif

};	// class CyaFont

#endif
