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

import netscape.util.Vector;
import netscape.security.ForbiddenTargetException;

/**
* Abstract class to represent entity capable of providing access to source text
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public abstract class SourceTextProvider
{
    public static SourceTextProvider getSourceTextProvider()  throws Exception 
    {
        return null;
    }
    /**
    * Return the vector of SourceTextItems. 
    * <p>
    * This is not liekly to be a copy. nor is it necessarily current
    */
    public abstract Vector           getSourceTextVector() throws ForbiddenTargetException;
    /**
    * Refresh the vector by whatever means to reflect any changes made in the 
    * underlying native system
    */
    public abstract void             refreshSourceTextVector();
    /**
    * Get the SourceText item for the given URL
    */
    public abstract SourceTextItem   findSourceTextItem( String url ) throws ForbiddenTargetException;
    /**
    * Load the SourceText item for the given URL
    * <p>
    * <B>This is not guaranteed to be implemented</B>
    */
    public abstract SourceTextItem   loadSourceTextItem( String url ) throws ForbiddenTargetException;
}    
