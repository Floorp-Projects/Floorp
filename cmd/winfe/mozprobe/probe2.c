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

/* probe2.c
 *
 * This is a C test/sample for how to use the Mozilla layout probe
 * "hooks" from a C++ program.
 */
#include <stdio.h>
#include <windows.h>

#include "mozprobe.h"

/* Clever macro to get DLL entry points. */
#define ENTRY_POINT(fn,type) \
    entryPoints.fn = (type)GetProcAddress( probeDLL, #fn ); \
    if ( !entryPoints.fn ) { \
        printf( "\t...error loading entry point %s, GetLastError=0x%lX\n", #fn, GetLastError() ); \
        goto abortTest;                                                                           \
    }

static const char * const typeAsText( int type ) {
    const char * p;
    switch ( type ) {
        case Probe_Text:   p = "text"; break;
        case Probe_HRule:  p = "horizontal rule"; break;
        case Probe_Image:  p = "image"; break;
        case Probe_Bullet: p = "bullet"; break;
        case Probe_Form:   p = "form element"; break;
        case Probe_Table:  p = "table"; break;
        case Probe_Cell:   p = "cell"; break;
        case Probe_Embed:  p = "embed"; break;
        case Probe_Java:   p = "java"; break;
        case Probe_Object: p = "object"; break;
        default:           p = "unknown"; break;
    }
    return p;
}

void main() {
    long context;
    BOOL bOK;
    long probe;
    HINSTANCE probeDLL;
    PROBEAPITABLE entryPoints = { 0 };

    printf( "MozillaLayoutProbe C test starting...\n" );

    printf( "\tLoading %s.dll...\n", mozProbeDLLName );
    probeDLL = LoadLibrary( mozProbeDLLName );

    if ( probeDLL ) {
        printf( "\t...loaded OK.\n" );

        printf( "\tGetting entry points in DLL...\n" );
        ENTRY_POINT( LO_QA_CreateProbe, LONG(*)(struct MWContext_*) );
        ENTRY_POINT( LO_QA_DestroyProbe, void(*)(long) );
        ENTRY_POINT( LO_QA_GotoFirstElement, BOOL(*)(long) );
        ENTRY_POINT( LO_QA_GotoNextElement, BOOL(*)(long) );
        ENTRY_POINT( LO_QA_GotoChildElement, BOOL(*)(long) );
        ENTRY_POINT( LO_QA_GotoParentElement, BOOL(*)(long) );
        ENTRY_POINT( LO_QA_GetElementType, BOOL(*)(long,int*) );
        ENTRY_POINT( LO_QA_GetElementXPosition, BOOL(*)(long,long*) );
        ENTRY_POINT( LO_QA_GetElementYPosition, BOOL(*)(long,long*) );
        ENTRY_POINT( LO_QA_GetElementWidth, BOOL(*)(long,long*) );
        ENTRY_POINT( LO_QA_GetElementHeight, BOOL(*)(long,long*) );
        ENTRY_POINT( LO_QA_HasText, BOOL(*)(long,BOOL*) );
        ENTRY_POINT( LO_QA_HasURL, BOOL(*)(long,BOOL*) );
        ENTRY_POINT( LO_QA_HasColor, BOOL(*)(long,BOOL*) );
        ENTRY_POINT( LO_QA_HasChild, BOOL(*)(long,BOOL*) );
        ENTRY_POINT( LO_QA_HasParent, BOOL(*)(long,BOOL*) );
        ENTRY_POINT( LO_QA_GetTextLength, BOOL(*)(long,long*) );
        ENTRY_POINT( LO_QA_GetText, BOOL(*)(long,char*,long) );
        ENTRY_POINT( LO_QA_GetColor, BOOL(*)(long,long*,PROBECOLORTYPE) );
        printf( "\t...all entry points loaded OK.\n" );

        /* Loop over "contexts" till done. */
        for ( context = 1; ; context++ ) {
            /* Create a probe for this context. */
            printf( "\tCreating probe for context %ld...\n", context );

            probe = entryPoints.LO_QA_CreateProbe( (struct MWContext_*)context );
        
            /* Check if it worked. */
            if ( probe ) {
                printf( "\t...created OK; now enumerating elements...\n" );
    
                /* Now enumerate all elements. */
                printf( "\t\tGoing to first element...\n" );

                bOK = entryPoints.LO_QA_GotoFirstElement( probe );
    
                while ( bOK ) {
                    int type;
                    long lAttr;
                    BOOL bAttr;
                    char *pText;
                    long textLen;

                    printf( "\t\t...OK, here's the element info:\n" );

                    printf( "\t\t\ttype         = %d (%s)\n", ( entryPoints.LO_QA_GetElementType( probe, &type ), type ), typeAsText(type) );
                    printf( "\t\t\tx            = %ld\n", ( entryPoints.LO_QA_GetElementXPosition( probe, &lAttr ), lAttr ) );
                    printf( "\t\t\ty            = %ld\n", ( entryPoints.LO_QA_GetElementYPosition( probe, &lAttr ), lAttr ) );
                    printf( "\t\t\twidth        = %ld\n", ( entryPoints.LO_QA_GetElementWidth( probe, &lAttr ), lAttr ) );
                    printf( "\t\t\theight       = %ld\n", ( entryPoints.LO_QA_GetElementHeight( probe, &lAttr ), lAttr ) );
                    printf( "\t\t\tfg color     = %lX\n", ( entryPoints.LO_QA_GetColor( probe, &lAttr, Probe_Foreground ), lAttr ) );
                    printf( "\t\t\tbg color     = %lX\n", ( entryPoints.LO_QA_GetColor( probe, &lAttr, Probe_Background ), lAttr ) );
                    printf( "\t\t\tborder color = %lX\n", ( entryPoints.LO_QA_GetColor( probe, &lAttr, Probe_Border ), lAttr ) );
                    printf( "\t\t\ttext len     = %ld\n", ( entryPoints.LO_QA_GetTextLength( probe, &lAttr ), lAttr ) );
                    printf( "\t\t\tHasText      = %d\n", ( entryPoints.LO_QA_HasText( probe, &bAttr ), (int)bAttr ) );
                    printf( "\t\t\tHasURL       = %d\n", ( entryPoints.LO_QA_HasURL( probe, &bAttr ), (int)bAttr ) );
                    printf( "\t\t\tHasColor     = %d\n", ( entryPoints.LO_QA_HasColor( probe, &bAttr ), (int)bAttr ) );
                    printf( "\t\t\tHasChild     = %d\n", ( entryPoints.LO_QA_HasChild( probe, &bAttr ), (int)bAttr ) );
                    printf( "\t\t\tHasParent    = %d\n", ( entryPoints.LO_QA_HasParent( probe, &bAttr ), (int)bAttr ) );

                    /* Allocate space for text. */
                    entryPoints.LO_QA_GetTextLength( probe, &textLen );
                    textLen++;
                    pText = malloc( textLen );
                    if ( pText ) {
                        /* Ensure it's properly terminated. */
                        *pText = 0;
                        /* Get text. */
                        entryPoints.LO_QA_GetText( probe, pText, textLen );
                        printf( "\t\t\ttext         = %s\n", pText );
                        /* Free the text buffer. */
                        free( pText );
                    }
                    /* Go on to next element. */
                    printf( "\t\tGoing to the next element...\n" );
                    bOK = entryPoints.LO_QA_GotoNextElement( probe );
                }
                printf( "\t...end of element list (or error); GetLastError=0x%lX\n", GetLastError() );
            } else {
                printf( "\t...create failed; GetLastError=0x%lX\n", GetLastError() );
                /* Terminate the test. */
                break;
            }
            // Destroy the probe.
            entryPoints.LO_QA_DestroyProbe( probe );
        }
    } else {
        printf( "\t...load failed; GetLastError=0x%lX\n", GetLastError() );
    }
    abortTest:
    printf( "...test completed.\n" );
}
