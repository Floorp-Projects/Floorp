/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

		
//		Trace.TRACE( "nextToken has:" + this.available() );

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
		}
		else
			throw new MalformedReggieStreamException( "invalid length for identifier" );
		
		//Trace.TRACE( "read:" + string );

		if ( string == null )
			string = "";
			
		return string;
    }
}
