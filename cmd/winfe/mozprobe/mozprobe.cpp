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

/* mozprobe.cpp :
 *
 * This is the WinFE implementation of mozprobe.dll.  This DLL provides 
 * dynamic "client" and "server" support for the layout probe API to be
 * accessed from third-party applications.
 *
 * The "server" piece is the function mozProbeServerProc.  It
 * is dynamically linked to by mozilla.exe (see hiddenfr.cpp) and called
 * when Mozilla's hidden frame received certain WM_COPYDATA requests.
 *
 * The "client" piece is implemented in two layers.  You can use objects
 * of the C++ class MozillaLayoutProbe.  Or, you can use the less elegant
 * C APIs that are declared identically to the real layout probe APIs in
 * layprobe.h.  The latter are intended to be linked to dynamically using
 * LoadLibrary and GetProcAddr.
 *
 * This file entirely encapsulates the interface/protocol between these
 * two components.  See the implementation below for details.
 */

#include <string.h>
#include <stdio.h>

#include <windows.h>

#include "mozprobe.h"

/*
 * Format for input/output area passed via lpData field in COPYDATASTRUCT.
 */
struct PROBEAPISTRUCT {
    long          lProbeID;        // Probe ID.  Returned on NSCP_Probe_Create.
    unsigned long ulSharedMemSize; // Length of shared memory result buffer.
    void*         data;            // Unused (at the moment).
};

/* copyResultToSharedMem
 *
 * This utility is used to copy LONG result values obtained from various LO_QA_*
 * APIs into the shared memory buffer allocated by the client component.  We use
 * the convention that the mapping file name is "MozillaLayoutProbe" followed
 * by the probeID.
 */
static const char * const mappingNameTemplate = "MozillaLayoutProbe%ld";

static BOOL copyResultToSharedMem( long lResult, long probeID ) {
    BOOL rc = FALSE;
    // Access shared memory (aka memory-mapped file).
    char mappingName[ _MAX_PATH ];
    sprintf( mappingName, mappingNameTemplate, probeID );

    HANDLE hSharedMem = OpenFileMapping( FILE_MAP_WRITE, FALSE, mappingName );

    if ( hSharedMem ) {
        // Map the file.
        long *pResult = (long*)MapViewOfFile( hSharedMem, FILE_MAP_WRITE, 0, 0, 0 );
        if ( pResult ) {
            // Copy the result to the shared memory.
            rc = TRUE;
            *pResult = lResult;
            UnmapViewOfFile( pResult );
        }
        CloseHandle( hSharedMem );
    }
    return rc;
}

/* mozProbeServerProc
 *
 * This is the exported function which is called from
 * CHiddenFrame::OnProcessIPCHook to process layout probe
 * IPC requests.
 *
 * This function's implementation defines the protocol between the client component
 * and the server component (that is, the layout and semantics of the information
 * passed on the WM_COPYDATA messages).
 */
long mozProbeServerProc( WPARAM wparam, LPARAM lparam, PROBEAPITABLE *api ) {
    LONG rc = 0;
    BOOL bResult;
    LONG lResult;
    char *pText;
    struct MWContext_* pContext;

    COPYDATASTRUCT *pcds = (COPYDATASTRUCT*)lparam;

    PROBEAPISTRUCT *p = (PROBEAPISTRUCT*)( pcds ? pcds->lpData : 0 );

    if ( pcds && p ) {
        switch ( pcds->dwData ) {
    
            case NSCP_Probe_Create:
                // Get the MWContext corresponding to the ordinal passed in.
                pContext = api->Ordinal2Context( p->lProbeID );

                // If the context is valid, use it to create a probe.
                if ( pContext ) {
                    // Create the probe, returning its ID as result.
                    rc = api->LO_QA_CreateProbe( pContext );
                }
                break;
    
            case NSCP_Probe_Destroy:
                // Destroy the probe.
                api->LO_QA_DestroyProbe( p->lProbeID );
    
                // Presume it succeeded.
                rc = 1;
                break;
    
            case NSCP_Probe_GotoFirstElement:
                // Position probe at first element.
                // Return FALSE if it fails, TRUE if successful.
                rc = api->LO_QA_GotoFirstElement( p->lProbeID );
                break;
    
            case NSCP_Probe_GotoNextElement:
                // Position probe at next element.
                // Return FALSE if it fails, TRUE if successful.
                rc = api->LO_QA_GotoNextElement( p->lProbeID );
                break;
    
            case NSCP_Probe_GotoChildElement:
                // Position probe at first element in child (if possible).
                // Returns FALSE if it fails, TRUE if successful.
                rc = api->LO_QA_GotoChildElement( p->lProbeID );
                break;
    
            case NSCP_Probe_GotoParentElement:
                // Position probe at parent of current element (if possible).
                // Returns FALSE if it fails, TRUE if successful.
                rc = api->LO_QA_GotoParentElement( p->lProbeID );
                break;
    
            case NSCP_Probe_GetElementType:
                // Get type of element at current probe position.
                // Returns FALSE if it fails.
                rc = 0;
                api->LO_QA_GetElementType( p->lProbeID, (int*)&rc );
                break;
    
            case NSCP_Probe_GetElementXPosition:
                // Get the horizontal offset of the current element.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The result value is returned through shared memory.
                rc = api->LO_QA_GetElementXPosition( p->lProbeID, &lResult );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            case NSCP_Probe_GetElementYPosition:
                // Get the vertical offset of the current element.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The result value is returned through shared memory.
                rc = api->LO_QA_GetElementYPosition( p->lProbeID, &lResult );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            case NSCP_Probe_GetElementWidth:
                // Get the width of the current element.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The result value is returned through shared memory.
                rc = api->LO_QA_GetElementWidth( p->lProbeID, &lResult );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            case NSCP_Probe_GetElementHeight:
                // Get the height of the current element.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The result value is returned through shared memory.
                rc = api->LO_QA_GetElementHeight( p->lProbeID, &lResult );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            case NSCP_Probe_ElementHasURL:
                // Query whether the current element has a URL associated with it.
                // Returns -1 if it fails, a TRUE or FALSE result if it succeeds.
                bResult = api->LO_QA_HasURL( p->lProbeID, (BOOL*)( &rc ) );
                if ( !bResult ) {
                    rc = -1;
                }
                break;
    
            case NSCP_Probe_ElementHasText:
                // Query whether the current element has text associated with it.
                // Returns -1 if it fails, a TRUE or FALSE result if it succeeds.
                bResult = api->LO_QA_HasText( p->lProbeID, (BOOL*)( &rc ) );
                if ( !bResult ) {
                    rc = -1;
                }
                break;
    
            case NSCP_Probe_ElementHasColor:
                // Query whether the current element has a color attribute.
                // Returns -1 if it fails, a TRUE or FALSE result if it succeeds.
                bResult = api->LO_QA_HasColor( p->lProbeID, (BOOL*)( &rc ) );
                if ( !bResult ) {
                    rc = -1;
                }
                break;
    
            case NSCP_Probe_ElementHasChild:
                // Query whether the current element has child elements (is a table/cell/layer).
                // Returns -1 if it fails, a TRUE or FALSE result if it succeeds.
                bResult = api->LO_QA_HasChild( p->lProbeID, (BOOL*)( &rc ) );
                if ( !bResult ) {
                    rc = -1;
                }
                break;
    
            case NSCP_Probe_ElementHasParent:
                // Query whether the current element has a parent element.
                // Returns -1 if it fails, a TRUE or FALSE result if it succeeds.
                bResult = api->LO_QA_HasParent( p->lProbeID, (BOOL*)( &rc ) );
                if ( !bResult ) {
                    rc = -1;
                }
                break;
    
            case NSCP_Probe_GetElementText:
                // Get the element's text.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The text string is returned in shared memory.
                if ( p->ulSharedMemSize ) {
                    // Access shared memory buffer.
                    char mappingName[ _MAX_PATH ];
                    sprintf( mappingName, mappingNameTemplate, p->lProbeID );
                
                    HANDLE hSharedMem = OpenFileMapping( FILE_MAP_WRITE, FALSE, mappingName );
                
                    if ( hSharedMem ) {
                        char *pResult = (char*)MapViewOfFile( hSharedMem, FILE_MAP_WRITE, 0, 0, 0 );
                
                        if ( pResult ) {
                            // Read text into shared memory buffer.
                            rc = api->LO_QA_GetText( p->lProbeID, pResult, p->ulSharedMemSize );

                            // Unmap the file.
                            UnmapViewOfFile( pResult );
                        }
                        // Close the mapping file.
                        CloseHandle( hSharedMem );
                    }
                }
                break;
    
            case NSCP_Probe_GetElementTextLength:
                // Get the length of the element's text.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The result is returned in shared memory.
                rc = api->LO_QA_GetTextLength( p->lProbeID, &lResult );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            case NSCP_Probe_GetElementColor:
                // Get the element's color.
                // Returns FALSE if it fails, TRUE if it succeeds.
                // The color value is returned in shared memory.
                rc = api->LO_QA_GetColor( p->lProbeID, &lResult, (PROBECOLORTYPE)(unsigned long)( p->data ) );
                if ( rc ) {
                    rc = copyResultToSharedMem( lResult, p->lProbeID );
                }
                break;
    
            default:
                // Unexpected API, return error.
                rc = 0;
                break;
        }
    }

    return rc;
}

static HWND mozHWND = 0;

/* MozillaLayoutProbe::MakeProbe
 *
 * Use "new" to create a probe from the context.
 */
MozillaLayoutProbe* MozillaLayoutProbe::MakeProbe( long context ) {
    return new MozillaLayoutProbe( context );
}

/* MozillaLayoutProbe::MozillaLayoutProbe
 *
 * The first time called, we locate the Mozilla hidden frame.
 * 
 * We create the probe by invoking the "create" layout probe
 * IPC request.
 */
MozillaLayoutProbe::MozillaLayoutProbe( long context )
    : m_lProbeID( 0 ),
      m_bOK( FALSE ),
      m_hSharedMem( 0 ),
      m_ulSharedMemSize( 0 ) {
    // If not yet hooked to mozilla, do so now.
    if ( !mozHWND ) {
        mozHWND = FindWindow( "aHiddenFrameClass", NULL );
    }

    // Create the probe and store the resulting ID.
    if ( mozHWND ) {
        m_lProbeID = context;
        m_lProbeID = sendRequest( NSCP_Probe_Create );
        m_bOK = !!m_lProbeID; // Bad if ID==0.
    }
}

/* MozillaLayoutProbe::~MozillaLayoutProbe
 *
 * Destroy the probe and clean up the mapping file.
 */
MozillaLayoutProbe::~MozillaLayoutProbe() {
    // Clean up state info inside Mozilla (destroy the probe).
    if ( m_lProbeID ) {
        sendRequest( NSCP_Probe_Destroy );
    }
    // Clean up shared memory.
    if ( m_hSharedMem ) {
        if ( m_pSharedMem ) {
            UnmapViewOfFile( m_pSharedMem );
        }
        CloseHandle( m_hSharedMem );
    }
}

/* MozillaLayoutProbe::position, MozillaLayoutProbe::Goto<X>Element
 *
 * All 4 positioning function simply send the request and record
 * whether it worked.
 */
BOOL MozillaLayoutProbe::position( PROBE_IPC_REQUEST req ) const {
    setOK( FALSE );
    if ( m_lProbeID ) {
        sendRequest( req );
    }
    return m_bOK;
}

BOOL MozillaLayoutProbe::GotoFirstElement() {
    return position( NSCP_Probe_GotoFirstElement );
}

BOOL MozillaLayoutProbe::GotoNextElement() {
    return position( NSCP_Probe_GotoNextElement );
}

BOOL MozillaLayoutProbe::GotoChildElement() {
    return position( NSCP_Probe_GotoChildElement );
}

BOOL MozillaLayoutProbe::GotoParentElement() {
    return position( NSCP_Probe_GotoParentElement );
}

/* MozillaLayoutProbe::GetElementType
 *
 */
int MozillaLayoutProbe::GetElementType() const {
    int result = 0;
    setOK( FALSE );
    if ( m_lProbeID ) {
        result = sendRequest( NSCP_Probe_GetElementType );
        setOK( !!result ); // 0 -> something went wrong.
    }
    return result;
}

/* MozillaLayoutProbe::getLongAttribute
 *
 * To obtain element attributes, we need shared memory into which mozilla can place
 * the result.  The process is the same for all 5 member functions, differing only
 * in the PROBE_IPC_REQUEST value passed.
 */
long MozillaLayoutProbe::getLongAttribute( PROBE_IPC_REQUEST req, void* data ) const {
    long result = 0;
    setOK( FALSE );
    if ( m_lProbeID ) {
        // Get room for result.
        setOK( allocSharedMem( sizeof( long ) ) );

        if ( m_bOK ) {
            sendRequest( req, sizeof( long ), data );
    
            // Extract result, if available.
            if ( m_bOK ) {
                result = *( (long*)m_pSharedMem );
            }
        }
    }
    return result;
}

long MozillaLayoutProbe::GetElementXPosition() const {
    return getLongAttribute( NSCP_Probe_GetElementXPosition );
}

long MozillaLayoutProbe::GetElementYPosition() const {
    return getLongAttribute( NSCP_Probe_GetElementYPosition );
}

long MozillaLayoutProbe::GetElementWidth() const {
    return getLongAttribute( NSCP_Probe_GetElementWidth );
}

long MozillaLayoutProbe::GetElementHeight() const {
    return getLongAttribute( NSCP_Probe_GetElementHeight );
}

long MozillaLayoutProbe::GetElementColor( PROBECOLORTYPE attr ) const {
    return getLongAttribute( NSCP_Probe_GetElementColor, (void*)attr );
}

long MozillaLayoutProbe::GetElementTextLength() const {
    return getLongAttribute( NSCP_Probe_GetElementTextLength );
}

/* MozillaLayoutProbe::GetElementText
 *
 * To obtain element string attributes, we need shared memory into which mozilla can place
 * the result.  We allocate a comparable chunk of shared memory, issue the request, and
 * then copy the result into the caller's buffer.  We return the length of the result
 * string.
 */
long MozillaLayoutProbe::GetElementText( char *pText, long maxLen ) const {
    long result = 0;
    setOK( FALSE );
    if ( m_lProbeID && maxLen ) {
        // Allocate shared memory (mapping file).
        setOK( allocSharedMem( maxLen + 1 ) );

        if ( m_bOK ) {
            sendRequest( NSCP_Probe_GetElementText, maxLen );
    
            // Extract result, if available.
            if ( m_bOK ) {
                sprintf( pText, "%.*s", (int)maxLen, (char*)m_pSharedMem );
                // Ensure our result is properly terminated.
                *( (char*)m_pSharedMem + maxLen ) = 0;
                result = strlen( (char*)m_pSharedMem );
            }
        }
    }
    return result;
}

/* MozillaLayoutProbe::getBOOLAttribute, MozillaLayoutProbe::ElementHas<Attribute>
 *
 * BOOL element attributes are obtained by issueing the request and mapping the
 * result code.
 */
BOOL MozillaLayoutProbe::getBOOLAttribute( PROBE_IPC_REQUEST req ) const {
    BOOL result = FALSE;
    setOK( FALSE );
    if ( m_lProbeID ) {
        // Don't use sendMessage as that converts the -1 to FALSE.
        PROBEAPISTRUCT pas = { m_lProbeID, 0, 0 };
        COPYDATASTRUCT cds = { req, sizeof pas, &pas  };
        result = SendMessage( mozHWND, WM_COPYDATA, NULL, (LPARAM)&cds );
        if ( result == -1 ) {
            // Request failed.
            setOK( FALSE );
            result = FALSE;
        } else {
            // Request succeeded.
            setOK( TRUE );
        }
    }
    return result;
}

BOOL MozillaLayoutProbe::ElementHasURL() const {
    return getBOOLAttribute( NSCP_Probe_ElementHasURL );
}

BOOL MozillaLayoutProbe::ElementHasText() const {
    return getBOOLAttribute( NSCP_Probe_ElementHasText );
}

BOOL MozillaLayoutProbe::ElementHasColor() const {
    return getBOOLAttribute( NSCP_Probe_ElementHasColor );
}

BOOL MozillaLayoutProbe::ElementHasChild() const {
    return getBOOLAttribute( NSCP_Probe_ElementHasChild );
}

BOOL MozillaLayoutProbe::ElementHasParent() const {
    return getBOOLAttribute( NSCP_Probe_ElementHasParent );
}

/* MozillaLayoutProbe::IsOK
 *
 * IsOK is simply the member m_bOK.
 */
BOOL MozillaLayoutProbe::IsOK() const {
    return m_bOK;
}

/* MozillaLayoutProbe::allocSharedMem
 *
 * Ensure m_pSharedMem points to sufficiently sized memory block
 * (backed by mapping file, as it happens).
 */
BOOL MozillaLayoutProbe::allocSharedMem( unsigned long ulSize ) const {
    MozillaLayoutProbe *self = (MozillaLayoutProbe*)this;

    BOOL result = FALSE;

    // Check whether current allocation is big enough for this request.
    if ( m_ulSharedMemSize >= ulSize ) {
        // Just use existing allocation.
        result = TRUE;
    } else {
        // See if existing allocation must be disposed of.
        if ( m_hSharedMem ) {
            // Need to free prior allocation.
            UnmapViewOfFile( m_pSharedMem );
            self->m_pSharedMem = 0;
            CloseHandle( m_hSharedMem );
            self->m_hSharedMem = 0;
            self->m_ulSharedMemSize = 0;
        }
    
        // Allocate minimum of 256 bytes.
        self->m_ulSharedMemSize = max( 256, ulSize );

        // Use canned name.
        char mappingName[ _MAX_PATH ];
        sprintf( mappingName, "MozillaLayoutProbe%ld", m_lProbeID );
    
        // Create memory-mapped file.
        self->m_hSharedMem = CreateFileMapping( (void*)0xFFFFFFFF, // Use dwMaximumSize[High/Low]
                                                NULL,              // No security attribute.
                                                PAGE_READWRITE,    // Read/write.
                                                0,                 // dwMaximumHigh.
                                                m_ulSharedMemSize, // dwMaximumLow.
                                                mappingName );     // Name.
        if ( m_hSharedMem ) {
            // Memory mapped file created, now "map" it.
            self->m_pSharedMem = MapViewOfFile( m_hSharedMem, FILE_MAP_WRITE, 0, 0, 0 );

            // Handle error.
            if ( m_pSharedMem ) {
                result = TRUE;
            } else {
                CloseHandle( m_hSharedMem );
                self->m_hSharedMem = 0;
                self->m_ulSharedMemSize = 0;
            }
        }
    }
    return result;
}

/* MozillaLayoutProbe::sendRequest
 *
 * Build PROBEAPISTRUCT and COPYDATASTRUCT and issue WM_COPYDATA message.
 * We convert a -1 return code to an error.
 */
long MozillaLayoutProbe::sendRequest( PROBE_IPC_REQUEST req,
                                      unsigned long ulSize,
                                      void* pUser ) const {
    long result = 0;
    PROBEAPISTRUCT pas = { m_lProbeID, ulSize, pUser };
    COPYDATASTRUCT cds = { req, sizeof pas, &pas };
    result = SendMessage( mozHWND, WM_COPYDATA, 0, (LPARAM)&cds );
    if ( result == -1 ) {
        result = 0;
    }
    setOK( result );
    return result;
}


/* MozillaLayoutProbe::setOK
 *
 * Have to cast away const so this can be set from const members.
 */
void MozillaLayoutProbe::setOK( BOOL b ) const {
    MozillaLayoutProbe *self = (MozillaLayoutProbe*)this;
    self->m_bOK = b;
    return;
}

/* LO_QA_*
 *
 * These functions are exported so they can be dynamically loaded from client
 * applications.  They all have the same name as the "real" layout probe APIs
 * declared in layprobe.h.  Further, they provide exactly the same interface
 * (with the one exception noted in mozprobe.h).
 *
 * Create/Destroy new/delete a MozillaLayoutProbe object.  This object's
 * address is converted to the "probeID" returned by Create (and provided
 * on all subsequent APIs).
 */
extern "C" long LO_QA_CreateProbe( struct MWContext_ *context ) {
    // Allocate a MozillaLayoutProbe.
    MozillaLayoutProbe *p = MozillaLayoutProbe::MakeProbe( (long)context );

    if ( p->IsOK() ) {
        // It worked.  Return it's address as the "id".
        return (long)p;
    } else {
        // It failed.  Delete the object and return 0.
        delete p;
        return 0;
    }
}

extern "C" void LO_QA_DestroyProbe( long probeID ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    // Delete the object.
    delete p;
}

extern "C" BOOL LO_QA_GotoFirstElement( long probeID ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    p->GotoFirstElement();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GotoNextElement( long probeID ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    p->GotoNextElement();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GotoChildElement( long probeID ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    p->GotoChildElement();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GotoParentElement( long probeID ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    p->GotoParentElement();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetElementType( long probeID, int *type ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *type = p->GetElementType();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetElementXPosition( long probeID, long *x ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *x = p->GetElementXPosition();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetElementYPosition( long probeID, long *y ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *y = p->GetElementYPosition();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetElementWidth( long probeID, long *width ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *width = p->GetElementWidth();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetElementHeight( long probeID, long *height ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *height = p->GetElementHeight();
    return p->IsOK();
}

extern "C" BOOL LO_QA_HasURL( long probeID, BOOL *hasURL ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *hasURL = p->ElementHasURL();
    return p->IsOK();
}

extern "C" BOOL LO_QA_HasText( long probeID, BOOL *hasText ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *hasText = p->ElementHasText();
    return p->IsOK();
}

extern "C" BOOL LO_QA_HasColor( long probeID, BOOL *hasColor ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *hasColor = p->ElementHasColor();
    return p->IsOK();
}

extern "C" BOOL LO_QA_HasChild( long probeID, BOOL *hasChild ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *hasChild = p->ElementHasChild();
    return p->IsOK();
}

extern "C" BOOL LO_QA_HasParent( long probeID, BOOL *hasParent ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *hasParent = p->ElementHasParent();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetText( long probeID, char *text, long len ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    p->GetElementText( text, len );
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetTextLength( long probeID, long *len ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *len = p->GetElementTextLength();
    return p->IsOK();
}

extern "C" BOOL LO_QA_GetColor( long probeID, long *color, PROBECOLORTYPE attr ) {
    MozillaLayoutProbe *p = (MozillaLayoutProbe*)probeID;
    *color = p->GetElementColor( attr );
    return p->IsOK();
}

