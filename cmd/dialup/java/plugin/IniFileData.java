/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

package netscape.npasw;

import netscape.npasw.*;
import java.io.*;
import java.lang.*;
import java.util.*;
import netscape.security.*;
//import Trace;

public class IniFileData
{
    Hashtable       sections;
    final String    sectionPrefix = "[";


    public IniFileData( File inputFile ) throws Exception
    {
        sections = new Hashtable();

        //Trace.TRACE( "reading file: " + inputFile.getPath() );

        BufferedReader  bufferedInputReader = new BufferedReader( new FileReader( inputFile ) );

        String line = bufferedInputReader.readLine();

        while ( line != null )
        {
            //Trace.TRACE( "line: " + line );

            while ( !line.startsWith( sectionPrefix ) )
                line = bufferedInputReader.readLine();

            int closingBracketAt = line.indexOf( "]" );
            if ( closingBracketAt != -1 )
            {
                String          sectionName = new String( line.substring( 1, closingBracketAt ).trim() );
                NameValueSet    nvSet = new NameValueSet();

                //Trace.TRACE( "found section: " + sectionName );

                nvSet.read( bufferedInputReader );
                sections.put( sectionName, nvSet );
            }
            else
                throw new MalformedIniFileException( "malformed file: " + inputFile.getPath() );

            line = bufferedInputReader.readLine();
        }
    }

    public String getValue( String sectionName, String name )
    {
        //Trace.TRACE( "getting section: " + sectionName );

        NameValueSet        nvSet = (NameValueSet)sections.get( sectionName );

        if ( nvSet == null )
            return null;

        String      value = nvSet.getValue( name );
        //Trace.TRACE( "value: " + value );
        return value;
    }

    public final void printIniFileData()
    {
        for ( Enumeration sectionList = sections.keys(); sectionList.hasMoreElements(); )
        {
            String          sectionName = (String)sectionList.nextElement();
            NameValueSet    iniSection = (NameValueSet)sections.get( sectionName );

            Trace.PrintToConsole( "section name: [ " + sectionName + " ] " );
            iniSection.printNameValueSet();
        }
    }
}


