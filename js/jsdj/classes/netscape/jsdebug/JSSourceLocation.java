/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

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
