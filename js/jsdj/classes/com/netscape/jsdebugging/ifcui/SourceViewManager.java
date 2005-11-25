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
* 'Model' to manage all the source views
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

public class SourceViewManager
    implements Observer, Target, WindowOwner, PrefsSupport
{
    public SourceViewManager(Emperor emperor)
    {
        super();

        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _sourceTyrant   = emperor.getSourceTyrant();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_sourceTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);

        _sourceViews    = new Hashtable();
        _preferredNewViewRect = new Rect(0,0,100,100);

        _controlTyrant.addObserver(this);
        _stackTyrant.addObserver(this);

        _updateMarksTimer = new Timer(this, null, 1000 ); // 1 second wait
        _updateMarksTimer.setCommand(UPDATE_MARKS);
        _updateMarksTimer.setRepeats(false);
        _updateMarksTimer.setCoalesce(true);
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        // XXX perhaps this object need do nothing?
        if( o == _sourceTyrant )
        {
            SourceTyrantUpdate update = (SourceTyrantUpdate)arg;
            switch( update.type )
            {
                case SourceTyrantUpdate.SELECT_CHANGED :
                    // ignore for now
                    break;
                case SourceTyrantUpdate.VECTOR_CHANGED :
                    // XXX update all views
                    break;
                case SourceTyrantUpdate.ITEM_CHANGED   :
                    // XXX update particular view
                    break;
                default:
                    // ignore any other types
                    break;
            }
        }
        else if ( o == _stackTyrant )
        {
            if( ControlTyrant.STOPPED == _controlTyrant.getState() )
            {
                JSSourceLocation loc = _stackTyrant.getCurrentLocation();
                if(null == loc )
                    return;

                SourceTextItem sti =
                    _sourceTyrant.findSourceItem( loc.getURL() );
                if(AS.S)ER.T(null!=sti,"could not find SourceTextItem for " + loc.getURL(),this);
                if(null == sti)
                    return;

                SourceView view = findView(sti);
                if( null != view )
                    activateView(view);
                else
                    view = createView(sti);

                view.updateMarks();
                view.ensureSourceLineVisible( loc.getLine() );
            }
        }
        else if ( o == _controlTyrant )
        {
            if( ControlTyrant.RUNNING == _controlTyrant.getState() )
            {
                // set timer to refresh marks on visible windows after a bit
                _updateMarksTimer.start();
            }
        }
    }

    // implement Target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(UPDATE_MARKS) )
        {
            if( _controlTyrant.getEnabled() &&
                ControlTyrant.RUNNING == _controlTyrant.getState() )
            {
                Object[] views = _sourceViews.elementsArray();
                for( int i = 0; i < views.length; i++ )
                {
                    SourceView view = (SourceView) views[i];
                    if( view.isVisible() )
                        view.updateMarks();
                }
            }
        }
    }

    public  SourceView findView(String url)
    {
        return (SourceView)_sourceViews.get(url);
    }

    public  SourceView findView(SourceTextItem item)
    {
        return findView(item.getURL());
    }

    private synchronized SourceView _createViewAtRect( Rect rect, SourceTextItem item )
        {
            if(AS.S)ER.T(null!=rect,"null rect in _createViewAtRect", this);

            if( null != findView(item.getURL()) )
            {
                if(AS.S)ER.T( false, "tried to create second SourceView for:" + item.getURL(), this);
                return null;
            }

            SourceView v = new SourceView( rect, _emperor, item );
            _sourceViews.put(item.getURL(),v);
            return v;
        }

    public synchronized SourceView createView( SourceTextItem item )
    {
        // XXX add smarter way of determining location for new source view
        Rect rect = _preferredNewViewRect;
        SourceView view = _createViewAtRect( rect, item );
        if( null != view )
            view.setOwner(this);
        _setSelDataForMainSourceView(view);
        return view;
    }

    public synchronized void activateView( SourceView view )
    {
        view.show();
    }

    public synchronized void viewBeingRemoved( SourceView view )
    {
        _sourceViews.remove(view.getURL());
    }

    public void setPreferredNewViewRect( Rect rect ){_preferredNewViewRect = rect;}

    public boolean getShowLineNumbers() {return _showLineNumbers;}

    public void    setShowLineNumbers(boolean b)
    {
        if( _showLineNumbers == b )
            return;
         _showLineNumbers = b;

        // force redraw of all visible views
        Object[] views = _sourceViews.elementsArray();
        if( null == views )
            return;
        for( int i = 0; i < views.length; i++ )
        {
            SourceView view = (SourceView) views[i];
            if( view.isVisible() )
                view.forceRefresh();
        }
    }

    public void selectionInSourceViewChanged( SourceView sv, String s, int lineno)
    {
        if( _mainSourceView != sv )
            return;

        _setSelDataForMainSourceView(sv);

    }

    // implement WindowOwner interface
    public boolean windowWillShow(Window aWindow)   {return true;}
    public void windowDidShow(Window aWindow)       {}
    public boolean windowWillHide(Window aWindow)   {return true;}
    public void windowDidHide(Window aWindow)       {}
    public void windowWillSizeBy(Window aWindow, Size deltaSize) {}
    public void windowDidBecomeMain(Window aWindow)
    {
        _setSelDataForMainSourceView((SourceView) aWindow);
    }
    public void windowDidResignMain(Window aWindow)
    {
        _setSelDataForMainSourceView(null);
    }

    private void _setSelDataForMainSourceView(SourceView sv)
    {
        _mainSourceView = sv;

        if( null == sv )
        {
            _selectedLineInMainSourceView = 0;
            _selectedTextInMainSourceView = null;
            _sourceTyrant.SetSelectedUrlLineText(null,0,null);
        }
        else
        {
            _selectedLineInMainSourceView = sv.getSelectedLineNumber();
            _selectedTextInMainSourceView = sv.getSelectedText();
            _sourceTyrant.SetSelectedUrlLineText( sv.getURL(),
                                                  _selectedLineInMainSourceView,
                                                  _selectedTextInMainSourceView );
        }
    }

    // implement PrefsSupport interface
    public void prefsWrite(Archiver archiver)    throws CodingException
    {
        SourceViewSaver svs = new SourceViewSaver();
        svs.getFromManager(this);
        archiver.archiveRootObject(svs);
    }
    public void prefsRead(Unarchiver unarchiver) throws CodingException
    {
        int id = Util.RootIdentifierForClassName(unarchiver.archive(),
                                      "com.netscape.jsdebugging.ifcui.SourceViewSaver" );
        if( -1 != id )
        {
            SourceViewSaver svs = (SourceViewSaver)
                                    unarchiver.unarchiveIdentifier(id);
            svs.sendToManager(this);
        }
    }

    // data...

    private Emperor         _emperor;
    private ControlTyrant   _controlTyrant;
    private SourceTyrant    _sourceTyrant;
    private StackTyrant     _stackTyrant;
    private Hashtable       _sourceViews;
    private Rect            _preferredNewViewRect;
    private Timer           _updateMarksTimer;
    private boolean         _showLineNumbers = false;

    private SourceView      _mainSourceView = null;
    private int             _selectedLineInMainSourceView = 0;
    private String          _selectedTextInMainSourceView = null;

    private static final String UPDATE_MARKS = "UPDATE_MARKS";
}



