/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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

package com.netscape.jsdebugging.api.corba;

import java.net.URL;
import java.net.Socket;
import java.io.DataInputStream;
import java.io.DataOutputStream;

public class MyNaming
{
    /*
    * host is in the form: http://server[:port] e.g. http://warp
    * name is just the name of the object (without "/NameService/")
    */
    public static String resolveToIOR(String host, String name)
    {
        try
        {
            URL url = new URL(host);
            int port = url.getPort();
            if( port == -1 )
                port = 80;
            Socket socket = new Socket(url.getHost(), port);

            DataOutputStream dos = new DataOutputStream(socket.getOutputStream());
            dos.writeBytes("GET "+"/NameService/"+name+" HTTP/1.0\n\n");

            DataInputStream dis = new DataInputStream(socket.getInputStream());
            String line;
            while(null != (line = dis.readLine()))
            {
                // System.out.println("line: "+ line);
                if( line.startsWith("IOR:") )
                    return line;
            }
        }
        catch(Exception e)
        {
            // eat it...
        }
        return null;
    }

    public static org.omg.CORBA.Object resolve(String host, String name)
    {
        String IOR = resolveToIOR(host, name);
        if( null == IOR )
            return null;
        org.omg.CORBA.ORB orb = org.omg.CORBA.ORB.init();
        if( null == orb )
            return null;
        return orb.string_to_object(IOR);
    }
}    
