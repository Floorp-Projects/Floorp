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
* 'View' to display and edit list of data watches
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
import com.netscape.jsdebugging.ifcui.palomar.widget.layout.*;

public class WatchView
    extends InternalWindow
    implements Observer, Target
{
    private static final int _buttonWidth  = 70;
    private static final int _buttonHeight = 24;
    private static final int _buttonYSpace = 5;
    private static final int _buttonXSpace = 5;

    private int _nextButtonX;
    private int _nextButtonY;

    public WatchView( Emperor emperor, Rect rect )
    {
        super(rect);

        _emperor = emperor;
        _watchTyrant  = emperor.getWatchTyrant();
        _commandTyrant = emperor.getCommandTyrant();

        if(AS.S)ER.T(null!=_watchTyrant     ,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_commandTyrant,"emperor init order problem", this);

        _listview = new BackgroundHackListView();

        Size size = contentSize();
        int listwidth    = size.width - (_buttonWidth + (_buttonXSpace * 2));
        Rect rectSG = new Rect(0, 0, listwidth, size.height);

        _nextButtonX = listwidth + _buttonXSpace;
        _nextButtonY = _buttonYSpace;

        _addButton( "New",       NEW_CMD );
        _addButton( "Edit",      EDIT_CMD);
        _addButton( "Move Up",   UP_CMD  );
        _addButton( "Move Down", DOWN_CMD);
        _addButton( "Delete",    DEL_CMD );
        _addButton( "Eval",      EVAL_CMD);
        _nextButtonY += _buttonYSpace * 2;
        _addButton( "Done",      DONE_CMD);

        ScrollGroup sg = new ScrollGroup(rectSG);
        sg.setHorizScrollBarDisplay( ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setVertScrollBarDisplay(  ScrollGroup.AS_NEEDED_DISPLAY );
        sg.setContentView( _listview );
        sg.setAutoResizeSubviews(true);
        sg.contentView().setLayoutManager( new MarginLayout() );
        sg.setBackgroundColor(_emperor.getBackgroundColor());

        setCloseable( false );
        setResizable( false );
        setTitle( "Watches" );
        addSubview(sg);
        setAutoResizeSubviews(true);

        _listview.setAllowsEmptySelection(false);
        _listview.setAllowsMultipleSelection(false);

        _listview.setTarget(this);
        _listview.setCommand("");
        _listview.setDoubleCommand(EDIT_CMD);

        _listview.setBackgroundColor(_emperor.getBackgroundColor());

        setLayer(InternalWindow.PALETTE_LAYER);

        refresh();

        layoutView(1,1);
        layoutView(-1,-1);
    }

    private Button _addButton( String title, String cmd )
    {
        Button button = new Button(_nextButtonX,_nextButtonY,
                                   _buttonWidth,_buttonHeight);
        button.setTitle(title);
        button.setTarget(this);
        button.setCommand(cmd);
        addSubview(button);
        _nextButtonY += _buttonHeight + _buttonYSpace;
        return button;
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
        if( cmd.equals(NEW_CMD ) )
        {
            StringEditorDialog sed = new StringEditorDialog("New Watch", "",
                                                            _emperor.getFixedFont());
            sed.showModally();
            if( sed.okPressed() )
            {
                int index = _listview.selectedIndex();
                if( -1 == index )
                    index = 0;

                Vector vec = _watchTyrant.getWatchList();
                synchronized( vec )
                {
                    vec.insertElementAt(sed.getString(),index);
                }
                refresh();
                _listview.selectItemAt(index);
                _watchTyrant.evalList();
            }
        }
        else if( cmd.equals(EDIT_CMD) )
        {
            int index = _listview.selectedIndex();
            if( -1 == index )
                return;
            Vector vec = _watchTyrant.getWatchList();
            synchronized( vec )
            {
                String str = (String) vec.elementAt(index);
                StringEditorDialog sed = new StringEditorDialog("Edit Watch",
                                                                str,
                                                                _emperor.getFixedFont());
                sed.showModally();

                if( sed.okPressed() )
                {
                    vec.setElementAt(sed.getString(),index);
                    refresh();
                    _listview.selectItemAt(index);
                    _watchTyrant.evalList();
                }
            }
        }
        else if( cmd.equals(UP_CMD  ) )
        {
            int index = _listview.selectedIndex();
            if( index < 1 )
                return;
            Vector vec = _watchTyrant.getWatchList();
            synchronized( vec )
            {
                String s = (String) vec.removeElementAt(index);
                vec.insertElementAt(s,index-1);
            }
            refresh();
            _watchTyrant.evalList();
        }
        else if( cmd.equals(DOWN_CMD) )
        {
            int index = _listview.selectedIndex();
            if( index < 0 || index == _listview.count()-1)
                return;
            Vector vec = _watchTyrant.getWatchList();
            synchronized( vec )
            {
                String s = (String) vec.removeElementAt(index);
                vec.insertElementAt(s,index+1);
            }
            refresh();
            _watchTyrant.evalList();
        }
        else if( cmd.equals(DEL_CMD ) )
        {
            int index = _listview.selectedIndex();
            if( index < 0 )
                return;
            Vector vec = _watchTyrant.getWatchList();
            synchronized( vec )
            {
                vec.removeElementAt(index);
            }
            refresh();
            _watchTyrant.evalList();
        }
        else if( cmd.equals(EVAL_CMD) )
        {
            _watchTyrant.evalList();
        }
        else if( cmd.equals(DONE_CMD) )
        {
            Application.application().performCommandLater(
                    _commandTyrant,
                    _commandTyrant.cmdString(CommandTyrant.WATCHES_SHOW_HIDE),
                    this );
        }
    }

    public synchronized void refresh()
    {
        String selText = null;
        ListItem selItem = _listview.selectedItem();
        if( null != selItem )
            selText = selItem.title();

        _listview.removeAllItems();

        Font linefont = _emperor.getFixedFont();
        int maxlinelen = 0;

        Vector vec = _watchTyrant.getWatchList();
        synchronized( vec )
        {
            for (int i = 0; i < vec.size(); i++)
            {
                String text = (String) vec.elementAt(i);
                ListItem item = new ListItem();

                maxlinelen = Math.max( maxlinelen, text.length() );
                item.setTitle( text );
                item.setFont( linefont );
                item.setSelectedColor(_emperor.getSelectionColor());
                _listview.addItem( item );

                if( 0 == i )
                    _listview.selectItemAt(i);  // make sure SOMETHING selected

                if( null != selText && selText.equals(text) )
                {
                    _listview.selectItemAt(i);
                    selText = null; // we only want the first match...
                }
            }
        }
        FontMetrics fm = linefont.fontMetrics();
        _listview.setBounds( 0, 0, (maxlinelen+1) * fm.charWidth('X'),0 );
        _listview.sizeToMinSize();

        layoutView(0,0);
        _listview.draw();
    }

    private Emperor         _emperor;
    private WatchTyrant     _watchTyrant;
    private CommandTyrant   _commandTyrant;
    private ListView        _listview;

    private static final String NEW_CMD   = "NEW_CMD";
    private static final String EDIT_CMD  = "EDIT_CMD";
    private static final String UP_CMD    = "UP_CMD";
    private static final String DOWN_CMD  = "DOWN_CMD";
    private static final String DEL_CMD   = "DEL_CMD";
    private static final String EVAL_CMD  = "EVAL_CMD";
    private static final String DONE_CMD  = "DONE_CMD";
}


