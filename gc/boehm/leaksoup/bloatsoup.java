/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express oqr
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 *
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

import java.io.*;
import java.util.*;

public class bloatsoup {
	public static void main(String[] args) {
		if (args.length == 0) {
			System.out.println("usage:  bloatsoup trace");
			System.exit(1);
		}

		FileLocator.USE_BLAME = true;
		
		for (int i = 0; i < args.length; i++) {
		    cook(args[i]);
		}
		
		// quit the application.
		System.exit(0);
	}
	
	/**
	 * An Entry is created for every object in the bloat report.
	 */
	static class Entry {
    	String mAddress;
    	Type mType;
    	Object[] mReferences;
    	CallTree.Node mCrawl;
    	int mCrawlID;
    	int mRefCount;
    	Entry[] mParents;
    	int mTotalSize;

    	Entry(String addr, Type type, Object[] refs, CallTree.Node crawl) {
    		mAddress = addr;
    		mReferences = refs;
    		mCrawl = crawl;
    		mCrawlID = crawl.id;
    		mRefCount = 0;
    		mType = type;
    		mTotalSize = 0;
    	}

    	public String toString() {
    		return ("<A HREF=\"#" + mAddress + "\" onMouseOver='return showCrawl(event,\"" + mCrawlID + "\");'>" + mAddress + "</A> [" + mRefCount + "] " + mType + " {" + mTotalSize + "}");
    	}
    }
    
    static String kStackCrawlScript =
        "function finishedLoad() {\n" +
        "   document.loaded = true;\n" +
        "   document.layers['popup'].visibility='show';\n" +
        "   return true;\n" +
        "}\n" +
        "var currentURL = null;\n" +
        "function showCrawl(event, addr) {\n" +
        "   if (!document.loaded) return true;\n" +
        "   var l = document.layers['popup'];\n" +
        "   var docURL = document.URL;\n" +
        "   var crawlURL = docURL.substring(0, docURL.lastIndexOf('/') + 1) + crawlDir + addr + '.html';\n" +
        "   // alert(crawlURL);\n" +
        "   if (currentURL != crawlURL) {\n" +
        "      l.load(crawlURL, 800);\n" +
        "      currentURL = crawlURL\n" +
        "      l.top = event.target.y + 15;\n" +
        "      l.left = event.target.x + 30;\n" +
        "   }\n" +
        "   l.visibility='show';\n" +
        "   return true;\n" +
        "}\n" +
        "";

    static String kStackCrawlPrefix =
        "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=3><TR><TD BGCOLOR=#00A000>\n" +
        "<TABLE BORDER=0 CELLSPACING=0 CELLPADDING=6><TR><TD BGCOLOR=#FFFFFF>\n" +
        "<PRE>\n" +
        "";
    static String kStackCrawlSuffix =
        "</PRE>\n" +
        "</TD></TR></TABLE>\n" +
        "</TD></TR></TABLE>\n" +
        "";

    /**
     * Initially, just create a histogram of the bloat.
     */
	static void cook(String inputName) {
		String outputName = inputName + ".html";
		String crawlDir = inputName + ".crawls/";

		try {
			int objectCount = 0;
			long totalSize = 0;
			
			StringTable strings = new StringTable();
			Hashtable types = new Hashtable();
			Histogram hist = new Histogram();
			CallTree calls = new CallTree(strings);
			Vector entries = new Vector();
			Vector vec = new Vector();
			BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(inputName)));
			String line = reader.readLine();
			while (line != null) {
				if (line.startsWith("0x")) {
					String addr = strings.intern(line.substring(0, 10));
					String name = strings.intern(line.substring(line.indexOf('<') + 1, line.indexOf('>')));
					int size;
					try {
						String str = line.substring(line.indexOf('(') + 1, line.indexOf(')')).trim();
						size = Integer.parseInt(str);
					} catch (NumberFormatException nfe) {
						size = 0;
					}
					String key = strings.intern(name + "_" + size);
					Type type = (Type) types.get(key);
					if (type == null) {
						type = new Type(name, size);
						types.put(key, type);
					}
					hist.record(type);
					
					++objectCount;
					totalSize += size;

                    line = reader.readLine();

                    // process references (optional).
                    Object[] refs = null;
                    if (line != null && line.charAt(0) == '\t') {
                        vec.setSize(0);
                        do {
                            vec.addElement(strings.intern(line.substring(1, 11)));
                            line = reader.readLine();
                        } while (line != null && line.charAt(0) == '\t');
                        refs = new Object[vec.size()];
                        vec.copyInto(refs);
                    }
                    
                    // process stack crawl (optional).
                    CallTree.Node crawl = null;
                    if (line != null && line.charAt(0) == '<') {
                        do {
                            crawl = calls.parseNode(line);
                            line = reader.readLine();
                        } while (line != null && line.charAt(0) == '<');
                    }
                    
                    entries.addElement(new Entry(addr, type, refs, crawl));
				} else {
    				line = reader.readLine();
    			}
			}
			reader.close();
			
			// build the Entries graph.

            // Create the output .html report.
			PrintWriter writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
			writer.println("<HTML><HEAD>");
			Date now = new Date();
			writer.println("<TITLE>Bloat as of " + now + "</TITLE>");
			
			// set up the stack crawl "layer". I know, this is deprecated, but still useful.
            writer.println("<SCRIPT>\n" + kStackCrawlScript +
                           "var crawlDir = '" + crawlDir + "';\n" + 
                           "</SCRIPT>");
            writer.println("</HEAD>\n<BODY onLoad='finishedLoad();'>");
            writer.println("<LAYER NAME='popup' onMouseOut='this.visibility=\"hide\";' LEFT=0 TOP=0 BGCOLOR='#FFFFFF' VISIBILITY='hide'></LAYER>");
            
			// print bloat summary.
			writer.println("<H2>Bloat Summary</H2>");
			writer.println("total object count = " + objectCount + "<BR>");
			writer.println("total memory bloat = " + totalSize + " bytes.<BR>");
			
            // print histogram sorted by count * size.
			writer.println("<H2>Bloat Histogram</H2>");
            leaksoup.printHistogram(writer, hist);
            
            File crawlFile = new File(inputName + ".crawls");
            if (!crawlFile.exists())
                crawlFile.mkdir();
            // String crawlPath = System.getProperty("user.dir") + "/" + crawlDir;
            
            // print the Entries graph.
			writer.println("<H2>Bloat Graph</H2>");
			writer.println("<PRE>");
			int count = entries.size();
			for (int i = 0; i < count; ++i) {
			    Entry entry = (Entry) entries.elementAt(i);
                writer.println("<A NAME=\"" + entry.mAddress + "\"></A>");
                writer.println(entry);
    			// print object's fields:
    			Object[] refs = entry.mReferences;
    			if (refs != null) {
        			int length = refs.length;
        			for (int j = 0; j < length; ++j)
        				leaksoup.printField(writer, refs[j]);
        		}
    			// print object's stack crawl, if it hasn't been printed already.
    			CallTree.Node crawl = entry.mCrawl;
    			int crawlID = crawl.id;
    			if (crawlID > 0) {
    			    // encode already printed by negating the crawl id.
    			    crawl.id = -crawlID;
        			PrintWriter crawlWriter = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(crawlDir + crawlID + ".html"))));
        			crawlWriter.print(kStackCrawlPrefix);
        			while (crawl != null) {
        				String location = FileLocator.getFileLocation(crawl.data);
        				crawlWriter.println(location);
        				crawl = crawl.parent;
        			}
        			crawlWriter.print(kStackCrawlSuffix);
        			crawlWriter.close();
        		}
			}
			writer.println("</PRE>");
			writer.println("</BODY></HTML>");

			writer.close();
		} catch (Exception e) {
			e.printStackTrace(System.err);
		}
	}
}
