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

public class IniFileData
{
    Hashtable   		  	sections = null;
   	File					me = null;
    boolean					dirty = false;
    boolean					writable = true;
    
    public static final String		SECTION_PREFIX = "[";
	public static final String		SECTION_POSTFIX = "]";
    public static final String		COMMENT_PREFIX = "#";
	
	protected final void init( File inputFile ) throws Exception
	{
    	if ( inputFile == null )
    		throw new NullPointerException( "IniFileData constructor requires non-null file argument" );
    		
        sections = new Hashtable();
		me = inputFile;
	
		if ( me.exists() )
			readFileContents();
	}
			
	public IniFileData( File inputFile ) throws Exception
	{
		init( inputFile );
	}

	public IniFileData( File inputFile, boolean isWritable ) throws Exception
	{
		init( inputFile );
		writable = isWritable;
	}
	
	private void readFileContents() throws Exception
	{
		//Trace.TRACE( "reading file: " + inputFile.getPath() );
		
		BufferedReader  bufferedReader = new BufferedReader( new FileReader( me ) );
		
		String line = bufferedReader.readLine();
		
		while ( line != null )
		{
			//Trace.TRACE( "line: " + line );
			
			while ( !line.startsWith( SECTION_PREFIX ) )
			    line = bufferedReader.readLine();
			
			int closingBracketAt = line.indexOf( SECTION_POSTFIX );
			if ( closingBracketAt != -1 )
			{
			    String          sectionName = new String( line.substring( 1, closingBracketAt ).trim() );
			    NameValueSet    nvSet = new NameValueSet();
			
			    //Trace.TRACE( "found section: " + sectionName );
			
			    nvSet.read( bufferedReader );
			    sections.put( sectionName, nvSet );
			}
			else
			    throw new MalformedIniFileException( "malformed file: " + me.getPath() );
			
			line = bufferedReader.readLine();
		}
	}
	
	private void writeFileContents() throws Exception
	{
		BufferedWriter 	bufferedWriter = new BufferedWriter( new FileWriter( me ) );
	
	    for ( Enumeration sectionList = sections.keys(); sectionList.hasMoreElements(); )
        {
            String          sectionName = (String)sectionList.nextElement();
            NameValueSet    nvSet = (NameValueSet)sections.get( sectionName );

			bufferedWriter.write( SECTION_PREFIX + sectionName + SECTION_POSTFIX );
			bufferedWriter.newLine();
			nvSet.write( bufferedWriter );
			bufferedWriter.newLine();
		}
	
		bufferedWriter.close();	
		dirty = false;
	}
	
	protected void finalize() throws Throwable
	{
		try
		{
			flush();
		}
		catch ( Throwable e )
		{
		}
		super.finalize();
	}
	
	public void flush() throws Exception
	{
		Trace.TRACE( "flushing " + me.getName() );
		if ( writable && isDirty() )
		{
			Trace.TRACE( "dirty, writing file contents" );
			writeFileContents();
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

	public void setValue( String sectionName, String name, String value )
	{
		if ( sectionName == null || name == null || value == null )
			throw new NullPointerException( "arguments to setValue must be non-null" );
			
		NameValueSet		nvSet = (NameValueSet)sections.get( sectionName );
	
		if ( nvSet == null )
		{
			//Trace.TRACE( "setValue, creating nvSet" );
			nvSet = new NameValueSet();
			sections.put( sectionName, nvSet );
			dirty = true;
		}
			
		String		currentValue;
		currentValue = nvSet.getValue( name );
		//Trace.TRACE( "currentValue: " + currentValue );
		//Trace.TRACE( "value: " + value );
		if ( value.compareTo( currentValue ) != 0 )
		{
			//Trace.TRACE( "		setting value..." );
			nvSet.setValue( name, value );
			dirty = true;
		}
	}
	
	public boolean isDirty()
	{
		return dirty;
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


