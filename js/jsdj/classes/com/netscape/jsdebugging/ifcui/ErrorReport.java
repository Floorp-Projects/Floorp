/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

/*
* Holds error state information
*/

// when     who     what
// 06/30/97 jband   added this simple dumb data class
//

package com.netscape.jsdebugging.ifcui;

class ErrorReport
{
    private ErrorReport() {};    // 'Hidden' default ctor

    public  ErrorReport(
                String msg,
                String filename,
                int    lineno,
                String linebuf,
                int    tokenOffset )
    {
        this.msg = msg;
        this.filename = filename;
        this.lineno = lineno;
        this.linebuf = linebuf;
        this.tokenOffset = tokenOffset;
    }

    // data...
    public String msg;
    public String filename;
    public int    lineno;
    public String linebuf;
    public int    tokenOffset;
}
