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
* 'Model' to do tree of data inspection
*/

// when     who     what
// 10/30/97 jband   added this file
//

package com.netscape.jsdebugging.ifcui;

import java.util.Observable;
import java.util.Observer;
import netscape.application.*;
import netscape.util.*;
import com.netscape.jsdebugging.ifcui.palomar.util.*;
import netscape.security.PrivilegeManager;
import netscape.security.ForbiddenTargetException;
import com.netscape.jsdebugging.api.*;

public class InspectorTyrant
    extends Observable
    implements Observer, Target, JSErrorReporter // , SimulatorPrinter // XXX Sim Hack
{
    public InspectorTyrant(Emperor emperor)
    {
        super();
        _emperor = emperor;
        _controlTyrant  = emperor.getControlTyrant();
        _stackTyrant    = emperor.getStackTyrant();

        if(AS.S)ER.T(null!=_controlTyrant,"emperor init order problem", this);
        if(AS.S)ER.T(null!=_stackTyrant,"emperor init order problem", this);

        _controlTyrant.addObserver(this);
        _stackTyrant.addObserver(this);
    }

    public InspectorNodeModel  getRootNode() {return _rootNode;}

    public InspectorNodeModel  setNewRootNode( String name )
    {
        _emperor.setWaitCursor(true);
        if( null != _rootNode )
        {
            _rootNode.clearLinks();
            _rootNode = null;
        }

        if( null != name )
            _rootNode = new InspectorNodeModel( name, 0, null, this );

        // for now the notification just means: "rootNode changed"
        _notifyObservers();
        _emperor.setWaitCursor(false);
        return _rootNode;
    }

    // It is important that our function name not conflict with anything
    // in the user's namespace. I think this should be safe :)
    private static final String _propEnumeratorFunctionName =
        "__John_Bandhauer_Says_This_Is_A_Unique_Identifier__";

    // Rhino currently does not like this form
//    private static final String _propEnumeratorFunction =
//        "function "+_propEnumeratorFunctionName+"(ob)\n"+
//        "{\n"+
//        "    var retval = \"\";\n"+
//        "    var prop;\n"+
//        "    for(prop in ob)\n"+
//        "       retval += prop+\",\";\n"+
//        "    return retval;\n"+
//        "}\n";

    private static final String _propEnumeratorFunction =
        _propEnumeratorFunctionName+" = new Function(\"ob\",\""+
        "    var retval = new String();"+
        "    var prop;"+
        "    for(prop in ob)"+
        "       retval += prop+\',\';"+
        "    return retval;\")";

    public String[] getPropNamesOfJavaScriptThing(String jsthing)
    {
        eval(_propEnumeratorFunction);
        String list = eval(_propEnumeratorFunctionName +"("+jsthing+")");
        eval( "delete " + _propEnumeratorFunctionName);
        //System.out.println(list);

        int len;
        String[] retval = null;
        if( null != list && 0 != (len = list.length()) )
        {
            // XXX This is a pure hack to add the 'layers' property to a list
            // that looks like a 'document', but lacks the 'layers' property.
            // This gets around the bug in Nav of not setting the ENUM flag
            // for document.layers
            //
            // a partial and arbitary list of props that ought to be there...
            if( -1 == list.indexOf("layers")     &&
                -1 != list.indexOf("alinkColor") &&
                -1 != list.indexOf("vlinkColor") &&
                -1 != list.indexOf("applets")    &&
                -1 != list.indexOf("bgColor")    &&
                -1 != list.indexOf("embeds") )
            {
                list = list + "layers,";
                len = list.length();
            }

            // we build both a string and a number vector so that we can
            // separately sort them before concating them. NOTE that numVec
            // actually holds strings representing numbers, rather than the
            // numbers themselves - we don't want rounding & conversions errors
            Vector stringVec = new Vector();
            Vector numVec = new Vector();
            int start = 0;
            for( int i = 0; i < len; i++ )
            {
                if( list.charAt(i) == ',' )
                {
                    String name = list.substring(start,i);
                    if( name.length() > 0 && ! name.equals(_propEnumeratorFunctionName) )
                    {
                        char c0 = name.charAt(0);
                        if( c0 == '-' || Character.isDigit(c0) )
                        {
                            // insertion sort of number
                            try
                            {
                                Double n = new Double(name);
                                double val = n.doubleValue();
                                int j;
                                int count = numVec.size();
                                for(j = 0; j < count; j++)
                                {
                                    if( val < new Double((String)numVec.elementAt(j)).doubleValue() )
                                    {
                                        numVec.insertElementAt(name,j);
                                        break;
                                    }
                                }
                                if( j == count )
                                    numVec.addElement(name);
                            }
                            catch(NumberFormatException e)
                            {
                                if(AS.S)ER.T(false,"failed to parse property name as number: "+name,this);
                                stringVec.addElement("["+name+"]");
                            }
                        }
                        else
                        {
                            // the below deals with cases where the property
                            // name is not a number, but is not a valid
                            // identifier either.
                            // e.g. window["foo/bar"]
                            // the navigator.mimeTypes array has such properties
                            String nameToAdd = null;
                            if(Character.isJavaIdentifierStart(c0))
                            {
                                int name_len = name.length();
                                int j;
                                for(j = 0; j < name_len; j++)
                                    if(!Character.isJavaIdentifierPart(name.charAt(j)))
                                        break;
                                if(j == name_len)
                                    nameToAdd = name;
                            }
                            if(nameToAdd == null)
                                nameToAdd = "[\""+name+"\"]";
                            stringVec.addElement(nameToAdd);
                        }
                    }
                    start = i+1;
                }
            }

            int stringVecLen = stringVec.size();
            int numVecLen    = numVec.size();

            if( (stringVecLen + numVecLen) > 0 )
            {
                retval = new String[stringVecLen + numVecLen];
                int i;
                for( i = 0; i < numVecLen; i++ )
                    retval[i] = "["+(String) numVec.elementAt(i)+"]";

                if( stringVecLen > 0 )
                    stringVec.sortStrings(true,true);

                for( i = 0; i < stringVecLen; i++ )
                    retval[numVecLen+i] = (String) stringVec.elementAt(i);
            }

        }
        return retval;
    }

    public String eval(String input)
    {
        if( null == input )
            return null;

        String eval = input;

        // currently don't allow eval when not stopped.
        if( ControlTyrant.STOPPED != _controlTyrant.getState() )
            return "[error unable to evaluate while running]";

        JSStackFrameInfo frame = (JSStackFrameInfo) _stackTyrant.getCurrentFrame();
        if( null == frame )
            return null;

        JSSourceLocation loc = _stackTyrant.getCurrentLocation();
        if( null == loc )
            return null;

        String filename;
        if( _emperor.isPre40b6() ||
            Emperor.REMOTE_SERVER == _emperor.getHostMode() )
            filename = "inspector";
        else
            filename = loc.getURL();

        PrivilegeManager.enablePrivilege("Debugger");

        String result = "";
        _errorString = null;

        JSErrorReporter oldER = null;

        if( ! _emperor.isCorbaHostConnection() )
            oldER = _controlTyrant.setErrorReporter(this);

        DebugController dc = _emperor.getDebugController();
        if( null != dc && null != frame )
        {
            ExecResult fullresult =
                dc.executeScriptInStackFrame(frame,eval,filename,1);
            result = fullresult.getResult();

            if( fullresult.getErrorOccured() )
                {
                    _errorString = fullresult.getErrorMessage();
                }
        }

        if( ! _emperor.isCorbaHostConnection() )
            _controlTyrant.setErrorReporter(oldER);

        if( null != _errorString )
            result = "[error "+_errorString+"]";
        else if( null == result )
            result = "[null]";
        // else
        //    result = result;

        return result;
    }

    /*
    * NOTE: this ErrorReporter may be called on a thread other than
    * the IFC UI thread
    */

    // implement JSErrorReporter interface
    public int reportError( String msg,
                            String filename,
                            int    lineno,
                            String linebuf,
                            int    tokenOffset )
    {
        _errorString = msg;
        return JSErrorReporter.RETURN;
    }

    // implement observer interface
    public void update(Observable o, Object arg)
    {
        if ( o == _controlTyrant )
        {
            if( ControlTyrant.RUNNING == _controlTyrant.getState() )
                setNewRootNode( null == _rootNode ? null : _rootNode.getName() );
        }
        else if( o == _stackTyrant )
        {
            // force re-eval in new frame
            setNewRootNode( null == _rootNode ? null : _rootNode.getName() );
        }
    }

    // implement target interface
    public void performCommand(String cmd, Object data)
    {
    }

    // for now the notification just means: "rootNode changed"
    private void _notifyObservers()
    {
        setChanged();
        notifyObservers(null);
    }

    private Emperor             _emperor;
    private ControlTyrant       _controlTyrant;
    private StackTyrant         _stackTyrant;
    private String              _errorString;
    private InspectorNodeModel  _rootNode;
}
