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

package com.netscape.jsdebugging.apitests.analyzing.data;

import com.netscape.jsdebugging.apitests.analyzing.tree.*;
import com.netscape.jsdebugging.apitests.xml.Tags;

/**
 * Given the head of a description subtree reconstructs fields which Error contains.
 * DataPoolManager is passed to look up serial numbers of objects inside the description.
 *
 * @author Alex Rakhlin
 */

public class DataError extends DataSerializable {

    public DataError (TreeNode head, DataPoolManager dpm){
        super (head, dpm);
        _msg = TreeUtils.getChildStringData (head, Tags.message_tag);
        _url = TreeUtils.getChildStringData (head, Tags.url_tag);
        _lineno = TreeUtils.getChildIntegerData (head, Tags.lineno_tag).intValue();
        _linebuf = TreeUtils.getChildStringData (head, Tags.linebuf_tag);
        _token_offset = TreeUtils.getChildIntegerData (head, Tags.token_offset_tag).intValue();
    }

    public String toString (){
        return " ErrorReport: MSG: "+_msg+" URL: "+_url+" LINENO: "+_lineno+
                " LINEBUF: "+_linebuf+" TOKEN_OFFSET: "+_token_offset;
    }

    public String toFormattedString (){
        return "ErrorReport \n"+
               "MSG: "+_msg+"\n" +
               "URL: "+_url+"\n" +
               "LINENO: "+_lineno + "\n"+
               "LINEBUF: "+_linebuf + "\n"+
               "TOKEN_OFFSET: "+_token_offset;
    }

    public boolean equalsTo (Data d){
        DataError err = (DataError) d;
        return (err.getMessage().equals(_msg) && err.getURL().equals(_url) && err.getLineno() == _lineno &&
                err.getLinebuf().equals(_linebuf) && err.getTokenOffset() == _token_offset);
    }

    public String getMessage () { return _msg; }
    public String getURL () { return _url; }
    public int getLineno () { return _lineno; }
    public String getLinebuf () { return _linebuf; }
    public int getTokenOffset () { return _token_offset; }
    
    private String _msg;
    private String _url;
    private int _lineno;
    private String _linebuf;
    private int _token_offset;
}
