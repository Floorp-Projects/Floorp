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
    Hashtable					nameValuePairs;
	boolean						ignoreSections = false;
	final static int			READ_AHEAD = 8192;		


	protected void init()
	{
        nameValuePairs = new Hashtable();
	}
	
	/* delimiterB between name-value pairs, delimiterA between the name and value */
	protected void initWithDelimitedString( String nvPairs, int delimiterA, int delimiterB )
	{
		init();
		
		String				temp = nvPairs;
		String              name = null;
		String              value = null;
		String				nvPair = null;
		
		int					delB;
		int					delA;
		
		boolean				done = false;
		
		while ( done == false )
		{
			delB = temp.indexOf( delimiterB );
			if ( delB == -1 )
			{
				done = true;
				nvPair = temp;
			}
			else
				nvPair = temp.substring( 0, delB );
				
			delA = nvPair.indexOf( delimiterA );
			if ( delA != - 1 )
			{
				name = new String( nvPair.substring( 0, delA ) );
				value = new String( nvPair.substring( delA + 1, nvPair.length() ) );
			}
			else
			{
				name = new String( nvPair );
				value = new String();
			}
			//Trace.TRACE( "name: " + name + " value: " + value );
			nameValuePairs.put( name, value );
			
			if ( done == false )
				temp = temp.substring( delB + 1, temp.length() );
		}
	}
			
	protected void initWithFile( File inputFile, boolean ignoreSects ) throws Exception
	{
		init();
		
    	if ( inputFile == null )
    		throw new NullPointerException( "constructor for NameValueSet requires non-null parameter" );
    	
    	ignoreSections = ignoreSects;

    	if ( inputFile.exists() == false )
    		return;

		//Trace.TRACE("parsing NameValueSet: " + inputFile.getAbsolutePath() );
        BufferedReader  bufferedReader = new BufferedReader( new FileReader( inputFile ) );

		this.read( bufferedReader );
	}
			
	public NameValueSet()
	{
        // * XXX
        init();
	}

    /*
        @param inputFile        file to be parsed into nameValuePairs
    */
	public NameValueSet( File inputFile, boolean ignoreSects ) throws Exception
	{
    	initWithFile( inputFile, ignoreSects );
	}
	
    public NameValueSet( File inputFile ) throws Exception
    {
    	initWithFile( inputFile, false );
    }

    /*
        @param nvPairs          string in the form "name1=value2&name2=value2&..." to
                                be parsed into nameValuePairs
    */
    public NameValueSet( String nvPairs, int delimiter ) throws Exception
    {
    	initWithDelimitedString( nvPairs, '=', delimiter );
	}

	public NameValueSet( String nvPairs ) throws Exception
	{
		initWithDelimitedString( nvPairs, '=', ' ' );
	}

	private void readLine( String line )
	{
        if ( !line.startsWith( IniFileData.COMMENT_PREFIX ) )
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
	}
	
	public void setIgnoreSections( boolean ignoreSects )
	{
		ignoreSections = ignoreSects;
	}
			
    public void read( BufferedReader reader ) throws Exception
    {
        reader.mark( READ_AHEAD );
        String          line = reader.readLine();

        while ( line != null )
        {
            //Trace.TRACE( "line: " + line );
            if ( ignoreSections || !line.startsWith( IniFileData.SECTION_PREFIX ) )
            {
                reader.mark( READ_AHEAD );
				this.readLine( line );
                line = reader.readLine();
            }
            else
            {
                reader.reset();
                break;
            }
        }
    }
	
	public void read( File inputFile ) throws Exception
	{
    	if ( inputFile.exists() == false )
    		return;

		//Trace.TRACE("parsing NameValueSet: " + inputFile.getAbsolutePath() );
        BufferedReader  bufferedReader = new BufferedReader( new FileReader( inputFile ) );

		this.read( bufferedReader );
	}
			
	public void write( BufferedWriter writer ) throws Exception
	{
        for ( Enumeration names = nameValuePairs.keys(); names.hasMoreElements(); )
        {
            String      name = (String)names.nextElement();
            String      value = (String)nameValuePairs.get( name );
            writer.write( name + "=" + value );
            writer.newLine();
        }
	}
	
    public boolean unsetName( String inputName  )
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
