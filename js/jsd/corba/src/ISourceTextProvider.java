/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

public interface ISourceTextProvider extends org.omg.CORBA.Object
{
    /* these coorespond to jsdebug.h values - change in both places if anywhere */
    public static final int INITED  = 0;
    public static final int FULL    = 1;
    public static final int PARTIAL = 2;
    public static final int ABORTED = 3;
    public static final int FAILED  = 4;
    public static final int CLEARED = 5;

    public String[] getAllPages();
    public void     refreshAllPages();
    public boolean  hasPage(String url);
    public boolean  loadPage(String url);
    public void     refreshPage(String url);
    public String   getPageText(String url);
    public int      getPageStatus(String url);
    public int      getPageAlterCount(String url);
}    
