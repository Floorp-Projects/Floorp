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

import java.io.*;
import java.util.zip.*;
import java.net.*;
import netscape.security.*;
//import Trace;

class DownloadException extends Exception
{
    public DownloadException( String s )
    {
        super( s );
    }
}

class UnjarException extends Exception
{
    public UnjarException( String s )
    {
        super( s );
    }
}

public class ServerDownload
{
    public static final boolean SLEEP = false;
    public static final int     IDLE = 0;
    public static final int     DOWNLOADING = 1;
    public static final int     UNJARRING = 2;

    static int                  state = IDLE;
    static int                  bytesDownloaded = 0;

    public static int getState()
    {
        return state;
    }

    public static int getBytesDownloaded()
    {
        return bytesDownloaded;
    }

    public static void resetBytesDownloaded()
    {
        bytesDownloaded = 0;
    }

    /*  public static void main(String[] arg)
    {
        final String sURL = "http://scuba.mcom.com/~racham/testisp2.jar";
        final String sLocalDir = "temp" + File.separator + "isp";

        long startTime = System.currentTimeMillis();
        if ( !UnjarURL( sURL, sLocalDir, true ) )
        {
            System.out.println( sURL + " failed download/unjar!" );
        }
        else
        {
            try
            {
                long stopTime = System.currentTimeMillis();
                String str = "==> Download/Unjar elapsed time: " + ( stopTime - startTime ) + " ms\n";
                System.out.print( str );
                RandomAccessFile raf = new RandomAccessFile( "testisp.dat", "rw" );
                raf.seek( raf.length() );
                raf.writeBytes( str );
                raf.close();
            }
            catch ( IOException e )
            {
                System.out.println( e.getMessage() );
            }
        }
    }
    */

    /**
    * download a compressed (zip/jar) file and uncompress it into
    * the designated local directory
    *
    * @return                    true if download suceeds
    * @param sURL                URL of the file to be downloaded
    * @param sLocalFolder        name of the destination (local) file
    * @param bDelTempFile        delete downloaded file after file extractions
    */
    public static boolean unjarURL( String sURL, String sLocalFolder, boolean bDelTempFile )
    throws Exception
    {
        state = IDLE;

        boolean             bResult = false;

        // * downloaded file name: append filename to provided local folder
        StringBuffer        localFile = new StringBuffer( sLocalFolder );
        int                 nIndex = sURL.lastIndexOf( '/' );
        String              sFileName;

        if ( nIndex == -1 )
            throw new UnjarException( "Invalid folder" );

        sFileName = new String( sURL.getBytes(), nIndex + 1, sURL.length() - nIndex - 1 );
        localFile.append( File.separator + sFileName );
        //Trace.TRACE( "localfile: "+ localFile );

        if ( downloadURL( sURL, localFile.toString() ) )
        {
            //Trace.TRACE( sURL + " successfully downloaded" );
            if ( unJarFile( localFile.toString(), bDelTempFile ) )
            {
                //Trace.TRACE( sURL + " successfully decompressed" );
                bResult = true;
            }
        }

        return bResult;
    }

    /**
    * download URL/file
    *
    * @return                    true if download suceeds
    * @param sURL                URL of the file to be downloaded
    * @param sLocalFileName      name of the destination (local) file
    */
    public static boolean downloadURL( String sURL, String sDestFileName )
    throws Exception
    {
        final int               nBuffSize = 512;
        boolean                 bResult = false;
        DataInputStream         is = null;
        InputStream             tis = null;
        FileOutputStream        out = null;
        URL                     urlSrc = null;
        int                     nIndex;
        String                  sFolderName;

        //Trace.TRACE( "downloading " + sURL );

        state = ServerDownload.DOWNLOADING;

        urlSrc = new URL( sURL );

        // This is a really gross fix to a stupid little problem:
        // The openStream call erroneously barfs when opening the second
        // URL during the run of the app.  Worse, it cleans up and throws an
        // exception rather than restarting the operation.  Since the cleanup
        // appears to work, we're adding the restart at this level.
        // ** Beware removing this hack since the appearance of the symptom was
        // ** random - suggesting a garbage-collection related bug in openStream.

        try
        {
            is = new DataInputStream( urlSrc.openStream() );
        }
        catch ( Exception e )
        {
            ;
        }
        finally
        {
            is = new DataInputStream( urlSrc.openStream() );
        }

        nIndex = sDestFileName.lastIndexOf( File.separator );
        if ( nIndex != -1 )
        {
            sFolderName = new String( sDestFileName.getBytes(), 0,  nIndex );
            File        localFolder = new File( sFolderName );

            //Trace.TRACE( "localFolder: " + localFolder );

            // * create folder to store the downloaded file
            if ( !localFolder.mkdirs() && CPGenerator.DEBUG )
                Trace.TRACE( "FAILED making dirs for " + localFolder.getPath() );
        }

        out = new FileOutputStream( sDestFileName );
        byte buffer[] = new byte[ nBuffSize ];
        int nBytesRead = 0;

        while ( ( nBytesRead = is.read( buffer ) ) != -1 )
        {
            bytesDownloaded += nBytesRead;
            out.write( buffer, 0, nBytesRead );
            if ( SLEEP )
                Thread.sleep( 500 );
            else
                Thread.yield();
            out.flush();
        }

        // * close streams
        is.close();
        out.close();

        is = null;
        out = null;
        urlSrc = null;
        //urlSrcConn = null;

        bResult = true;

        state = IDLE;
        return bResult;
  }

    /**
    * Uncompress a JAR file
    *
    * @return true if file is decompressed successfully
    * @param sCompFile           name of the file to be decompressed
    * @param sDeleteJarFile      delete orig file after file extraction
    */
    public static boolean unJarFile( String sCompFile, boolean bDeleteJarFile )
    throws Exception
    {
        state = UNJARRING;

        boolean             bResult = false;
        final int           nBuffSize = 500;

        FileOutputStream    fileout = null;
        ZipInputStream      inflaterin = null;
        File                comprFile = null;

        try
        {
            //Trace.TRACE( "sCompFile: " + sCompFile );

            comprFile = new File( sCompFile );
            inflaterin = new ZipInputStream( new FileInputStream( sCompFile ) );

            //Trace.TRACE( "unjaring file: " + comprFile.getPath() );

            String      comprFilePath = comprFile.getPath();
            // construct folder to store extracted files
            String      localFolderName = comprFile.getParent();
            //Trace.TRACE( "localFolderName: " + localFolderName );

            byte[]      buffer = new byte[ nBuffSize ];
            int         nBytesRead = 0;
            ZipEntry    zEntry;

            while ( ( zEntry = inflaterin.getNextEntry() ) != null )
            {
                String      entryName = zEntry.getName();
                //Trace.TRACE( "next entry: " + entryName );

                File zEntryFile = new File( localFolderName + File.separator + entryName );
                //Trace.TRACE( "constructed target file: " + zEntryFile.getPath() );

                // * entry is a directory, create local directories on the filesystem
                if ( zEntry.isDirectory() )
                {
                    if ( !zEntryFile.mkdirs() && CPGenerator.DEBUG )
                       ; // Trace.TRACE( "FAILED creating folder " + zEntryFile.getPath() );
                }

                // * entry is a file: extract it
                else
                {
                    //Trace.TRACE( "--> extracting \"" + zEntry.getName() + "\" to " + zEntryFile.getPath() );

                    fileout = new FileOutputStream( zEntryFile.getPath() );
                    while ( ( nBytesRead = inflaterin.read( buffer, 0, nBuffSize ) ) != -1 )
                    {
                        bytesDownloaded += nBytesRead;
                        fileout.write( buffer, 0, nBytesRead );

                        if ( SLEEP )
                            Thread.sleep( 500 );
                        else
                            Thread.yield();
                    }

                    fileout.flush();
                    inflaterin.closeEntry();
                }
            }

            // * delete original JAR file if specified
            if ( bDeleteJarFile )
            {
                //Trace.TRACE( "deleting " + comprFile.getPath() + " status: "+ comprFile.delete() );
                comprFile.delete();
            }
            bResult = true;
        }
        finally
        {
            if ( fileout != null )
                fileout.close();
            if ( inflaterin != null )
                inflaterin.close();
        }

        state = IDLE;
        return bResult;
    }
}
