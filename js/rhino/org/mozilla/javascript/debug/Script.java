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

package org.mozilla.javascript.debug;

import org.mozilla.javascript.*;
import org.mozilla.javascript.debug.util.*;

final class Script
    implements IScript, ICListElement
{
    Script(NativeFunction fun)
    {
        _fun = fun;
        if(fun instanceof INativeFunScript)
        {
            _fund = (INativeFunScript) fun;
            _fund.set_debug_script(this);
        }
        CList.initCList(this);
    }

    // implements ICListElement
    public final ICListElement getNext()       {return _next;}
    public final ICListElement getPrev()       {return _prev;}
    public final void setNext(ICListElement e) {_next = e;}
    public final void setPrev(ICListElement e) {_prev = e;}
    private ICListElement _next;
    private ICListElement _prev;

    public synchronized String getURL()
    {
        if( null == _fun )
            return null;
        return NativeDelegate.getSourceName(_fun);
    }

    public synchronized String getFunction()
    {
        if( null == _fun )
            return null;
        if( ! NativeDelegate.isFunction(_fun) )
            return null;
        return NativeDelegate.getFunctionName(_fun);
    }

    public synchronized int getBaseLineNumber()
    {
        if( null == _fund )
            return 0;
        return _fund.get_debug_baseLineno();
    }

    public synchronized int getLineExtent()
    {
        if( null == _fund )
            return 0;
        return 1 + _fund.get_debug_endLineno() - _fund.get_debug_baseLineno();
    }

    public synchronized boolean isValid()
    {
        return null != _fun;
    }

    public synchronized IPC getClosestPC(int line)
    {
        if( null == _fund )
            return null;

        int pc = _fund.lineno2pc(line);
        if( 0 == pc )
            return null;
        return new PC(this, pc);
    }

    public synchronized org.mozilla.javascript.NativeFunction getWrappedFunction()
    {
        return _fun;
    }

    synchronized void invalidate()
    {
        if(null != _fund)
        {
            _fund.set_debug_script(null);
            _fund = null;
        }
        _fun = null;
    }

    synchronized int  incRefCount() {return ++refCount;}
    synchronized int  decRefCount() {return --refCount;}
    synchronized int  getRefCount() {return refCount;}

    public String toString()
    {
        if( null == _fun )
            return "Invalidated Script";
        else             
        {
            String range = null;
            String flags = "";
            if(null != _fund)
            {
                range = "["+_fund.get_debug_baseLineno()+","+_fund.get_debug_endLineno()+"]";
                flags = " with stop flags "+ _fund.get_debug_stopFlags();
            }
            else
                range = "[?,?]";
            String fun = getFunction();
            return "Script for "+getURL()+"::"+((null==fun?"<top_level>":fun)+
                   " "+range+flags);
        }
    }

    public int hashCode()
    {
        return _fun.hashCode();
    }

    public boolean equals(Object obj)
    {
        if(!(obj instanceof Script))
            return false;
        Script o = (Script)obj;
        if( (!isValid()) || (!o.isValid()))
            return false;
        return _fun == o._fun;
    }

    private NativeFunction _fun;
    private INativeFunScript _fund;
    private int refCount;
}
