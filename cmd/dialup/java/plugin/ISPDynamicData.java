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
import java.util.Vector;

//import Trace;

public class ISPDynamicData
{
	static final String			NAME_STRING = "ISPNAME";
	static final String			LANGUAGE_STRING = "LANGUAGE";
	static final String			SERVICE_TYPE_STRING = "SERVICE_TYPE";
	static final String			DYNAMIC_DATA_STRING = "DYNAMICDATA";
	//  static final String			zipFilesURL = "http://seaspace.netscape.com:8080/programs/ias5/regserv/docs/ISP/";
	
	public NameValueSet			reggieData = null;
	protected String			name = null;
	protected String			language = null;
	protected String			serviceType = null;
	
	static String				lastName = null;
	
	protected Vector			dynamicData = null;
	
    public ISPDynamicData()
    {
    }

    public void read( ReggieStream is ) throws Exception
    {
        // this will need to be reworked a little once we get the reggie protocol fixed
        String      name;
        String      value;
        boolean     done = false;
        boolean     haveName = false;

        reggieData = new NameValueSet();

        try
        {
			//Trace.TRACE( "creating ISPDynamicData" );
			while ( !done )
			{
				if ( lastName != null )
				{
					//Trace.TRACE( "have lastName" );
					name = lastName;
					lastName = null;
				}
				else
				{
					name = is.nextToken();
				}
			
				//Trace.TRACE( "name is: " + name );
			
				if ( name.compareTo( NAME_STRING ) == 0 )
				{
					if ( !haveName )
					{
						//Trace.TRACE( "setting haveName" );
						haveName = true;
					}
					else
					{
						//Trace.TRACE( "haveName was set, setting lastName, we're done" );
						lastName = name;
						done = true;
					}
				}
			
				if ( done == false && name.compareTo( "" ) != 0 )
				{
					value = is.nextToken();
					//Trace.TRACE( "value is: " + value );
					reggieData.setValue( name, value );
				}
			
				if ( name.compareTo( "" ) == 0 )
					throw new EOFException( "done with reggie stream" );
			}
        }
        catch ( MalformedReggieStreamException e )
        {
            lastName = null;
            throw new EOFException( "done with reggie stream" );
        }
        catch ( Exception e )
        {
            lastName = null;
            throw e;
        }
    }

    public String getName()
    {
        if ( name != null )
            return name;
        else
        {
            if ( reggieData != null )
            {
                String      temp = reggieData.getValue( NAME_STRING );
                if ( temp != null && temp.compareTo( "" ) != 0 )
                {
                    name = temp;
                    return name;
                }
            }
        }
        return "";
    }

    public String getLanguage()
    {
        if ( language != null )
        {
            //Trace.TRACE( "returning language" + language );
            return language;
        }
        else
        {
            if ( reggieData != null )
            {
                //Trace.TRACE( "looking for language" );
                String      temp = reggieData.getValue( LANGUAGE_STRING );
                if ( temp != null && temp.compareTo( "" ) != 0  )
                {
                    //Trace.TRACE( "language value found:" + temp );
                    language = temp;
                    return language;
                }
            }
        }
        return "";
    }
	
	public NameValueSet getDynamicData()
	{
		return this.getDynamicData( 0 );
	}
	
	public int getDynamicDataSize()
	{
		if ( dynamicData == null )
			this.parseDynamicData();
		if ( dynamicData == null )
			return 0;
			
		return dynamicData.size();
	}
	
	public NameValueSet getDynamicData( int index )
	{
		
		if ( dynamicData == null )
			this.parseDynamicData();
		
		if ( dynamicData == null )
			return null;

		try
		{
			NameValueSet		nvSet = (NameValueSet)dynamicData.elementAt( index );	
			return nvSet;
		}
		catch ( Throwable e )
		{
			return null;
		}
	}
	
	
    public void parseDynamicData()
    {
		if ( reggieData != null )
		{
			//Trace.TRACE( "parseDynamicData" );
		    String      temp = reggieData.getValue( DYNAMIC_DATA_STRING );
		    if ( temp != null && temp.compareTo( "" ) != 0 )
		    {
				try
				{
					boolean		done = false;
		        
		        	if ( dynamicData == null )
		        		dynamicData = new Vector();
		        			
		        	while ( done == false )
		        	{
						int			delimiter = 1;
						String		subTemp = null;
						
						//Trace.TRACE( "temp: " + temp );
						
						int			delIndex = temp.indexOf( delimiter );
						
						if ( delIndex != -1 )
		        		{
							subTemp = temp.substring( 0, delIndex );
							temp = temp.substring( delIndex + 1 );
						}
						else
						{
							subTemp = temp;
							//Trace.TRACE( "	done..." );
							done = true;
						}
						//Trace.TRACE( "subTemp: " + subTemp );
						NameValueSet nvSet = new NameValueSet( subTemp, 2 );
						dynamicData.addElement( nvSet );
					}
				}
				catch ( Throwable e )
				{
					dynamicData = null;
					e.printStackTrace();
				}
		    }
		}
    }

    public final void printISPDynamicData()
    {
        if ( reggieData != null )
            reggieData.printNameValueSet();
    }

}

