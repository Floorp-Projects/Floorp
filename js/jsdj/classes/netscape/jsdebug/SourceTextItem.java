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
* This class is used to represent a file or url which contains 
* JavaScript source text. The actual JavaScript source may be intermixed
* with other text (as in an html file) or raw. The debugger uses this 
* interface to access the source. The file of the actual source need 
* not physially exist anywhere; i.e. the underlying engine might synthesize
* it as needed.
* <p>
* The <i>url</i> of this class is immutable -- it represents a key in 
* collections of these objects
*
* @author  John Bandhauer
* @version 1.0
* @since   1.0
*/
public class SourceTextItem
{
    /* these coorespond to jsdebug.h values - change in both places if anywhere */

    /**
    * This object is initialized, but contains no text
    */
    public static final int INITED  = 0;
    /**
    * This object contains the full text
    */
    public static final int FULL    = 1;
    /**
    * This object contains the partial text (valid , but more may come later)
    */
    public static final int PARTIAL = 2;
    /**
    * This object may contain partial text, but loading was aborted (by user?)
    */
    public static final int ABORTED = 3;
    /**
    * This object may contain partial text, but loading failed (or the 
    * underlying debugger support was unable to capture it; e.g. 
    * not enough memory...)
    */
    public static final int FAILED  = 4;
    /**
    * This object contains no text because the debugger has signaled that 
    * the text is no longer needed
    */
    public static final int CLEARED = 5;

    /**
    * Constuct for url (which is immutable during the lifetime of the object)
    * <p>
    * Presumably, text will be added later
    * @param url url or filename by which this object will be known
    */
    public SourceTextItem( String url )
    {
        this( url, (String)null, INITED );
    }

    /**
    * Constuct for url with text and status
    * <p>
    * @param url url or filename by which this object will be known
    * @param text source text this object should start with
    * @param status status this object should start with
    */
    public SourceTextItem( String url, String text, int status )
    {
        _url    = url;
        _text   = text;
        _status = status;
        _dirty  = true;
    }
    
    public String   getURL()    {return _url;   }
    public String   getText()   {return _text;  }
    public int      getStatus() {return _status;}
    public boolean  getDirty()  {return _dirty; }
    
    public void     setText(String text)    {_text = text;}
    public void     setStatus(int status)   {_status = status;}
    public void     setDirty(boolean b)     {_dirty = b; }

    private String  _url;
    private String  _text;
    private int     _status;
    private boolean _dirty;
}    
