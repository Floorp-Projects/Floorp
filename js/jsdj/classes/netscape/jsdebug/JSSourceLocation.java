/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.jsdebug;

/**
* JAvaScript specific SourceLocation
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public class JSSourceLocation extends SourceLocation
{
    public JSSourceLocation( JSPC pc, int line )
    {
        _pc   = pc;
        _line = line;
        _url  = pc.getScript().getURL();
    }

    /**
     * Gets the first line number associated with this SourceLocation.
     * This is the lowest common denominator information that will be
     * available: some implementations may choose to include more
     * specific location information in a subclass of SourceLocation.
     */
    public int getLine() {return _line;}

    public String getURL() {return _url;}

    /**
     * Get the first PC associated with a given SourceLocation.
     * This is the place to set  a breakpoint at that location.
     * returns null if there is no code corresponding to that source
     * location.
     */
    public PC getPC() {return _pc;}

    public String toString()
    {
        return "<SourceLocation "+_url+"#"+_line+">";
    }

    private JSPC    _pc;
    private int     _line;
    private String  _url;
}
