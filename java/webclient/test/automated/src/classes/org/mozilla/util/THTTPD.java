/*
 * $Id: THTTPD.java,v 1.2 2004/06/22 19:23:23 edburns%acm.org Exp $
 */

/* 
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Sun
 * Microsystems, Inc. Portions created by Sun are
 * Copyright (C) 1999 Sun Microsystems, Inc. All
 * Rights Reserved.
 *
 * Contributor(s): Ed Burns &lt;edburns@acm.org&gt;
 */

package org.mozilla.util;

import java.io.File;
import java.net.ServerSocket;
import java.net.Socket;
import java.io.FileInputStream;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.BufferedReader;
import java.io.BufferedWriter;


// THTTPD.java

public class THTTPD extends Object {

    public final static int PORT = 5243;

    public static class ServerThread extends Thread {

	protected File root = null;
	protected boolean keepRunning = true;
	protected int maxRequests = -1;
	protected int numRequests = 0;
	protected int count = 0;

	public final static int REQUEST_GET = 2;
	public final static int REQUEST_POST = 2;

	public ServerThread(String name, File root,
			    int maxRequests) {
	    super(name);
	    this.root = root;
	    this.maxRequests = maxRequests;
	    keepRunning = true;
	}

	protected int soTimeout = -1;
	public int getSoTimeout() {
	    return soTimeout;
	}
	
	public void setSoTimeout(int newSoTimeout) {
	    soTimeout = newSoTimeout;
	}

	public void stopRunning() {
	    keepRunning = false;
	    this.interrupt();
	}

	public void run() {
	    ServerSocket serverSocket = null;
	    Socket socket = null;
	    BufferedReader 
		responseReader = null,
		requestReader = null;
	    BufferedWriter 
		responseWriter = null;
	    String 
		requestLine = null,
		curLine = null;
	    File responseFile = null;
	    StringBuffer responseString = null;

	    V();
	    
	    while (keepRunning) {
		if (numRequests >= maxRequests) {
		    if (-1 != maxRequests) {
			break;
		    }
		}
		numRequests++;
		try {
		    serverSocket = new ServerSocket(PORT);
		    if (-1 != getSoTimeout()) {
			serverSocket.setSoTimeout(getSoTimeout());
		    }
		    socket = serverSocket.accept();
		    requestReader = new BufferedReader(new InputStreamReader(socket.getInputStream()));
		    requestLine = requestReader.readLine();
		    
		    while (null != (curLine = requestReader.readLine())) {
			if (curLine.trim().length() == 0) {
			    break;
			}
		    }
		   
		    switch (getRequestMethod(requestLine)) {
		    case REQUEST_GET:
			responseWriter = new BufferedWriter(new OutputStreamWriter(socket.getOutputStream()));
			if (null != 
			    (responseFile = getFileForRequestURI(getRequestURI(requestLine)))) {
			    curLine = "HTTP/1.0 200 OK\r\n";
			    responseWriter.write(curLine, 0, 
						 curLine.length());
			    responseReader = new BufferedReader(new InputStreamReader(new FileInputStream(responseFile)));
			    responseString = new StringBuffer();
			    while (null != (curLine = responseReader.readLine())) {
				responseString.append(curLine);
			    }
			    curLine = "Content-type: text/plain\r\nContent-Length: " + responseString.length() + "\r\n\r\n";
			    responseWriter.write(curLine, 0, 
						 curLine.length());
			    responseWriter.write(responseString.toString(),
						 0, responseString.length());
			    responseWriter.flush();
			    responseWriter.close();
			}
			break;
		    }
		    
		    socket.close();
		    serverSocket.close();
		}
		catch (Exception e) {
		    System.out.println("Exception: " + e + " " + 
				       e.getMessage());
		    stopRunning();
		}
	    }
	    V();
	}

	protected int getRequestMethod(String requestLine) {
	    int result = REQUEST_GET;
	    return result;
	}

	protected String getRequestURI(String requestLine) {
	    String result = null;
	    int space2, space1 = requestLine.indexOf(" ");
	    if (-1 == space1) {
		return result;
	    }
	    space2 = requestLine.indexOf(" ", ++space1);
	    if (-1 == space2) {
		return result;
	    }
	    result = requestLine.substring(space1, space2);
	    return result;
	}

	protected File getFileForRequestURI(String requestURI) {
	    File result = new File(root, requestURI);
	    if (!result.exists() || result.isDirectory()) {
		result = null;
	    }
	    return result;
	}

	public synchronized void P() {
	    while (count <= 0) {
		try { wait(); } catch (InterruptedException ex) {}
	    }
	    --count;
	}
	
	public synchronized void V() {
	    ++count;
	    notifyAll();
	}
	
    }
    

    public static void printUsage() {
    }

    public static void main(String args[]) {
	// validate args
	if ((args.length < 2) ||
	    (null == args[0] || 0 == args[0].length()) ||
	    (null == args[1] || 0 == args[1].length()) ||
	    (!args[0].equals("-root"))) {
	    printUsage();
	    return;
	}

	File root = new File(args[1]);
	if (!root.exists() || !root.isDirectory()) {
	    printUsage();
	}
	Object toNotify = new Object();

	ServerThread server = new ServerThread("THTTPD-MainThread", root, -1);
	server.start();
	server.P();
	server.P();
    }
}
