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

/* probe1.cpp :
 *
 * This is a C++ test/sample for how to use the Mozilla layout probe
 * "hooks" from a C++ program.
 */
#include <iostream.h>
#include <windows.h>

#include "mozprobe.h"

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

static const char * const indent( int depth ) {
    static char buffer[ 256 ];
    memset( buffer, '\t', depth + 2 );
    buffer[ depth + 2 ] = 0;
    return buffer;
}

void main() {

    cout << "Mozilla layout probe C++ test starting..." << endl;

    // Loop over "contexts" till done.
    for ( long context = 1; ; context++ ) {
        // Create a probe for this context.
        cout << "\tCreating probe for context " << context << "..." << endl;
        MozillaLayoutProbe *probe = MozillaLayoutProbe::MakeProbe( context );
    
        // Check if it worked.
        if ( probe->IsOK() ) {
            cout << "\t...created OK; now enumerating elements..." << endl;

            // Now enumerate all elements.
            cout << "\t\tGoing to first element..." << endl;
            probe->GotoFirstElement();

            int depth = 0;

            while ( probe->IsOK() ) {
                cout << indent(depth) << "...OK, here's the element info:" << endl;
                cout << indent(depth) << "\ttype          = " << probe->GetElementType()
                                                              << " (" << typeAsText( probe->GetElementType() ) << ")" << endl;
                cout << indent(depth) << "\tx             = " << probe->GetElementXPosition() << endl;
                cout << indent(depth) << "\ty             = " << probe->GetElementYPosition() << endl;
                cout << indent(depth) << "\twidth         = " << probe->GetElementWidth() << endl;
                cout << indent(depth) << "\theight        = " << probe->GetElementHeight() << endl;
                cout << indent(depth) << "\tfg color      = " << hex << probe->GetElementColor(Probe_Foreground) << endl;
                cout << indent(depth) << "\tbg color      = " << hex << probe->GetElementColor(Probe_Background) << endl;
                cout << indent(depth) << "\tborder color  = " << hex << probe->GetElementColor(Probe_Border) << endl;
                cout << indent(depth) << "\ttext len      = " << dec << probe->GetElementTextLength() << endl;
                cout << indent(depth) << "\tHasText       = " << probe->ElementHasText() << endl;
                cout << indent(depth) << "\tHasURL        = " << probe->ElementHasURL() << endl;
                cout << indent(depth) << "\tHasColor      = " << probe->ElementHasColor() << endl;
                cout << indent(depth) << "\tHasChild      = " << probe->ElementHasChild() << endl;
                cout << indent(depth) << "\tHasParent     = " << probe->ElementHasParent() << endl;
                // Allocate space for text.
                long len = probe->GetElementTextLength() + 1;
                char *pText = new char[ len ];
                // Ensure it's properly terminated.
                if ( pText ) {
                    *pText = 0;
                    cout << indent(depth) << "\ttext          = " << ( probe->GetElementText( pText, len ), pText ) << endl;
                    // Free the text buffer.
                    delete pText;
                }
                // Recurse into sub-elements if there are any...
                if ( probe->GotoChildElement() ) {
                    cout << indent(depth) << "Recursing into child elements..." << endl;
                    depth++;
                } else {
                    // Go on to next element.
                    cout << indent(depth) << "Going to the next element..." << endl;
                    probe->GotoNextElement();
                    while ( !probe->IsOK() ) {
                        if ( depth-- ) {
                            // Go up to parent and advance to next element.
                            cout << indent(depth) << "...end of child elements." << endl;
                            probe->GotoParentElement();
                            probe->GotoNextElement();
                        } else {
                            break;
                        }
                    }
                }
            }
            cout << "\t...end of element list (or error); GetLastError=0x" << GetLastError() << endl;
        } else {
            cout << "\t...create failed; GetLastError=0x" << hex << GetLastError() << dec << endl;
            // Terminate the test.
            break;
        }
        // Delete probe.
        delete probe;
    }

    cout << "...test completed." << endl;
}
