/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

package org.mozilla.javascript.debug.util;

/*
* NOTE: none of this is synchronized -- that is the caller's problem
*/

public class CList
    implements ICListElement
{
    public CList()
    {
        CList.initCList(this);
    }

    // implements ICListElement
    public final ICListElement getNext()       {return _next;}
    public final ICListElement getPrev()       {return _prev;}
    public final void setNext(ICListElement e) {_next = e;}
    public final void setPrev(ICListElement e) {_prev = e;}
    private ICListElement _next;
    private ICListElement _prev;

    // statics

    public static final void initCList(ICListElement e)
    {
        e.setNext(e);
        e.setPrev(e);
    }

    public static final void removeLink(ICListElement e)
    {
        e.getPrev().setNext(e.getNext());
        e.getNext().setPrev(e.getPrev());
    }

    public static final void removeAndInitLink(ICListElement e)
    {
        removeLink(e);
        initCList(e);
    }

    public static final boolean isEmpty(ICListElement e)
    {
        return e.getNext() == e;
    }

    // non-statics

    public final void insertBefore(ICListElement e)
    {
        e.setNext(this);
        e.setPrev(this.getPrev());
        this.getPrev().setNext(e);
        this.setPrev(e);
    }

    public final void insertAfter(ICListElement e)
    {
        e.setNext(this.getNext());
        e.setPrev(this);
        this.getNext().setPrev(e);
        this.setNext(e);
    }

    public final void appendLink(ICListElement e)
    {
        insertBefore(e);
    }

    public final void insertLink(ICListElement e)
    {
        insertAfter(e);
    }

    public final ICListElement head()
    {
        return getNext();
    }

    public final ICListElement tail()
    {
        return getPrev();
    }
}    
