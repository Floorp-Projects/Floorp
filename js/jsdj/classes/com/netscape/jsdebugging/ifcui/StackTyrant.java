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

/*
* 'Model' of stack information of stopped thread
*/

// when     who     what
// 06/27/97 jband   added this header to my code
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import com.netscape.jsdebugging.api.*;

public class StackTyrant
    extends Observable
    implements Observer
{
    public StackTyrant(Emperor emperor)
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);

        _controlTyrant.addObserver(this);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _controlTyrant )
        {
            if( ControlTyrant.STOPPED == _controlTyrant.getState() )
            {
                // fill list

                // invert the order from the DebugController
                StackFrameInfo[] stack = null;

                try
                {
                    stack = _controlTyrant.getThreadState().getStack();
                }
                catch(InvalidInfoException e)
                {
                    if(AS.S)ER.T(false,"InvalidInfoException in StackTyrant",this);
                }

                if(AS.S)ER.T(null!=stack,"stack is null!",this);
                if(AS.S)ER.T(0!=stack.length,"stack is empty!",this);

                if( null != stack )
                {
                    _frameArray = new StackFrameInfo[stack.length];

                    for(int i = 0; i < stack.length; i++)
                        _frameArray[i] = stack[stack.length-(i+1)];
                }

                // set selected and notify

                _currentFrameIndex = 0;
                _notifyArrayChanged();
            }
            else
            {
                // clear and notify
                _frameArray = null;
                setCurrentFrame(0);
            }
        }
    }

    public StackFrameInfo[] getFrameArray() {return _frameArray;}
    public int              getCurrentFrameIndex() {return _currentFrameIndex;}
    public StackFrameInfo   getCurrentFrame()
    {
        if( null == _frameArray )
            return null;
//        if(AS.S)ER.T(null!=_frameArray,"getCurrentFrame called when no frameArray!",this);

        return _frameArray[_currentFrameIndex];
    }


    public JSSourceLocation getCurrentLocation()
    {
        if( null == _frameArray )
            return null;
        if( ControlTyrant.STOPPED != _controlTyrant.getState() )
            return null;

  //      if(AS.S)ER.T(null!=_frameArray,"getCurrentLocation called when no frameArray!",this);

        StackFrameInfo rawframe = _frameArray[_currentFrameIndex];
        if( null == rawframe || ! (rawframe instanceof JSStackFrameInfo) )
            return null;

        JSPC pc;
        try
        {
            pc = (JSPC) rawframe.getPC();
        }
        catch(InvalidInfoException e)
        {
            if(AS.S)ER.T(false,"InvalidInfoException in StackTyrant",this);
            return null;
        }
        return (JSSourceLocation)pc.getSourceLocation();
    }

    public void setCurrentFrame( int i )
    {
        if(AS.S)ER.T(null==_frameArray||i>=0,"setCurrentFrame index out of bounds!",this);
        if(AS.S)ER.T(null==_frameArray||i<_frameArray.length,"setCurrentFrame index out of bounds!",this);
        _currentFrameIndex = i;
        _notifyCurrentFrameChanged();
    }

    private void _notifyCurrentFrameChanged()
    {
        setChanged();
        notifyObservers(new StackTyrantUpdate(StackTyrantUpdate.SELECT_CHANGED,_currentFrameIndex));
    }

    private void _notifyArrayChanged()
    {
        setChanged();
        notifyObservers(new StackTyrantUpdate(StackTyrantUpdate.ARRAY_CHANGED,_currentFrameIndex));
    }


    // data...

    private Emperor          _emperor;
    private ControlTyrant    _controlTyrant;

    private int              _currentFrameIndex;
    private StackFrameInfo[] _frameArray;
}

class StackTyrantUpdate
{
    public static final int SELECT_CHANGED = 0;
    public static final int ARRAY_CHANGED  = 1;

    public StackTyrantUpdate( int type, int sel )
    {
        this.type = type;
        this.sel  = sel;
    }
    public int  type;
    public int  sel;
}


