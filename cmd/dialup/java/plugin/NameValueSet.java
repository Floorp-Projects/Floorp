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
//import Trace;
import java.io.*;
import java.lang.*;
import java.util.*;

class FileCreationException extends Exception
{
    public FileCreationException( String s )
    {
        super( s );
    }
}

class MalformedNameValueStringException extends Exception
{
    public MalformedNameValueStringException( String s )
    {
        super( s );
    }
}

public class NameValueSet
{
    Hashtable           nameValuePairs;
    final String        commentPrefix = "#";


    public NameValueSet()
    {
        // ¥ XXX
        nameValuePairs = new Hashtable();
    }

    /*
        @param nvPairs          string in the form "name1=value2&name2=value2&..." to
                                be parsed into nameValuePairs
    */
    public NameValueSet( String nvPairs ) throws Exception
    {
        nameValuePairs = new Hashtable();

        StringReader        stringReader = new StringReader( nvPairs );
        int                 ch = 0;
        StringBuffer        buffer = new StringBuffer();
        String              name = null;
        String              value = null;

        ch = stringReader.read();
        while ( ch != -1 )
        {
            buffer.setLength( 0 );
            while ( ch == ' ' )
                ch = stringReader.read();

            while ( ch != -1 && ch != '=' &&  ch != ' ' )
            {
                buffer.append( (char)ch );
                ch = stringReader.read();
            }

            if ( ch == ' ' )
            {
                while ( ch == ' ' )
                    ch = stringReader.read();
            }

            if ( ch == '=' )
            {
                name = new String( buffer );
                buffer.setLength( 0 );
                ch = stringReader.read();

                while ( ch == ' ' )
                    ch = stringReader.read();

                while ( ch != -1 &&  ch != ' ' )
                {
                    buffer.append( (char)ch );
                    ch = stringReader.read();
                }

                value = new String( buffer );

                nameValuePairs.put( name, value );
                if ( ch == ' ' )
                {
                    while ( ch == ' ' )
                        ch = stringReader.read();
                }
            }
        }
    }

    /*
        @param inputFile        file to be parsed into nameValuePairs
    */
    public NameValueSet( File inputFile ) throws Exception
    {
        nameValuePairs = new Hashtable();

        BufferedReader  bufferedInputReader = new BufferedReader( new FileReader( inputFile ) );

        String          line = bufferedInputReader.readLine();
        while ( line != null )
        {
            if ( !line.startsWith( commentPrefix ) )
            {
                int equalsSignAt = line.indexOf( "=" );
                if ( equalsSignAt != -1 )
                {
                    String          nameString = line.substring( 0, equalsSignAt ).trim();
                    String          valueString = line.substring( ++equalsSignAt ).trim();

                    if ( nameString.length() != 0 && valueString.length() != 0 )
                        nameValuePairs.put( nameString, valueString );
                }
            }
            line = bufferedInputReader.readLine();
        }
    }

    public void read( BufferedReader reader ) throws Exception
    {
        final String    sectionPrefix = "[";

        reader.mark( 8192 );
        String          line = reader.readLine();

        while ( line != null )
        {
            //Trace.TRACE( "line: " + line );
            if ( !line.startsWith( sectionPrefix ) )
            {
                if ( !line.startsWith( commentPrefix ) )
                {
                    int equalsSignAt = line.indexOf( "=" );
                    if ( equalsSignAt != -1 )
                    {
                        String          nameString = new String( line.substring( 0, equalsSignAt ).trim() );
                        String          valueString = new String( line.substring( ++equalsSignAt ).trim() );

                        //Trace.TRACE( "name: " + nameString + " value: " + valueString );

                        if ( nameString.length() != 0 && valueString.length() != 0 )
                            nameValuePairs.put( nameString, valueString );
                    }
                }
                reader.mark( 8192 );
                line = reader.readLine();
            }
            else
            {
                reader.reset();
                break;
            }
        }
    }

    public void addNameValuePair( String inputName, String inputValue )
    {
        nameValuePairs.put( inputName, inputValue );
    }

    public boolean removeNameValuePair( String inputName  )
    {
        if ( nameValuePairs.remove( inputName ) != null )
            return true;
        return false;
    }

    public boolean containsNameValuePair( NameValuePair inputNVPair )
    {
        String      value = (String)nameValuePairs.get( inputNVPair.name );
        if ( value != null && ( value.compareTo( inputNVPair.value ) == 0 ) )
            return true;
        return false;
    }

    public boolean isSubsetOf( NameValueSet inputNVSet )
    {
        for ( Enumeration names = nameValuePairs.keys(); names.hasMoreElements(); )
        {
            String      name = (String)names.nextElement();
            String      value = (String)nameValuePairs.get( name );

            if ( !inputNVSet.containsNameValuePair( new NameValuePair( name, value ) ) )
                return false;
        }
        return true;
    }

    public String getValue( String name )
    {
        String      result;
        result = (String)nameValuePairs.get( name );
        if ( result == null )
            return new String( "" );
        else
            return result;
    }

    public void setValue( String name, String value )
    {
        nameValuePairs.put( name, value );
    }

    public final void printNameValueSet()
    {
        for ( Enumeration names = nameValuePairs.keys(); names.hasMoreElements(); )
        {
            String      name = (String)names.nextElement();
            String      value = (String)nameValuePairs.get( name );
            Trace.PrintToConsole( "name: " + name + " value: " + value );
        }
    }
}
