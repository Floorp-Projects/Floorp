/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ----- BEGIN LICENSE BLOCK -----
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is .
 *
 * The Initial Developer of the Original Code is Netscape Communications Corporation.
 * Portions created by Netscape Communications Corporation are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Patrick C. Beard <beard@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ----- END LICENSE BLOCK ----- */

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

        void setParents(Vector parents) {
            mParents = new Entry[parents.size()];
            parents.copyInto(mParents);
        }

        public String toString() {
            String typeName = mType.mName;
            int typeSize = mType.mSize;
            String typeLink = "<A HREF=\"" + typeName + "_" + typeSize + ".html\">&LT;" + typeName + "&GT;</A> (" + typeSize + ")";
            return ("<A HREF=\"" + typeName + "_" + typeSize + ".html#" + mAddress + "\" onMouseDown='return showCrawl(event,\"" + mCrawlID + "\");'>" + mAddress + "</A> [" + mRefCount + "] " + typeLink + " {" + mTotalSize + "}");
        }
    }

    static class ByTypeBloat extends QuickSort.Comparator {
        Histogram hist;
        
        ByTypeBloat(Histogram hist) {
            this.hist = hist;
        }
    
        public int compare(Object obj1, Object obj2) {
            Entry e1 = (Entry) obj1, e2 = (Entry) obj2;
            Type t1 = e1.mType, t2 = e2.mType;
            return (hist.count(t2) * t2.mSize - hist.count(t1) * t1.mSize);
        }
    }

    /**
     * Sorts the bins of a histogram by (count * typeSize) to show the
     * most pressing leaks.
     */
    static class HistComparator extends QuickSort.Comparator {
        Histogram hist;
        
        HistComparator(Histogram hist) {
            this.hist = hist;
        }
    
        public int compare(Object obj1, Object obj2) {
            Type t1 = (Type) obj1, t2 = (Type) obj2;
            return (hist.count(t1) * t1.mSize - hist.count(t2) * t2.mSize);
        }
    }

    static void printHistogram(PrintWriter out, Histogram hist, String dir) throws IOException {
        // sort the types by histogram count.
        Object[] types = hist.objects();
        QuickSort sorter = new QuickSort(new HistComparator(hist));
        sorter.sort(types);
        
        out.println("<PRE>");
        int index = types.length;
        while (index > 0) {
            Type type = (Type) types[--index];
            int count = hist.count(type);
            String name = type.mName;
            int size = type.mSize;
            out.print("<A HREF=\"" + dir + name + "_" + size + ".html\">&LT;" + name + "&GT;</A> (" + size + ")");
            out.println(" : " + count + " {" + (count * type.mSize) + "}");
        }
        out.println("</PRE>");
    }
    
    static String kStackCrawlScript =
        "var currentURL = null;\n" +
        "function showCrawl(event, crawlID) {\n" +
        "   if (!(event.modifiers & Event.SHIFT_MASK)) return true;\n" +
        "   var l = document.layers['popup'];\n" +
        "   var docURL = document.URL;\n" +
        "   var crawlURL = docURL.substring(0, docURL.lastIndexOf('/') + 1) + crawlID + '.html';\n" +
        "   // alert(crawlURL);\n" +
        "   if (l.visibility == 'hide' || currentURL != crawlURL) {\n" +
        "      if (currentURL != crawlURL) {\n" +
        "         l.load(crawlURL, 800);\n" +
        "         currentURL = crawlURL;\n" +
        "      }\n" +
        "      l.top = event.target.y + 15;\n" +
        "      l.left = event.target.x + 30;\n" +
        "      l.visibility='show';\n" +
        "   } else {\n" +
        "      l.visibility = 'hide';\n" +
        "   }\n" +
        "   return false;\n" +
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
        String contentsDir = inputName + ".contents/"; 

        try {
            int objectCount = 0;
            long totalSize = 0;
            
            StringTable strings = new StringTable();
            strings.internAs("void*", "void");
            Hashtable types = new Hashtable();
            Histogram hist = new Histogram();
            CallTree calls = new CallTree(strings);
            Hashtable entriesTable = new Hashtable();
            Vector vec = new Vector();
            BufferedReader reader = new BufferedReader(new InputStreamReader(new FileInputStream(inputName)));
            String line = reader.readLine();
            while (line != null) {
                if (line.startsWith("0x")) {
                    String addr = strings.intern(line.substring(0, 10));
                    String name = strings.intern(line.substring(line.indexOf('<') + 1, line.indexOf('>')));
                    int size;
                    try {
                        String str = line.substring(line.indexOf('(') + 1, line.indexOf(')'));
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
                    
                    entriesTable.put(addr, new Entry(addr, type, refs, crawl));
                } else {
                    line = reader.readLine();
                }
            }
            reader.close();
            
            // build the entries array & graph.
            Entry[] entries = new Entry[objectCount];

            // now, we have a table full of leaked objects, lets derive reference counts, and build the graph.
            {
                Hashtable parentTable = new Hashtable();
                int entryIndex = 0;
                Enumeration e = entriesTable.elements();
                while (e.hasMoreElements()) {
                    Entry entry = (Entry) e.nextElement();
                    Object[] refs = entry.mReferences;
                    if (refs != null) {
                        int count = refs.length;
                        for (int i = 0; i < count; ++i) {
                            String addr = (String) refs[i];
                            Entry ref = (Entry) entriesTable.get(addr);
                            if (ref != null) {
                                // increase the ref count.
                                ref.mRefCount++;
                                // change string to ref itself.
                                refs[i] = ref;
                                // add entry to ref's parents vector.
                                Vector parents = (Vector) parentTable.get(ref);
                                if (parents == null) {
                                    parents = new Vector();
                                    parentTable.put(ref, parents);
                                }
                                parents.addElement(entry);
                            }
                        }
                    }
                    entries[entryIndex++] = entry;
                }

                // be nice to the GC.
                entriesTable.clear();
                entriesTable = null;
                
                // set the parents of each entry.
                e = parentTable.keys();
                while (e.hasMoreElements()) {
                    Entry entry = (Entry) e.nextElement();
                    Vector parents = (Vector) parentTable.get(entry);
                    if (parents != null)
                        entry.setParents(parents);
                }
                
                // be nice to the GC.
                parentTable.clear();
                parentTable = null;
            }
            
            // Sort the entries by type bloat.
            {
                QuickSort sorter = new QuickSort(new ByTypeBloat(hist));
                sorter.sort(entries);
            }

            // Create the bloat summary report.
            PrintWriter writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
            Date now = new Date();
            writer.println("<HTML><HEAD><TITLE>Bloat Summary as of: " + now + "</TITLE></HEAD>");
            
            // print bloat summary.
            writer.println("<H2>Bloat Summary</H2>");
            writer.println("total object count = " + objectCount + "<BR>");
            writer.println("total memory bloat = " + totalSize + " bytes.<BR>");
            
            // print histogram sorted by count * size.
            writer.println("<H2>Bloat Histogram</H2>");
            printHistogram(writer, hist, contentsDir);
            writer.println("</BODY></HTML>");
            writer.close();
            writer = null;
            
            // ensure the contents directory has been created.
            File contentsFile = new File(inputName + ".contents");
            if (!contentsFile.exists())
                contentsFile.mkdir();
            
            // create the stack crawl script.
            outputName = contentsDir + "showCrawl.js";
            writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
            writer.print(kStackCrawlScript);
            writer.close();
            writer = null;

            // print the Entries graph.
            int length = entries.length;
            Type anchorType = null;
            for (int i = 0; i < length; ++i) {
                Entry entry = entries[i];
                if (anchorType != entry.mType) {
                    if (writer != null) {
                        writer.println("</PRE>");
                        writer.close();
                    }
                    anchorType = entry.mType;
                    outputName = contentsDir + anchorType.mName + "_" + anchorType.mSize + ".html";
                    writer = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(outputName))));
                    // set up the stack crawl script. use a <LAYER> on Communicator 4.X, a separate
                    // window on other browsers.
                    writer.println("<TITLE>&LT;" + anchorType.mName + "&GT; (" + anchorType.mSize + ")</TITLE>");
                    writer.println("<SCRIPT src='showCrawl.js'></SCRIPT>");
                    writer.println("<LAYER NAME='popup' LEFT=0 TOP=0 BGCOLOR='#FFFFFF' VISIBILITY='hide'></LAYER>");
                    writer.println("<A NAME=\"" + anchorType.mName + "_" + anchorType.mSize + "\"></A>");
                    writer.println("<H3>" + anchorType + " Bloat</H3>");
                    writer.println("<PRE>");
                }
                writer.println("<A NAME=\"" + entry.mAddress + "\"></A>");
                if (entry.mParents != null) {
                    writer.print(entry);
                    writer.println(" <A HREF=\"#" + entry.mAddress + "_parents\">parents</A>");
                } else {
                    writer.println(entry);
                }
                // print object's fields:
                Object[] refs = entry.mReferences;
                if (refs != null) {
                    int count = refs.length;
                    for (int j = 0; j < count; ++j)
                        leaksoup.printField(writer, refs[j]);
                }
                // print object's stack crawl, if it hasn't been printed already.
                CallTree.Node crawl = entry.mCrawl;
                int crawlID = crawl.id;
                if (crawlID > 0) {
                    // encode already printed by negating the crawl id.
                    crawl.id = -crawlID;
                    File crawlFile = new File(contentsDir + crawlID + ".html");
                    if (! crawlFile.exists()) {
                        PrintWriter crawlWriter = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(crawlFile))));
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
                // print object's parents.
                if (entry.mParents != null) {
                    writer.println("<A NAME=\"" + entry.mAddress + "_parents\"></A>");
                    writer.println("\nObject Parents:");
                    Entry[] parents = entry.mParents;
                    int count = parents.length;
                    for (int j = 0; j < count; ++j)
                        writer.println("\t" + parents[j]);
                }
            }
            if (writer != null) {
                writer.println("</PRE>");
                writer.close();
            }
        } catch (Exception e) {
            e.printStackTrace(System.err);
        }
    }
}
