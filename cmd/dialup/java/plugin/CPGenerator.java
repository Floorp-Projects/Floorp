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
/*
    compare page generator
*/
package netscape.npasw;

import netscape.npasw.*;
import java.io.*;
import java.net.*;
import java.lang.*;
import java.util.*;
import netscape.security.*;
//import Trace;

public class CPGenerator
{
	public static final int			SENDING = 4;
	public static final int			RECEIVING_RESPONSE = 5;
	public static final int			WAITING = 6;
	public static final int			CONTACTING_SERVER = 7;
	public static final int			DONE = 0;
	public static final int			ABORT = -1;
	
	public static final String		FEATURE_STRING = "FEATURES";
	public static final int			FEATURE_COUNT = 8;
	public static final boolean		DEBUG = true;
	
	public static boolean			done = false;
	public static String			currentFile = "";
	public static int				totalBytes = 0;
	
	static final String				REG_SOURCE_STRING = "REG_SOURCE";
		
	static int						state = DONE;
	
	static String					regSource;
	
	static String					ispDirectorySymbol = "isp_dir";
	static String					comparePageTemplateFileName = "compare.tmpl";
	static String					comparePageFileName = "compare.htm";
	static String					localPath;
	
	// ispList is a vector of ISPDynamicData's that is created from the stream passed back
	//	from the registration server "POST"
	static Vector					ispList = null;
	static Vector					nameValueSets = null;

    //static CPGeneratorProgress    progress = null;

//  static final String         regservMimeData = "http://seaspace.netscape.com:8080/programs/ias5/regserv/docs/reg.cgi";

    public static int getState()
    {
        return state;
    }
	
	private static String getLocalPath()
	{
		return localPath;
	}
	
	private static String getMetadataPath()
	{
		return ( getLocalPath() + "metadata" + File.separator + "html" + File.separator );
	}
	
	private static String getISPPath()
	{
		return ( getLocalPath() + "isp" + File.separator );
	}
	
	private static String getJarFilePath( ISPDynamicData isp )
	{
		return new String( getISPPath() + isp.getLanguage() + File.separator +
							isp.getName() + File.separator + isp.getName() + ".jar" );
    }

	private static String getConfigPath( ISPDynamicData isp )
	{
		return new String( getISPPath() + isp.getLanguage() + File.separator +
							isp.getName() + File.separator + "client_data" +
								File.separator + "config" + File.separator );
    }

	private static String getConfigFilePath( ISPDynamicData isp )
	{
		return new String( getConfigPath( isp ) + "config.ias" );
	}
	
	private static String getCompareFilePath( ISPDynamicData isp )
	{
		return new String( getConfigPath( isp ) + "compare.cfg" );
	}

	private static String getMasterFilePath( ISPDynamicData isp )
	{
		return new String( getConfigPath( isp ) + "master.cfg" );
	}
	
	private static ISPDynamicData getISPDynamicData( String ispName )
	{
		//Trace.TRACE( "getISPDynamicData: " + ispName );
		
		for ( int i = 0; i < ispList.size(); i++ )
		{
			ISPDynamicData		ispData = (ISPDynamicData)ispList.elementAt( i );
			String				name = ispData.getName();
			
			if ( name.compareTo( ispName ) == 0 )
				return ispData;
		}
		return null;
	}
	
	private static NameValueSet getISPData( String ispName )
	{
		//Trace.TRACE( "getISPData: " + ispName );
		
		for ( int i = 0; i < nameValueSets.size(); i++ )
		{
			NameValueSet		nvSet = (NameValueSet)nameValueSets.elementAt( i );
			//nvSet.printNameValueSet();
			String				name = nvSet.getValue( "ISPName" );
			
			if ( name.compareTo( ispName ) == 0 )
				return nvSet;
		}
		return null;
	}
		
	public static String[] getISPPopList( String isp )
	{
		String		retArray[] = null;
		
		//Trace.TRACE( "getISPPopList" );

		try
		{
			ISPDynamicData		ispData = getISPDynamicData( isp );

			if ( ispData == null )
			{
				//Trace.TRACE( "isp not found: " + isp );
				return null;
			}
			
			int		size = ispData.getDynamicDataSize();
			
			//Trace.TRACE( "size: " + size );
			
			retArray = new String[ size ];
			
			while ( size > 0 )
			{
				size = size - 1;

				NameValueSet		nvSet = ispData.getDynamicData( size );
				String				phoneNum = nvSet.getValue( "phone" );
				String				city = nvSet.getValue( "city" );
				String				state = nvSet.getValue( "state" );
				
				String				entry = phoneNum + ", " + city + ", " + state;
				retArray[ size ] = entry;
				//Trace.TRACE( "entry:	" + entry );
			}
		}
		catch ( Throwable e )
		{
			retArray = null;
			e.printStackTrace();
		}
		return retArray;
	}
	
	public static void createConfigIAS( String ispName, int index ) 
	{
		// * fill in the variables in each "config.ias" with the dynamic data from the server
		ISPDynamicData      ispData = getISPDynamicData( ispName );
				
		String          inputFileName = getConfigFilePath( ispData );
		String          outputFileName = inputFileName + ".r";
		
		//Trace.TRACE( "inputFileName: " + inputFileName );
		//Trace.TRACE( "outputFileName: " + outputFileName );
		
		File            inputFile = new File( inputFileName );
		File            outputFile = new File( outputFileName );
		
	
		try
		{
			NameValueSet		nvSet = ispData.getDynamicData( index );
			if ( nvSet == null )
				nvSet = new NameValueSet();
				
			executeNameValueReplacement( ispData.getDynamicData( index ), inputFile, outputFile );
		}
		catch ( Throwable e )
		{
			e.printStackTrace();
		}
		
		//if ( inputFile.delete() )
		//{
		//  Trace.TRACE( "deleted ISP config file" );
		//  if ( outputFile.renameTo( inputFile ) )
		//      ; //Trace.TRACE( "rename succeeded" );
		//}
	}

	public static String getISPDisplayName( String ispName )
	{
		NameValueSet		nvSet = getISPData( ispName );
		
		
		if ( nvSet != null )
		{
			//Trace.TRACE( "have nvSet" );
			String		ispDisplayName = new String ( nvSet.getValue( "DisplayName" ) );
			//Trace.TRACE( "ispDisplayName: " + ispDisplayName );
			return ispDisplayName;
		}
		return null;
	}
	
	/*
		Takes the given inputFile and looks for strings in the form
		"@@@string_to_replace@@@" and replaces them with the value
		found in nvSet, writing the output into outputFile
		
		@return             void
		@param nvSet        NameValueSet that contains the name/value pairs to use for replacement
		@param inputFile    file containing replacement strings in the form "@@@string_to_replace@@@"
		@param outputFile   file that is created
	*/
    public static void executeNameValueReplacement( NameValueSet nvSet, File inputFile, File outputFile )
    throws Exception
    {
        if ( outputFile.exists() )
        {
            //Trace.TRACE( "deleting outputfile: " + outputFile.getPath() );
            outputFile.delete();
        }

        BufferedReader  bufferedInputReader = new BufferedReader( new FileReader( inputFile ) );
        BufferedWriter  bufferedOutputWriter = new BufferedWriter( new FileWriter( outputFile ) );

        int c = bufferedInputReader.read();
        while ( c != -1 )
        {
            if ( c == '@' )
            {
                bufferedInputReader.mark( 1024 );

                boolean     successful = false;
                int         c1;
                int         c2 = -1;

                c1 = bufferedInputReader.read();
                if ( c1 != -1 )
                    c2 = bufferedInputReader.read();
                if ( c1 == '@' && c2 == '@' )
                {
                    StringBuffer        buffer = new StringBuffer( 256 );

                    int c3 = bufferedInputReader.read();

                    while ( c3 != -1 && c3 != '@' )
                    {
                        buffer.append( (char)c3 );
                        c3 = bufferedInputReader.read();
                    }
                    //Trace.TRACE( "read identifier: " + buffer.toString() );

                    if ( c3 == '@' )
                    {
                        c2 = -1;
                        c1 = bufferedInputReader.read();
                        if ( c1 != -1 )
                            c2 = bufferedInputReader.read();
                        if ( c1 == '@' && c2 == '@' )
                        {
                            //Trace.TRACE( "successful" );
                            successful = true;

                            String      name = new String( buffer );
                            //Trace.TRACE( "grepping nvSet for " + name );
                            //nvSet.printNameValueSet();
                            String      value;
                            if ( nvSet == null )
                                value = new String( "" );
                            else
                            {

                                value = nvSet.getValue( name );
                                if ( value == null )
                                    value = new String( "" );
                            }

                            //Trace.TRACE( "value is: " + value );
                            bufferedOutputWriter.write( value );
                        }
                    }
                }
                if ( !successful )
                {
                    //Trace.TRACE( "not successful, backing up stream" );
                    bufferedOutputWriter.write( c );
                    bufferedInputReader.reset();
                }
            }
            else
            {
                bufferedOutputWriter.write( c );
            }
            c = bufferedInputReader.read();
        }

        bufferedOutputWriter.flush();
        bufferedOutputWriter.close();
        bufferedInputReader.close();
    }

    /*
        Takes the bufferedInputReader and reads the stream looking for instances
        of ###filename_minus_extension###, then it will open "filename_minus_extension" + .mat
        and parse the contents of that file as a NameValueSet.

        Then, it looks at each NameValueSet in the Vector nvSets and if a given set in
        that vector is a superset of the .mat NameValueSet, a match is found, the file
        "filename_minus_extension" + .tmpl is opened and executeNameValueReplacement
        is called to create a file to be included in the bufferedOutputWriter (got that?)

        @return                         void
        @param bufferedInputReader      input file containing constraint template to be replaced
        @param bufferedOutputWriter     output file (html)
    */
    public static void executeConstraintReplacement( Vector inSets, BufferedReader bufferedInputReader, BufferedWriter bufferedOutputWriter )
    throws Exception
    {
		int c = bufferedInputReader.read();
		while ( c != -1 )
		{
			if ( c == '#' )
			{
				bufferedInputReader.mark( 1024 );

				boolean     successful = false;
				int         c1;
				int         c2 = -1;
				
				c1 = bufferedInputReader.read();
				if ( c1 != -1 )
				    c2 = bufferedInputReader.read();
				if ( c1 == '#' && c2 == '#' )
				{
					StringBuffer        buffer = new StringBuffer( 128 );
					
					int c3 = bufferedInputReader.read();
					
					while ( c3 != -1 && c3 != '#' )
					{
						buffer.append( (char)c3 );
						c3 = bufferedInputReader.read();
					}

					if ( c3 == '#' )
					{
						c2 = -1;
						c1 = bufferedInputReader.read();
						if ( c1 != -1 )
							c2 = bufferedInputReader.read();
						if ( c1 == '#' && c2 == '#' )
						{
                            successful = true;

                            String          criterionFileName = getMetadataPath() + buffer.toString() + ".mat"; /* will be something like "template1.mat" */
                            String          templateFileName = getMetadataPath() + buffer.toString() + ".tmpl"; /* will be something like "template1.tmpl" */
                            String          outputFileName = getMetadataPath() + buffer.toString() + ".html";

                            //Trace.TRACE( "criterionFile: " + criterionFileName );
                            //Trace.TRACE( "templateFile: " + templateFileName );
                            //Trace.TRACE( "outputFile: " + outputFileName );

                            File            templateFile = new File( templateFileName );
                            File            criterionFile = new File( criterionFileName );
                            File            outputFile = new File( outputFileName );

                            NameValueSet    criterionSet = new NameValueSet( criterionFile );
							criterionSet.printNameValueSet();
							
							for ( int i = 0; i < inSets.size(); i++ )
							{
								NameValueSet        nvSet = (NameValueSet)inSets.elementAt( i );
								//Trace.TRACE( "testing a set: " );
								//nvSet.printNameValueSet();
								if ( criterionSet.isSubsetOf( nvSet ) )
								{
									executeNameValueReplacement( nvSet, templateFile, outputFile );
									BufferedReader bufSubInputReader = new BufferedReader( new FileReader( outputFile ) );
									executeConstraintReplacement( inSets, bufSubInputReader, bufferedOutputWriter );
									bufSubInputReader.close();
								}
							}
						}
					}
				}
                if ( !successful )
                {
                    bufferedOutputWriter.write( c );
                    bufferedInputReader.reset();
                }
            }
            else
            {
                bufferedOutputWriter.write( c );
            }
            c = bufferedInputReader.read();
        }

        bufferedOutputWriter.flush();
    }


    /*
        Opens a connection to "sURL" and sends a POST with the data contained
        in "reggieData".  Each entry in "reggieData" should be a string of the
        form "name=value".  The result send back from the server is then
        parsed into individual ISPDynamicData's which are put into a Vector
        and returned

        @return                     Vector of ISPDynamicData's
        @param sURL                 URL of the server to send the POST to
        @param reggieData           String array with entries of the form "name=value" to send to server as POST
    */
    private static Vector parseMimeStream( String sURL, String reggieData[] ) throws Exception
    {

        int                 nBuffSize = 0;
        int                 count = 0;
        URLConnection       urlSrcConn = null;
        URL                 urlSrc = null;
        Vector              ispList = new Vector();
		ISPDynamicData		newData = null;

        state = CONTACTING_SERVER;

        //Trace.TRACE( "opening connection to registration server" );
        urlSrc = new URL( sURL );
        urlSrcConn = urlSrc.openConnection();

        state = SENDING;
        urlSrcConn.setDoOutput( true );
        urlSrcConn.setUseCaches( false );
        urlSrcConn.setAllowUserInteraction( true );

        Trace.TRACE( "sending POST to:" );
        Trace.TRACE( sURL );

        // * send the post
		PrintWriter out = new PrintWriter( urlSrcConn.getOutputStream() );
		String traceOut = new String();
		for ( count = 0; count < reggieData.length; count++ )
		{
			if ( count > 0 )
			{
				out.print( "&" );
				traceOut = traceOut + "&";
			}
			out.print( reggieData[ count ] );
			traceOut = traceOut + reggieData[ count ];
		}
		
        out.println();
        out.close();
	
		Trace.TRACE( traceOut );
        
        state = WAITING;

		try
		{
			String              buffer;
			
			InputStream         origStream = urlSrcConn.getInputStream();
			
			//Trace.TRACE( "creating reggie stream" );
			ReggieStream        is = new ReggieStream( origStream );
			
			state = RECEIVING_RESPONSE;
			
			buffer = is.nextToken();
			if ( buffer.compareTo( "STATUS" ) != 0 )
				throw new MalformedReggieStreamException( "no STATUS message sent" );
			
			buffer = is.nextToken();
			if ( buffer.compareTo( "OK" ) != 0 )
				throw new MalformedReggieStreamException( "STATUS not OK" );

			Trace.TRACE( "STATUS=OK" );
			
			buffer = is.nextToken();
			if ( buffer.compareTo( "SIZE" ) != 0 )
				throw new MalformedReggieStreamException( "no SIZE value sent" );

            totalBytes = new Integer( is.nextToken() ).intValue();

			Trace.TRACE( "SIZE=" + totalBytes );
			
			buffer = is.nextToken();
			if ( buffer.compareTo( REG_SOURCE_STRING ) != 0 )
				throw new MalformedReggieStreamException( "no REG_SOURCE value present" );
			
			buffer = is.nextToken();
			regSource = buffer;
			
			Trace.TRACE( "REG_SOURCE=" + regSource );
			
			while ( true )
			{
				//Trace.TRACE( "trying to create a new ISPDynamicData" );
				newData = new ISPDynamicData();
				newData.read( is );
			
				//newData.printISPDynamicData();
			
				ispList.addElement( newData );
			}
        }
        catch ( EOFException e )
        {
			state = DONE;
			ispList.addElement( newData );
			Trace.TRACE( "successfully finished downloading dynamic data" );
        }
 		catch ( IOException e )
		{
			state = ABORT;
			Trace.TRACE( "I/O exception (check the RegCGI URL in the .IAS file)" );
			ispList = null;
		}
		catch ( MalformedReggieStreamException e )
		{
			state = ABORT;
			Trace.TRACE( "malformed stream, saving partial dynamic data" );
		}

        //Trace.TRACE( "returning ispList" );
        return ispList;
    }

    public static void main( String[] arg )
    {
    //  generateComparePage( "https://reggie.netscape.com/IAS5/docs/reg.cgi",
    //      new String[] { "CST__PHONE_NUMBER=6743552", "CLIENT_LANGUAGE=en",
    //          "REG_SOURCE=APL", "CST_AREA_CODE_1=415", "CST_AREA_CODE_2=650",
    //          "CST_AREA_CODE_3=408" } );
    }

    private static void parseFeatureSet( NameValueSet ispSet, NameValueSet featureMapping )
    {
        String      featureList = ispSet.getValue( FEATURE_STRING );
        //Trace.TRACE( "features: " + featureList );

        for ( int i = 1; i <= FEATURE_COUNT; i++ )
        {
            String      featureName = "feature" + i;
            String      featureMappedName = featureMapping.getValue( featureName );

            //Trace.TRACE( featureName + " mapped to " + featureMappedName );

            // * featureMappedName will be something like "hosting" or "freetime"
            if (    featureMappedName != null &&
                    featureList != null &&
                    featureMappedName.compareTo( "" ) != 0 &&
                    featureList.indexOf( featureMappedName ) != -1 )
            {
                //Trace.TRACE( "going to show" );
                ispSet.setValue( featureName, "SHOW" );
            }
            else
            {
                //Trace.TRACE( "going to hide" );
                ispSet.setValue( featureName, "HIDE" );
            }
        }
    }

	private static void downloadAndUnzipMetadata( String rootURL ) throws Throwable
	{
	    String          zipFileURL =  rootURL + "metadata.jar";
		String			localFileName = getLocalPath() + "metadata.jar";
		
		ServerDownload.downloadURL( zipFileURL, localFileName );
		ServerDownload.unJarFile( localFileName, true );
	}		

	private static void downloadJarFiles( Vector ispList, String rootURL ) throws Throwable
	{
		// * download the ".jar" for each ISP
		for ( int i = 0; i < ispList.size(); i++ )
		{
		    ISPDynamicData      ispData;
		    ispData = (ISPDynamicData)ispList.elementAt( i );
		
		    String          zipFileURL =  rootURL + "ISP/" +
		                                    ispData.getLanguage() + "/" +
		                                    ispData.getName() + "/" +
		                                    "server_data/" +
		                                    ispData.getName() + ".jar";
		
		    String          ispLocalFileName = getJarFilePath( ispData );
		
		    currentFile = new String( ispData.name );
		
			Trace.TRACE( "downloading: " + zipFileURL );
		    ServerDownload.downloadURL( zipFileURL, ispLocalFileName );
		}
	}

	private static void decompressJarFiles( Vector ispList ) throws Throwable
	{
		// * decompress the ".jar" for each ISP
		for ( int i = 0; i < ispList.size(); i++ )
		{
		    ISPDynamicData      ispData;
		    ispData = (ISPDynamicData)ispList.elementAt( i );
		
		    String          ispLocalFileName = getJarFilePath( ispData );
		
		    currentFile = new String( ispLocalFileName );
		
			Trace.TRACE( "unzipping: " + ispLocalFileName );
		    ServerDownload.unJarFile( ispLocalFileName, true );
		}
	}

	private static Vector parseCompareFiles( Vector ispList, NameValueSet featureMappings ) throws Throwable
	{
		// * create a name/value set for each ISP by parsing the "compare.cfg" files
		Vector returnSets = new Vector();

		for ( int i = 0; i < ispList.size(); i++ )
		{
			ISPDynamicData      ispData;
			ispData = (ISPDynamicData)ispList.elementAt( i );
			
			String          ispConfigFileName = getCompareFilePath( ispData );
			String			ispMasterFileName = getMasterFilePath( ispData );
			
			//Trace.TRACE( "ispConfigFileName: " + ispConfigFileName );
			
			File            ispConfigFile = new File( ispConfigFileName );
			File			ispMasterFile = new File( ispMasterFileName );
			
			NameValueSet	nvSet = new NameValueSet();
			nvSet.setIgnoreSections( true );
			
			if ( ispConfigFile.exists() )
				nvSet.read( ispConfigFile );
			
			if ( ispMasterFile.exists() )
				nvSet.read( ispMasterFile );
				
			nvSet.setValue( ispDirectorySymbol, new String( ispData.language + "/" + ispData.name + "/client_data" ) );
			parseFeatureSet( nvSet, featureMappings );
			returnSets.addElement( nvSet );
				//nvSet.printNameValueSet();
		}
		return returnSets;
	}
	
	public static boolean generateComparePage( String inLocalPath, String sCGIUrl, 
		String sRootURL, String metadataMode, String reggieData[] )
	{
		done = false;

        Trace.TRACE( "Hello" );

        NameValueSet    featureMappings = null;
        boolean         result = false;

		localPath = new String( inLocalPath );
		
        try
        {
			//if ( progress == null )
			//  progress = new CPGeneratorProgress();
			
			//progress.show();
			//progress.toFront();
			//new Thread( progress ).start();
			
			// * send "POST" to registration server and parse the returned MIME stream
			ispList = parseMimeStream( sCGIUrl, reggieData );
			
			if ( metadataMode.toLowerCase().compareTo( "no" ) != 0 )
				downloadAndUnzipMetadata( sRootURL );
			
			ServerDownload.resetBytesDownloaded();
			
			downloadJarFiles( ispList, sRootURL );
			
			ServerDownload.resetBytesDownloaded();
			
			decompressJarFiles( ispList );
			
			//Trace.TRACE( "features.cfg settings: " );
			String		featuresConfigPath = getLocalPath() + "metadata" + 
												File.separator + "config" + File.separator + "features.cfg";


			featureMappings = new NameValueSet( new File( featuresConfigPath ) );
			
			nameValueSets = parseCompareFiles( ispList, featureMappings );
			
			//featureMappings.printNameValueSet();
						
            // * parse the feature list for each ISP
            //for ( int i = 0; i < dynamicData.size(); i++ )
            //{
        //      ISPDynamicData      ispData;
        //      ispData = (ISPDynamicData)dynamicData.elementAt( i );
        //
        //      ispData.parseFeatureSet( featureMappings );
        //      ispData.printISPDynamicData();
        //  }

			// * now, generate the compare page using the compare page template and the name/value pairs for each ISP
			File inputFile = new File( getMetadataPath() + comparePageTemplateFileName );
			File outputFile = new File( getLocalPath() + comparePageFileName );
			
			BufferedReader  bufferedReader = new BufferedReader( new FileReader( inputFile ) );
			BufferedWriter  bufferedWriter = new BufferedWriter( new FileWriter( outputFile ) );
			
			executeConstraintReplacement( nameValueSets, bufferedReader, bufferedWriter );
			bufferedWriter.close();
			bufferedReader.close();
			
			done = true;
			result = true;
			//System.in.read(); // prevent console window from going away
		}

		catch ( MalformedURLException e )
		{
			done = true;
			result = false;
		    Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}
		catch ( ConnectException e )
		{
			done = true;
			result = false;
		    Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}
		catch ( UnknownHostException e )
		{
			done = true;
			result = false;
			Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}
		catch ( FileNotFoundException e )
		{
			done = true;
			result = false;
		    Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}
		catch ( Exception e )
		{
			done = true;
			result = false;
		    Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}
		catch ( Throwable e )
		{
		    done = true;
		    result = false;
		    Trace.TRACE( "caught an exception" );
		    Trace.TRACE( e.getMessage() );
		    e.printStackTrace();
		}

		Trace.TRACE( "returning result: " + result );
		return result;
    }
}

