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
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;

/**
* This class provides access to SourceText items.
* <p>
* This class is meant to be a singleton and has a private constructor.
* Call the static <code>getSourceTextProvider()</code> to get this object.
* <p>
* Note that all functions use netscape.security.PrivilegeManager to verify 
* that the caller has the "Debugger" privilege. The exception 
* netscape.security.ForbiddenTargetException will be throw if this is 
* not enabled.
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
* @see netscape.security.PrivilegeManager
* @see netscape.security.ForbiddenTargetException
*/
public final class JSSourceTextProvider extends SourceTextProvider
{
    private JSSourceTextProvider( long nativeContext )
    {
        _sourceTextVector = new Vector();
        _nativeContext    = nativeContext;
    }
    

    /**
     * Get the SourceTextProvider object for the current VM.
     * <p>
     * @return the singleton SourceTextProvider
     */
    public static synchronized SourceTextProvider getSourceTextProvider()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        if( null == _sourceTextProvider )
        {
            long nativeContext = DebugController.getDebugController().getNativeContext();
            if( 0 != nativeContext )
                _sourceTextProvider = new JSSourceTextProvider(nativeContext);
        }
        return _sourceTextProvider;
    }

    /**
    * Get the SourceText item for the given URL
    */
    public SourceTextItem findSourceTextItem( String url )
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return findSourceTextItem0(url);
    }

    // also called from native code...
    private SourceTextItem findSourceTextItem0( String url )
    {
        synchronized( _sourceTextVector )
        {
            for (int i = 0; i < _sourceTextVector.size(); i++)
            {
                SourceTextItem src = (SourceTextItem) _sourceTextVector.elementAt(i);
            
                if( src.getURL().equals(url) )
                    return src;
            }
            return null;
        }
    }

    /**
    * Return the vector of SourceTextItems. 
    * <p>
    * This is NOT a copy. nor is it necessarily current
    */
    public Vector getSourceTextVector()
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return _sourceTextVector;
    }

    /**
    * Load the SourceText item for the given URL
    * <p>
    * <B>This is not guaranteed to be implemented</B>
    */
    public synchronized SourceTextItem   loadSourceTextItem( String url )
        throws ForbiddenTargetException
    {
        PrivilegeManager.checkPrivilegeEnabled("Debugger");
        return loadSourceTextItem0( url );
    }

    private native SourceTextItem   loadSourceTextItem0( String url );

    /**
    * Refresh the vector to reflect any changes made in the 
    * underlying native system
    */
    public synchronized native void refreshSourceTextVector();

    private static SourceTextProvider   _sourceTextProvider = null;
    private Vector                      _sourceTextVector;
    private long                        _nativeContext;
}    
