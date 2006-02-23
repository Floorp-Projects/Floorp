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
    * Construct for url (which is immutable during the lifetime of the object)
    * <p>
    * Presumably, text will be added later
    * @param url url or filename by which this object will be known
    */
    public SourceTextItem( String url )
    {
        this( url, (String)null, INITED );
    }

    /**
    * Construct for url with text and status
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
