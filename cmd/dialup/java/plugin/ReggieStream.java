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

public class ReggieStream extends DataInputStream
{
	public ReggieStream( InputStream is )
	{
		super( is );
	}

	public String nextToken() throws Exception
	{
		byte        buffer[] = null;
		int         nBuffSize = 0;
		String      string = null;

		//Trace.TRACE( "nextToken has:" + this.available() );
		nBuffSize = this.readInt();
		//Trace.TRACE( "nBuffSize: " + nBuffSize );
		if ( nBuffSize == 0 )
            string = "";
		else if ( nBuffSize > 0 && nBuffSize < 4096 )
		{
			buffer = new byte[ nBuffSize ];
			//Trace.TRACE( "trying to read buffer" );
			this.readFully( buffer );
			//Trace.TRACE( "creating string" );
			string = new String( buffer );
			//Trace.TRACE( "read:" + string );
		}
		else
			throw new MalformedReggieStreamException( "invalid length for identifier" );

		//Trace.TRACE( string );
		return string;
    }
}
