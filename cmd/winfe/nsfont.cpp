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

// implement CNetscapeFontModule
#include "stdafx.h"

#ifndef XP_WIN
#define XP_WIN
#endif

#ifdef Displayer_SDK
	#define CASTINT(a) ((int)(a))
#endif

#include "nsfont.h"

#include "nf.h"         // for nfSpacingProportional
#include "Mnff.h"       // for nff_GetRenderableFont
#include "Mnfrf.h"      // for 	struct nfrf     *pRenderableFont;
#include "Mnffmi.h"
#include "Mnfrc.h"


#include "Mnffbp.h"
#include "Mnffbc.h"
#include "Mnffbu.h"
#include "Mnfdoer.h"

#include "prefapi.h"		// for PREF_GetIntPref

extern "C" char *FE_GetProgramDirectory(char *buffer, int length);

// XXX Arthur todo
// These directory names should be relative to preferance.
#define WF_FONT_CATALOG			"dynfonts\\fonts.cat"
#define WF_FONT_DISPLAYER_PATH	"dynfonts"

char *CNetscapeFontModule::converGenericFont(char *pExtracted)
{
	if( stricmp( pExtracted, "serif" ) == 0) 
		return( DEF_PROPORTIONAL_FONT );   //  "Times New Roman" defined in defaults.h

	if( stricmp( pExtracted, "sans-serif" ) == 0)  return( "Arial" );
	if( stricmp( pExtracted, "cursive" ) == 0) return( "Script" );
	if( stricmp( pExtracted, "fantasy" ) == 0) return( "Arial" );
	if( stricmp( pExtracted, "monospace" ) == 0) return( DEF_FIXED_FONT ); // "Courier New"

	return( pExtracted );  // return the origenal.
}

char * CNetscapeFontModule::convertToFmiCharset( BYTE charSetID )
{
	switch( charSetID )
	{
		case ANSI_CHARSET:			return ("iso-8859-1");
		case DEFAULT_CHARSET:		return ("DEFAULT_CHARSET");
		case SYMBOL_CHARSET:		return ("adobe-symbol-encoding");
		case SHIFTJIS_CHARSET:		return ("Shift_JIS");
		case HANGEUL_CHARSET:		return ("EUC-KR");
		case CHINESEBIG5_CHARSET:	return ("big5");
		case OEM_CHARSET:			return ("OEM_CHARSET");
		//	We cannot use xxx_CHARSET for the rest since they are not 
		//	defined for Win16 
		case 130: return ("JOHAB_CHARSET");
		case 134: return ("gb2312");	// not defined for 16 bit
		case 177: return ("HEBREW_CHARSET");
		case 178: return ("ARABIC_CHARSET");
		case 161: return ("windows-1253");
		case 162: return ("iso-8859-9");
		case 222: return ("x-tis620");
		case 238: return ("windows-1250");
		case 204: return ("windows-1251");
		default:  return ("DEFAULT_CHARSET");
	}

}

int CNetscapeFontModule::convertToFmiPitch( LOGFONT *pLogFont )
{
    if( pLogFont->lfPitchAndFamily == FIXED_PITCH )
        return( nfSpacingMonospaced );

    if( pLogFont->lfPitchAndFamily == VARIABLE_PITCH )
        return( nfSpacingProportional );

    return( nfSpacingDontCare );
}

int CNetscapeFontModule::convertToFmiStyle( LOGFONT *pLogFont )
{
    //TODO
    if( pLogFont->lfItalic )
        return( nfStyleItalic );

    // todo map:    lfUnderline, lfStrikeOut
    //      to      nfStyleNormal, nfStyleOblique
    return( nfStyleDontCare );
}

struct nffmi*
CNetscapeFontModule::createFMIfromLOGFONTWithAttr( LOGFONT *pLogFont, int YPixelPerInch, int extraAttributes  )
{

    nffmi*  pFmi;
    char    *FmiName;
    char    *FmiCharset;
    char    *FmiEncoding;
    int     FmiWeight;
    int     FmiPitch;
    int     FmiStyle;
	int		FmiUnderline;
	int     FmiOverstrike;
    int     FmiPixelPerInchX;

    FmiName         = pLogFont->lfFaceName;
    FmiCharset      = convertToFmiCharset( pLogFont->lfCharSet );
    FmiEncoding     = extraAttributes ? "Unicode": FmiCharset;
    FmiWeight       = pLogFont->lfWeight;
    FmiPitch        = convertToFmiPitch( pLogFont );
    FmiStyle        = convertToFmiStyle( pLogFont ); 
	FmiUnderline	= pLogFont->lfUnderline? nfUnderlineYes : nfUnderlineDontCare;
	FmiOverstrike	= pLogFont->lfStrikeOut? nfStrikeOutYes : nfStrikeOutDontCare;
    FmiPixelPerInchX  = 0;
    

    pFmi = nffbu_CreateFontMatchInfo(
		m_pFontBrokerUtilityInterface, 
        FmiName,
        FmiCharset,
        FmiEncoding,
        FmiWeight,
		FmiPitch,
        FmiStyle,
		FmiUnderline,
		FmiOverstrike,
		FmiPixelPerInchX,                          
        YPixelPerInch,                          
		NULL /* exception */
		);

    return( pFmi );        
}


//////////////////////////////////////////////////////////////////////////////////
//  Implement class CNetscapeFontModule

CNetscapeFontModule::CNetscapeFontModule()
{
    m_pFontBrokerDisplayerInterface  = NULL; /* The FontBroker Displayer interface */
    m_FontBrokerConsumerInterface   = NULL; /* The FontBroker Consumer interface */
    m_pFontBrokerUtilityInterface   = NULL; /* The FontBroker Utility interface */

    m_pFontDisplayerInterface        = NULL; /* The FontDisplayer interface */

	m_fontModuleStatus				= FMSTATUS_NULL;
 
}

CNetscapeFontModule::~CNetscapeFontModule()
{
	if( m_workingRC )
		nfrc_release( m_workingRC, NULL);
	m_workingRC = NULL;

	// Save the catalog
	if (m_pFontBrokerUtilityInterface) {
		char 	exeDir_buffer[1024];
		FE_GetProgramDirectory( exeDir_buffer, 512 );
	
		if( *exeDir_buffer ) {
			strcat( exeDir_buffer, WF_FONT_CATALOG );
			nffbu_SaveCatalog(m_pFontBrokerUtilityInterface, exeDir_buffer, NULL);  
		}
    }
    
    releaseDisplayer();
}

int CNetscapeFontModule::InitFontModule( )
{
	if( m_fontModuleStatus != FMSTATUS_NULL )
		return( FONTERR_OK );			// already initialized

	if( createObjects() ) 
		return( FONTERR_CreateObjectFail );

	if ( getInterfaces() )
		return( FONTERR_GetInterfacesFail );

    if ( registerFontDisplayer() )
		return( FONTERR_RegisterFontDisplayerFail );
    
	m_fontModuleStatus = FMSTATUS_IntialOK;

   	// create the RC object
	m_workingRC			= convertDC(NULL);

	return( FONTERR_OK );
}

int CNetscapeFontModule::createObjects()
{
	int ret = 0;

	/* Create the FontBroker Object */

	m_FontBrokerConsumerInterface = NF_FontBrokerInitialize();
	if (!m_FontBrokerConsumerInterface)
		ret = -1;

	return (ret);

}   // int CNetscapeFontModule::createObjects()

int CNetscapeFontModule::getInterfaces()
{
	int ret = 0;

	/* Get all the FontBroker interfaces */
	m_pFontBrokerDisplayerInterface = (struct nffbp *) nffbc_getInterface(m_FontBrokerConsumerInterface, &nffbp_ID, NULL);
	if (!m_pFontBrokerDisplayerInterface )
	  {
		ret--;
	  }

	m_pFontBrokerUtilityInterface = (struct nffbu *) nffbc_getInterface(m_FontBrokerConsumerInterface, &nffbu_ID, NULL);
	if (!m_pFontBrokerUtilityInterface)
	  {
		ret--;
	  }

   	/* Create the FontDisplayer Object, passing the broker interface to Displayer */
	m_pFontDisplayerInterface = (struct nffp *) winfpFactory_Create(NULL, m_pFontBrokerDisplayerInterface );
	if (!m_pFontDisplayerInterface)
	  {
		ret--;
	  }
    
    return (ret);

}   // int CNetscapeFontModule::getInterfaces()

/* Register the Displayer with the broker */
int CNetscapeFontModule::registerFontDisplayer()
{
	int		reCode ;
	int		ndisplayer = 0;

	char 	exeDir_buffer[513], dir_buffer[1024];
	FE_GetProgramDirectory( exeDir_buffer, 512 );

#ifdef WIN32
	// not working on win16
	if( *exeDir_buffer ) {
		strcat( strcpy(dir_buffer,exeDir_buffer), WF_FONT_CATALOG );
		// Load the font catalogs
		nffbu_LoadCatalog(m_pFontBrokerUtilityInterface, dir_buffer, NULL);
	}
#endif
	
	// Register native windows displayer
    reCode = (int) nffbp_RegisterFontDisplayer(m_pFontBrokerDisplayerInterface, m_pFontDisplayerInterface, NULL);
	if (reCode >= 0)
	{
		ndisplayer = 1;
	}

	/* 
    0    : always ignore document fonts 
    1    : use document fonts but ignore webfonts 
    2    : use document fonts and webfonts (default) 
	*/
	if(*exeDir_buffer ) {
		// Scan other displayers               
		strcat( strcpy(dir_buffer,exeDir_buffer), WF_FONT_DISPLAYER_PATH );
	    reCode = (int) nffbp_ScanForFontDisplayers(m_pFontBrokerDisplayerInterface, dir_buffer, NULL);
		if (reCode > 0)
		{
			ndisplayer += reCode;
		}
	}

	if (ndisplayer <= 0)
	{
		ASSERT( ndisplayer > 0 ); // XXX Aaagh. NO displayer. Alert user and exit.
	}

	return( 0);		// arthur 2/19 reCode );
}

int CNetscapeFontModule::releaseAllFontResource(MWContext *pContext)
{
	return(0);
}

int CNetscapeFontModule::LookupFailed(MWContext *pContext, struct nfrc *prc, struct nffmi * pfmi)
{
	if( m_pFontBrokerUtilityInterface == NULL )
		return(-1);

	nffbu_LookupFailed(m_pFontBrokerUtilityInterface, pContext,prc, pfmi, NULL);
	return(0);
}

// this function mallocate space. When done with the returned pointer
// call nfrc_release( pRenderingContext, NULL);
struct nfrc	* CNetscapeFontModule::convertDC( HDC hDC )
{
	struct nfrc		*tempRenderingContext;
   	void            *rcbuf[4]; 

	if( m_fontModuleStatus == FMSTATUS_NULL ) {
		m_lastError = FONTERR_NotInitiated;
		return(NULL );
	}

	rcbuf[0] = (void *)hDC;         // windows specific rendering context
	rcbuf[1] = 0;           // terminator
	tempRenderingContext = nffbu_CreateRenderingContext(m_pFontBrokerUtilityInterface, NF_RC_DIRECT, 0, rcbuf, 1,
									  NULL /* exception */);

	if (tempRenderingContext == NULL) {
		m_lastError = FONTERR_ConvertDCFailed;
		return( NULL );
	}
	
	return( tempRenderingContext );
}

int	CNetscapeFontModule::WebfontsNeedReload( MWContext *pContext )
{
	int nn;
	nn = (int) nffbu_WebfontsNeedReload(m_pFontBrokerUtilityInterface, pContext, NULL);
	return(nn);
}

int	CNetscapeFontModule::UpdateWorkingRC( HDC hDC )
{
	struct rc_data  RenderingContextData;
	RenderingContextData = nfrc_GetPlatformData( m_workingRC, NULL/*exception*/ ); 
	RenderingContextData.t.directRc.dc = (void *)hDC;
	nfrc_SetPlatformData( m_workingRC, &RenderingContextData, NULL /*exception*/ ); 
	return( FONTERR_OK );
}

int
CNetscapeFontModule::FreeFMIList( struct nffmi ** fmiList)
{
	nffbu_free( getBrokerUtilityInterface(), fmiList, NULL );
	return( FONTERR_OK );
}

// this function malloc space. After done with the list,
// call CNetscapeFontModule::FreeFMIList()
struct nffmi **CNetscapeFontModule::CreateFMIList(HDC hDC )
{
	struct nffmi **pTempFmiList;
   	struct nffmi    *pWantedFmi;

	if( m_fontModuleStatus == FMSTATUS_NULL ) {
		m_lastError = FONTERR_NotInitiated;
		return( NULL );
	}

	if( UpdateWorkingRC( hDC ) ) 
		return(NULL);			// error

	// Create a FMI for the font of interest 
    // Not really used by Displayer, because it has hardcoded the font and don't do any matching.
	pWantedFmi = nffbu_CreateFontMatchInfo(
		        m_pFontBrokerUtilityInterface, 
                "Arial",                  // "nfFmiName"
                "iso8859",                  // "nfFmiCharset"
                "1",                        // "nfFmiEncoding"
                400,                        // "nfFmiWeight"
				nfSpacingDontCare,          // "nfFmiPitch"     nfSpacingProportional,
                nfStyleDontCare,            // "nfFmiStyle"     nfStyleDontCare, nfStyleItalic
				0,							// FmiUnderline,
				0,							// FmiOverstrike,
				0,                          // "nfFmiPixelPerInchX"
                0,                          // "nfFmiPixelPerInchY"
				NULL /* exception */);
	if (pWantedFmi == NULL) { 
		m_lastError = FONTERR_CreateFontMatchInfoFailed;
		return( NULL );
	  }


    pTempFmiList = (struct nffmi **)nffbc_ListFonts(m_FontBrokerConsumerInterface, m_workingRC, pWantedFmi, NULL /* exception */);

    // clean up 
    nffmi_release( pWantedFmi, NULL);

	return( pTempFmiList );

}

int CNetscapeFontModule::releaseDisplayer()
{

    if( m_pFontDisplayerInterface != NULL )
        nffp_release( m_pFontDisplayerInterface, NULL);
    m_pFontDisplayerInterface = NULL;
    
    return( FONTERR_OK );
}


///////////////////////////////////////////////////////////////////////////////////
// class Cyafont : yet another font class

extern CNetscapeFontModule    theGlobalNSFont;

CyaFont::CyaFont()
{
	m_pRenderableFont	= NULL;
	m_iMeanWidth		= 0;

#ifdef DEBUG_aliu0
	selectFlag			= 0;
#endif

	// create the RC object
	m_displayRC = NULL;
	m_displayRC = theGlobalNSFont.convertDC(NULL);
	ASSERT( m_displayRC != NULL );
#ifndef NO_PERFORMANCE_HACK
	m_displayRCData = NULL;
	if (m_displayRC != NULL)
	{
		m_displayRCData = NF_GetRCNativeData(m_displayRC);
	}
#endif
}

CyaFont::~CyaFont()
{
	if( m_pRenderableFont != NULL )
        nfrf_release( m_pRenderableFont, NULL);
	m_pRenderableFont = NULL;

	if( m_displayRC )
		nfrc_release( m_displayRC, NULL);
	m_displayRC = NULL;
#ifndef NO_PERFORMANCE_HACK
	m_displayRCData = NULL;
#endif /* NO_PERFORMANCE_HACK */
}

// this interface is for a quick integrating with Navigator
// back-end xp code should use low level interface.
//
// this function allocate space, the destructor for m_pRenderableFont,
// need to free the space.
int	CyaFont::CreateNetscapeFontWithLOGFONT(
	   MWContext *pContext,
	   HDC hDC, LOGFONT *pLogFont, int extraAttributes /* =0 */  )
{
   	struct nffmi    *pWantedFmi;
	int				YPixelPerInch = ::GetDeviceCaps(hDC, LOGPIXELSY);

	pWantedFmi = theGlobalNSFont.createFMIfromLOGFONTWithAttr( pLogFont, YPixelPerInch, extraAttributes) ;

	if (pWantedFmi == NULL)
		return( FONTERR_CreateFontMatchInfoFailed );

	// remember the lfPitchAndFamily, cannot get it from FMI easily
	m_pitch = theGlobalNSFont.convertToFmiPitch( pLogFont );

	int reCode = CreateNetscapeFontWithFMI(
		pContext,
		hDC, pWantedFmi, pLogFont->lfHeight / YPixelPerInch ) ;
	CalculateMeanWidth( hDC, extraAttributes );
	return( reCode );
}

// this function allocate space, the destructor for m_pRenderableFont,
// need to free the space.
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
    int     FmiPixelPerInchX,
    int     FmiPixelPerInchY,
	double	fontHeight				// not a fmi field
	)
{
   	struct nffmi    *pWantedFmi;
	int				reCode;

	// remember the lfPitchAndFamily, cannot get it from FMI easily
	m_pitch = FmiPitch;

    pWantedFmi = nffbu_CreateFontMatchInfo( 
		theGlobalNSFont.getBrokerUtilityInterface(),
        FmiName,
        FmiCharset,
        FmiEncoding,
        FmiWeight,
		FmiPitch,
        FmiStyle,
		FmiUnderline,
		FmiOverstrike,
		FmiPixelPerInchX,                          
        FmiPixelPerInchY,                          
		NULL				/* exception */
		);

	if (pWantedFmi == NULL)
		return( FONTERR_CreateFontMatchInfoFailed );
	
	reCode = CreateNetscapeFontWithFMI(pContext, hDC, pWantedFmi, fontHeight );
    nffmi_release( pWantedFmi, NULL);

	return( reCode); 

}

// this function allocate space, the destructor for m_pRenderableFont,
// need to free the space.
int	CyaFont::CreateNetscapeFontWithFMI(
	MWContext *pContext,
	HDC hDC, struct nffmi *pWantedFmi, double fontHeight )
{
	struct nff      *genericFontHandle;

	if( m_pRenderableFont != NULL )
        nfrf_release( m_pRenderableFont, NULL);
    m_pRenderableFont = NULL;
	
	if( UpdateDisplayRC( hDC ) ) 
		return(1);			// error

  	// Create a FMI for the font of interest 
    // Not really used by Displayer, because it has hardcoded the font and don't do any matching.
	
	if (pWantedFmi == NULL)
		return( FONTERR_CreateFontMatchInfoFailed );
	
	/* Find the url that is loading this font */
    const char *accessing_url_str = NULL;
	if( pContext ) {
		History_entry *he = SHIST_GetCurrent(&(pContext->hist));
		if( he )
			accessing_url_str = he->address;
	}

	// Look for a font 
	genericFontHandle = nffbc_LookupFont(
		   theGlobalNSFont.getFontBrokerConsumerInterface(), 
		   m_displayRC,
		   pWantedFmi,
		   accessing_url_str, /* XXX GetContext()->url */
		   NULL /* exception */);

	if (genericFontHandle == NULL) {
		// failed
		theGlobalNSFont.LookupFailed( pContext, m_displayRC, pWantedFmi);
		return( FONTERR_nffbc_LookupFontFailed );

	}

	if( fontHeight < 0 )
		fontHeight = - fontHeight;		// webfont use positive size, see winfp.cpp

	/* Create a rendering font */
	m_pRenderableFont = nff_GetRenderableFont( genericFontHandle, m_displayRC, fontHeight, NULL /* exception */);
	
    // clean up 
    nff_release( genericFontHandle , NULL );


    return( FONTERR_OK );
}   // int	CyaFont::CreateNetscapeFontWithFMI( HDC hDC )

int CyaFont::GetAscent()	{ return(CASTINT(NULL==m_pRenderableFont?0:nfrf_GetFontAscent( m_pRenderableFont,  NULL ) )); }
int CyaFont::GetDescent()	{ return(CASTINT(NULL==m_pRenderableFont?0:nfrf_GetFontDescent( m_pRenderableFont, NULL ) )); }
int CyaFont::GetMeanWidth() { return( m_iMeanWidth ); }
int CyaFont::GetHeight()    { return( (int) ( GetAscent() + GetDescent() )); }

int CyaFont::GetMaxWidth()  
{ 
	int wid = CASTINT(NULL==m_pRenderableFont?0:nfrf_GetMaxWidth( m_pRenderableFont, NULL ) );
	return(wid); 
}

jint CyaFont::MeasureTextWidth( HDC hDC, jbyte *str, jsize strLen, jint *charLocs, jsize charLoc_len )
{
	jint  len, chrSpaceing;
	if( m_pRenderableFont == NULL )
		return -1;

	if( UpdateDisplayRC( hDC ) ) 
			return( -2 );			// error

	if( strLen <= 0 )
		return(0);

	len = nfrf_MeasureText( m_pRenderableFont, m_displayRC, chrSpaceing, str, strLen, charLocs, charLoc_len, NULL );
	return(len);
}

BOOL CyaFont::MeasureTextSize( HDC hDC, jbyte *str, jsize strLen, jint *charLocs, jsize charLoc_len, LPSIZE lpSize)
{
	lpSize->cx = CASTINT(MeasureTextWidth( hDC, str, strLen, charLocs, charLoc_len ));
	lpSize->cy = GetHeight();
	return( lpSize->cx >= 0? TRUE : FALSE );
}
// this is a mimic of Navigator code in CNetscapeFont::Initialize()
// return -1 if failed.
int CyaFont::CalculateMeanWidth(  HDC hDC, BOOL isUnicode )
{
	// calculate the mean width
	m_iMeanWidth	= 0;
	if( isUnicode ) {
		m_iMeanWidth = CASTINT(MeasureTextWidth( hDC, "\x0W\x0w", 4, NULL, 0 ));
	} else {
		m_iMeanWidth = CASTINT(MeasureTextWidth( hDC, "Ww", 2, NULL, 0 ));
	}
	m_iMeanWidth = m_iMeanWidth / 2 ;

	return( m_iMeanWidth );
}

int	CyaFont::UpdateDisplayRC( HDC hDC )
{
#ifndef NO_PERFORMANCE_HACK
	m_displayRCData->t.directRc.dc = (void *)hDC;
#else
	ASSERT( m_displayRC != NULL );
	struct rc_data  RenderingContextData;
	RenderingContextData = nfrc_GetPlatformData( m_displayRC, NULL/*exception*/ ); 
	RenderingContextData.t.directRc.dc = (void *)hDC;
	nfrc_SetPlatformData( m_displayRC, &RenderingContextData, NULL /*exception*/ ); 
#endif /* NO_PERFORMANCE_HACK */
	return( FONTERR_OK );
}

// PrepareDrawText() and EndDrawText() must be called in pair!
int	CyaFont::PrepareDrawText( HDC hDC )
{
#ifdef DEBUG_aliu0
	ASSERT( selectFlag == 0 );
	selectFlag = 1;
#endif
	
	if( NULL == m_pRenderableFont )
		return(1);		// todo

	if( UpdateDisplayRC( hDC ) )
		return(1);			// error

	nfrf_PrepareDraw( m_pRenderableFont, m_displayRC, NULL /*exception*/ ); 
	return( FONTERR_OK );
}

// PrepareDrawText() and EndDrawText() must be called in pair!
int	CyaFont::EndDrawText( HDC hDC )
{
#ifdef DEBUG_aliu0
	ASSERT( selectFlag == 1 );
	selectFlag = 0;
#endif

	if( NULL == m_pRenderableFont ) 
		return( FONTERR_OK );
	if( UpdateDisplayRC( hDC ) ) 
		return(1);			// error 

	nfrf_EndDraw( m_pRenderableFont, m_displayRC, NULL /*exception*/ ); 
	return( FONTERR_OK );
}
					  
int	CyaFont::drawText( HDC hDC, int xx, int yy, char *str, int strLen)
{
    if( m_pRenderableFont == NULL )
        return(-1);

	if( UpdateDisplayRC( hDC ) ) 
		return(1);			// error 

	jint	chrSpaceing;
    // draw text through the renderable font
    nfrf_DrawText( m_pRenderableFont, m_displayRC, xx, yy, chrSpaceing, str, strLen, NULL );

    return( FONTERR_OK );

}
