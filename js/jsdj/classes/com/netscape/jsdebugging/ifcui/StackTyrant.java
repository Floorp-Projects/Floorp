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


