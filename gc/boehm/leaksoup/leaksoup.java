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

class Leak extends Reference {
	String mName;
	long mCrawlOffset;
	short mCrawlCount;
	short mRefCount;
	short mChildCount;
	Leak[] mParents;
	int mTotalSize;
	boolean mMarked;

	Leak(String addr, Type type, Object[] refs, long crawlOffset, short crawlCount) {
	    super(addr, type, refs);
	    mName = addr;
		mCrawlOffset = crawlOffset;
		mCrawlCount = crawlCount;
		mRefCount = 0;
		mChildCount = 0;
		mParents = null;
		mTotalSize = 0;
		mMarked = false;
	}
	
	void setParents(Vector parents) {
		mParents = new Leak[parents.size()];
		parents.copyInto(mParents);
	}
	
	void computeTotalSize() {
		// first, mark this node as having been visited.
		// we only want to include nodes that haven't been
		// visited in our total size.
		mTotalSize = mType.mSize;
		
		// then, visit all nodes that haven't been visited,
		// and include their total size in ours.
		int count = mReferences.length;
		for (int i = 0; i < count; ++i) {
			Object ref = mReferences[i];
			if (ref instanceof Leak) {
				Leak leak = (Leak) ref;
				if (leak.mTotalSize == 0) {
					leak.computeTotalSize();
					mTotalSize += leak.mTotalSize;
				}
			}
		}
	}

	void clearTotalSize() {
		// first, clear our total size.
		mTotalSize = 0;
		
		// then, visit all nodes that haven't been visited,
		// and clear each one's total size.
		int count = mReferences.length;
		for (int i = 0; i < count; ++i) {
			Object ref = mReferences[i];
			if (ref instanceof Leak) {
				Leak leak = (Leak) ref;
				if (leak.mTotalSize != 0)
					leak.clearTotalSize();
			}
		}
	}

	void clearMarks() {
		// first, clear mark.
		mMarked = false;
		
		// then, visit all nodes that haven't been visited,
		// and clear each one's mark.
		int count = mReferences.length;
		for (int i = 0; i < count; ++i) {
			Object ref = mReferences[i];
			if (ref instanceof Leak) {
				Leak leak = (Leak) ref;
				if (leak.mMarked)
					leak.clearMarks();
			}
		}
	}
	
	static final char INDENT = '\t';
	
    void printGraph(PrintWriter out) {
	    printGraph(out, 0);
	    clearMarks();
    }

	private void printGraph(PrintWriter out, int indent) {
		// first, mark this node as having been visited.
		// we only want to include nodes that haven't been
		// visited in our total size.
		mMarked = true;
		for (int i = 0; i < indent; ++i)
		    out.print(INDENT);
		out.println(toString());
		
		// then, visit all nodes that haven't been visited,
		// and include their total size in ours.
		int count = mReferences.length;
		if (count > 0) {
		    int subIndent = indent + 1;
    		for (int i = 0; i < count; ++i) {
    			Object ref = mReferences[i];
    			if (ref instanceof Leak) {
    				Leak leak = (Leak) ref;
    				if (!leak.mMarked)
        				leak.printGraph(out, subIndent);
    			}
    		}
    	}
	}
	
	void printCycle(PrintWriter out) {
	    printCycle(out, 0);
	    clearMarks();
	}
	
	private void printCycle(PrintWriter out, int indent) {
		// first, mark this node as having been visited.
		// we only want to include nodes that haven't been
		// visited in our total size.
		mMarked = true;
		
		// then, visit all nodes that haven't been visited,
		// and include their total size in ours.
		if (mChildCount > 0) {
		    // don't print leaf nodes in a cycle. they aren't interesting.
    		for (int i = 0; i < indent; ++i)
    		    out.print(INDENT);
    		out.println(toString());
		    int subIndent = indent + 1;
    		int count = mReferences.length;
    		for (int i = 0; i < count; ++i) {
    			Object ref = mReferences[i];
    			if (ref instanceof Leak) {
    				Leak leak = (Leak) ref;
    				if (!leak.mMarked)
        				leak.printCycle(out, subIndent);
    			}
    		}
    	}
	}

	public String toString() {
		return ("<A HREF=\"#" + mName + "\">" + mName + "</A> [" + mRefCount + "] " + mType + "{" + mTotalSize + "}");
	}
	
	/**
	 * Sorts in order of increasing reference count.
	 */
	static class ByRefCount extends QuickSort.Comparator {
		public int compare(Object obj1, Object obj2) {
			Leak l1 = (Leak) obj1, l2 = (Leak) obj2;
			return (l1.mRefCount - l2.mRefCount);
		}
	}

	/**
	 * Sorts in order of decreasing number of children.
	 */
	public static class ByChildCount extends QuickSort.Comparator {
		public int compare(Object obj1, Object obj2) {
			Leak l1 = (Leak) obj1, l2 = (Leak) obj2;
			return (l2.mChildCount - l1.mChildCount);
		}
	}

	/**
	 * Sorts in order of decreasing total size.
	 */
	static class ByTotalSize extends QuickSort.Comparator {
		public int compare(Object obj1, Object obj2) {
			Leak l1 = (Leak) obj1, l2 = (Leak) obj2;
			return (l2.mTotalSize - l1.mTotalSize);
		}
	}
}

final class LineReader {
    BufferedReader reader;
    long offset;
    
    LineReader(BufferedReader reader) {
        this.reader = reader;
        this.offset = 0;
    }
    
    String readLine() throws IOException {
        String line = reader.readLine();
        if (line != null)
            offset += 1 + line.length();
        return line;
    }
    
    void close() throws IOException {
        reader.close();
    }
}

public class leaksoup {
	private static boolean ROOTS_ONLY = false;

	public static void main(String[] args) {
		if (args.length == 0) {
			System.out.println("usage:  leaksoup [-blame] [-lxr] [-assign] [-roots] leaks");
			System.exit(1);
		}
		
		// assume user want's blame URLs.
		FileLocator.USE_BLAME = true;
		FileLocator.ASSIGN_BLAME = false;
		ROOTS_ONLY = false;
		
		for (int i = 0; i < args.length; i++) {
			String arg = args[i];
			if (arg.charAt(0) == '-') {
				if (arg.equals("-blame"))
					FileLocator.USE_BLAME = true;
				else if (arg.equals("-lxr"))
					FileLocator.USE_BLAME = false;
				else if (arg.equals("-assign"))
					FileLocator.ASSIGN_BLAME = true;
				else if (arg.equals("-roots"))
					ROOTS_ONLY = true;
				else
					System.out.println("unrecognized option: " + arg);
			} else {
				cook(arg);
			}
		}
		
		// quit the application.
		System.exit(0);
	}
	
	static void cook(String inputName) {
		try {
			Vector vec = new Vector();
			Hashtable leakTable = new Hashtable();
			Hashtable types = new Hashtable();
			Histogram hist = new Histogram();
			LineReader reader = new LineReader(new BufferedReader(new InputStreamReader(new FileInputStream(inputName))));
			StringTable strings = new StringTable();
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

					// generate a unique type for this object.
					String key = strings.intern(name + "_" + size);
					Type type = (Type) types.get(key);
					if (type == null) {
						type = new Type(name, size);
						types.put(key, type);
					}
					
					// read in fields. could compress these by converting to Integer objects.
					vec.setSize(0);
					for (line = reader.readLine(); line != null && line.charAt(0) == '\t'; line = reader.readLine())
						vec.addElement(strings.intern(line.substring(1, 11)));
					Object[] refs = new Object[vec.size()];
					vec.copyInto(refs);
					vec.setSize(0);
					
				    // record the offset of the stack crawl, which will be read in and formatted at the end, to save memory.
					long crawlOffset = reader.offset;
					short crawlCount = 0;
					for (line = reader.readLine(); line != null && !line.startsWith("Leaked "); line = reader.readLine())
						++crawlCount;

					// record the leak.
					leakTable.put(addr, new Leak(addr, type, refs, crawlOffset, crawlCount));

					// count the leak types in a histogram.
					hist.record(type);
				} else {
					line = reader.readLine();
				}
			}
			reader.close();
			
			// don't need the interned strings table anymore.
			strings = null;
			
			Leak[] leaks = new Leak[leakTable.size()];
			int leakCount = 0;
			long totalSize = 0;

			Hashtable parentTable = new Hashtable();

			// now, we have a table full of leaked objects, lets derive reference counts, and build the graph.
			Enumeration e = leakTable.elements();
			while (e.hasMoreElements()) {
				Leak leak = (Leak) e.nextElement();
				Object[] refs = leak.mReferences;
				int count = refs.length;
				for (int r = 0; r < count; ++r) {
					String addr = (String) refs[r];
					Leak ref = (Leak) leakTable.get(addr);
					if (ref != null) {
						// increase the ref count.
						ref.mRefCount++;
						// change string to ref itself.
						refs[r] = ref;
						// add leak to ref's parents vector.
						Vector parents = (Vector) parentTable.get(ref);
						if (parents == null) {
							parents = new Vector();
							parentTable.put(ref, parents);
						}
						parents.addElement(leak);
					}
				}
				leaks[leakCount++] = leak;
				totalSize += leak.mType.mSize;
			}

			// be nice to the GC.
			leakTable.clear();
			leakTable = null;

            // sort the leaks by address, and find interior pointers.
            {
                QuickSort byAddress = new QuickSort(new Reference.ByAddress());
                byAddress.sort(leaks);
            }
            
            for (int i = 0; i < leakCount; ++i) {
                Leak leak = leaks[i];
                Object[] refs = leak.mReferences;
                int count = refs.length;
                short childCount = 0;
                for (int r = 0; r < count; ++r) {
                    if (refs[r] instanceof String) {
                        String addr = (String) refs[r];
                        if (addr.equals("0x00000000")) continue;
                        int address = (int) Long.parseLong(addr.substring(2), 16);
                        Leak ref = (Leak) Reference.findNearest(leaks, address);
                        if (ref != null) {
    						// increase the ref count.
    						ref.mRefCount++;
    						// change string to ref itself.
    						refs[r] = ref;
						    // add leak to ref's parents vector.
    						Vector parents = (Vector) parentTable.get(ref);
    						if (parents == null) {
    							parents = new Vector();
    							parentTable.put(ref, parents);
    						}
    						parents.addElement(leak);
    						++childCount;
                        }
                    } else {
                        ++childCount;
                    }
                }
                leak.mChildCount = childCount;
            }
			
			// set the parents of each leak.
			e = parentTable.keys();
			while (e.hasMoreElements()) {
				Leak leak = (Leak) e.nextElement();
				Vector parents = (Vector) parentTable.get(leak);
				if (parents != null)
					leak.setParents(parents);
			}
			
			// be nice to the GC.
			parentTable.clear();
			parentTable = null;
			
			// store the leak report in inputName + ".html"
			PrintWriter out = new PrintWriter(new BufferedWriter(new OutputStreamWriter(new FileOutputStream(inputName + ".html"))));

			Date now = new Date();
			out.println("<TITLE>Leaks as of " + now + "</TITLE>");

            // print leak summary.
            out.println("<H2>Leak Summary</H2>");
            out.println("total objects leaked = " + leakCount + "<BR>");
            out.println("total memory leaked  = " + totalSize + " bytes.<BR>");

    		printLeakHistogram(out, hist);
	        printLeakStructure(out, leaks);

			// open original file again, as a RandomAccessFile, to read in stack crawl information.
			
			// print the leak report.
			if (!ROOTS_ONLY) {
    			RandomAccessFile in = new RandomAccessFile(inputName, "r");
				printLeaks(in, out, leaks);
				in.close();
			}
			
			out.close();
		} catch (Exception e) {
			e.printStackTrace(System.err);
		}
	}
	
	/**
	 * Sorts the bins of a histogram by (count * typeSize) to show the
	 * most pressing leaks.
	 */
	static class ByTypeBinSize extends QuickSort.Comparator {
		Histogram hist;
		
		ByTypeBinSize(Histogram hist) {
			this.hist = hist;
		}
	
		public int compare(Object obj1, Object obj2) {
			Type t1 = (Type) obj1, t2 = (Type) obj2;
			return (hist.count(t1) * t1.mSize - hist.count(t2) * t2.mSize);
		}
	}

	static void printLeakHistogram(PrintWriter out, Histogram hist) throws IOException {
		// sort the types by histogram count.
		Object[] types = hist.objects();
		QuickSort byTypeBinSize = new QuickSort(new ByTypeBinSize(hist));
		byTypeBinSize.sort(types);
		
		out.println("<H2>Leak Histogram</H2>");
		out.println("<PRE>");
		int index = types.length;
		while (index > 0) {
			Type type = (Type) types[--index];
			int count = hist.count(type);
			out.println(type.toString() + " : " + count + " {" + (count * type.mSize) + "}");
		}
		out.println("</PRE>");
	}
	
	static void printLeakStructure(PrintWriter out, Leak[] leaks) {
        // print root leaks. consider only leaks with a reference
        // count of 0, which when fixed, will hopefully reclaim
        // all of the objects below them in the graph.
        {
            QuickSort byRefCount = new QuickSort(new Leak.ByRefCount());
            byRefCount.sort(leaks);
        }
        
        int rootCount = 0;
        int leakCount = leaks.length;
        for (int i = 0; i < leakCount; ++i) {
            Leak leak = leaks[i];
            if (leak.mRefCount > 0)
                break;
            ++rootCount;
            leak.computeTotalSize();
        }

        {
            QuickSort byTotalSize = new QuickSort(new Leak.ByTotalSize());
            byTotalSize.sort(leaks, rootCount);
        }

        out.println("<H2>Leak Roots</H2>");
        out.println("<PRE>");
        for (int i = 0; i < rootCount; ++i) {
            Leak leak = leaks[i];
            leak.printGraph(out);
        }
        out.println("</PRE>");

        // print leak cycles. traverse the leaks from objects with most number
        // of children to least, so that leaf objects will be printed after
        // their parents.
        {
            QuickSort byChildCount = new QuickSort(new Leak.ByChildCount());
            byChildCount.sort(leaks);
        }
        
        out.println("<H2>Leak Cycles</H2>");
        out.println("<PRE>");
        for (int i = 0; i < leakCount; ++i) {
            Leak leak = leaks[i];
            // if an object's total size isn't known yet, then it must
            // be a member of a cycle, since it wasn't reached when traversing roots.
            if (leak.mTotalSize == 0) {
                leak.computeTotalSize();
                leak.printCycle(out);
            }
        }
        out.println("</PRE>");
	}
	
	static StringBuffer appendChar(StringBuffer buffer, int ch) {
		if (ch > 32 && ch < 0x7F) {
			switch (ch) {
			case '<':	buffer.append("&LT;"); break;
			case '>':	buffer.append("&GT;"); break;
			default:	buffer.append((char)ch); break;
			}
		} else {
			buffer.append("&#183;");
		}
		return buffer;
	}
	
	static void printField(PrintWriter out, Object field) {
		String value = field.toString();
		if (field instanceof String) {
			// this is just a plain HEX value, print its contents as ASCII as well.
			if (value.startsWith("0x")) {
				try {
					int hexValue = Integer.parseInt(value.substring(2), 16);
					// don't interpret some common values, to save some space.
					if (hexValue != 0 && hexValue != -1) {
						StringBuffer buffer = new StringBuffer(value);
						buffer.append('\t');
						appendChar(buffer, ((hexValue >>> 24) & 0x00FF));
						appendChar(buffer, ((hexValue >>> 16) & 0x00FF));
						appendChar(buffer, ((hexValue >>>  8) & 0x00FF));
						appendChar(buffer, (hexValue & 0x00FF));
						value = buffer.toString();
					}
				} catch (NumberFormatException nfe) {
				}
			}
		}
		out.println("\t" + value);
	}

	static void printLeaks(RandomAccessFile in, PrintWriter out, Leak[] leaks) throws IOException {
		// sort the leaks by total size.
		QuickSort bySize = new QuickSort(new Leak.ByTotalSize());
		bySize.sort(leaks);

		// now, print the report, sorted by type size.
		out.println("<PRE>");
		Type anchorType = null;
        int leakCount = leaks.length;
		for (int i = 0; i < leakCount; ++i) {
			Leak leak = leaks[i];
			if (anchorType != leak.mType) {
				anchorType = leak.mType;
				out.println("\n<HR>");
				out.println("<A NAME=\"" + anchorType.mName + "_" + anchorType.mSize + "\"></A>");
				out.println("<H3>" + anchorType + " Leaks</H3>");
			}
			out.println("<A NAME=\"" + leak.mName + "\"></A>");
			if (leak.mParents != null) {
				out.print(leak);
				out.println(" <A HREF=\"#" + leak.mName + "_parents\">parents</A>");
			} else {
				out.println(leak);
			}
			// print object's fields:
			Object[] refs = leak.mReferences;
			int count = refs.length;
			for (int j = 0; j < count; j++)
				printField(out, refs[j]);
			// print object's stack crawl:
			in.seek(leak.mCrawlOffset);
			short crawlCount = leak.mCrawlCount;
			while (crawlCount-- > 0) {
			    String line = in.readLine();
				String location = FileLocator.getFileLocation(line);
				out.println(location);
			}
			// print object's parents.
			if (leak.mParents != null) {
				out.println("<A NAME=\"" + leak.mName + "_parents\"></A>");
				out.println("\nLeak Parents:");
				Leak[] parents = leak.mParents;
				count = parents.length;
				for (int j = 0; j < count; j++)
					out.println("\t" + parents[j]);
			}
		}
		out.println("</PRE>");
	}
}
