/*
 The contents of this file are subject to the Mozilla Public
 License Version 1.1 (the "License"); you may not use this file
 except in compliance with the License. You may obtain a copy of
 the License at http://www.mozilla.org/MPL/

 Software distributed under the License is distributed on an "AS
 IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 implied. See the License for the specific language governing
 rights and limitations under the License.

 The Original Code is mozilla.org code.

 The Initial Developer of the Original Code is Sun Microsystems,
 Inc. Portions created by Sun are
 Copyright (C) 1999 Sun Microsystems, Inc. All
 Rights Reserved.

 Contributor(s):
*/

package org.mozilla.dom.test;

import java.lang.*;
import java.util.*;
import java.io.*;
import java.net.URL;
import java.applet.Applet;
import org.w3c.dom.Document;
import org.mozilla.dom.DOMAccessor;
import org.mozilla.dom.DocumentLoadListener;
import org.mozilla.dom.test.*;

public class TestLoader extends Applet implements DocumentLoadListener 
{

  private Object targetObj;
  private int returnType = 0;
  private static String TESTFILE      = "BWTestClass.lst";
  private static String PROPERTYFILE  = "BWProperties";
  private static String LOGFILE       = "BWTest.log";
  private static String LOGHTML       = "BWTest.html";
  private static String LOGTXT        = "BWTest.txt";
  public static Properties propTable  = new Properties();
  private final boolean debug = true;
  private static String FILE_SEP = "/";

  /**
   ************************************************************************
   *  Default constructor
   *
   ************************************************************************
   */
  public TestLoader()
  {
	System.out.println("########################## Createing default TestLoader ...\n");
	try {
		throw new Exception();
	} catch (Exception e) {
		e.printStackTrace();
	}
        FILE_SEP = System.getProperty("file.separator");
  }

  /**
   ************************************************************************
   *  Constructor
   *
   *  @param    targetObj   Object instance (Node/Document/....)
   *  @param    areturnType if 1  then return Object expected
   *                        if 0  then no return Object expected
   *
   ************************************************************************
   */
  public TestLoader(Object obj, int areturnType)
  {
    System.out.println("########################## Createing TestLoader ...");
    targetObj = obj;
    returnType = areturnType;
    FILE_SEP = System.getProperty("file.separator");
  }


  /**
   *
   ************************************************************************
   *
   * Sets the Testing Target
   *
   *  @param  target     Target to be tested   
   *
   *  @return void
   *
   ************************************************************************
   *
   */
  void setTarget(Object target) 
  {
    targetObj = target;
  }
  
  /**
   *
   ************************************************************************
   *
   *  Load all the Test cases specified in file $BLACWOOD_TESTFILE
   *  It reads each entry and creates a runtime instantiation of each class
   *
   ************************************************************************
   *
   */
  public Object loadTest()
  {
     if (targetObj == null) {
         System.out.println("Target Object " + targetObj.toString() + " is null....");
         return null;
     }

     // Read Property File
     TestLoader.readPropertyFile();

     // Check Property Names, to see if provided with correct file
     // separators.
     // For Windows platform FILE_SEP is \ but it has to be escaped with 
     // another \ , so a property value would look like c:\\mozilla\\java
     // instead of c:\mozilla\java
     //
     String CHECK_SEP = "/";

     if ( FILE_SEP.compareTo("/") == 0) CHECK_SEP = "\\";
     Enumeration em = propTable.propertyNames();
     if (em == null) return null;

     while (em.hasMoreElements())
     {
         String name = (String)(em.nextElement());
         String val = propTable.getProperty(name);
         if (val == null) continue;
	 if (val.indexOf("BW_HTMLTEST") != -1) continue;
	 if (val.indexOf("BW_XMLTEST") != -1) continue;

         int idx =  val.indexOf(CHECK_SEP);
         if (idx !=  -1) {
             System.out.println("********** ERROR:  File Separator for Property " + name + " is incorrect in file " + PROPERTYFILE);
             return null;
         } 
     }


     String testDir = ".";
     String testFile = TESTFILE;

System.out.println("Inside LoadTest");
     // Set Test directory
     testDir = propTable.getProperty("BW_TESTDIR");
     if (testDir == null) testDir=".";

     // Set Test Filename
     testFile = propTable.getProperty("BW_TESTFILE");
     if (testFile == null) testFile = TESTFILE;
  
     String fname =  testDir + FILE_SEP + testFile;
     FileInputStream in = null;

     try {
        in = new FileInputStream(fname);
     } catch (SecurityException e) {
        System.out.println ("Security Exception:Could not create stream for file " + fname);
	System.exit(-1);
        return null;
     } catch (FileNotFoundException e) {
        System.out.println ("Could not create stream for file " + fname);
	System.exit(-1);
        return null;
     }

     BufferedReader din;
     try {
       din = new BufferedReader(new InputStreamReader(in));
     } catch (Exception e) {
        System.out.println ("Could not get Input Reader for file " + fname);
        return null;
     }

     String line= new String("");
     Vector fileVect = new Vector();
     try {
        while (line != null)
        {
            line = din.readLine();
            if (line != null)
	    {
		 if (line.charAt(0) == '#')
                      continue;
                 fileVect.addElement(line);
	    }
        }
     } catch (Exception e) {
        System.out.println("could not read Line from file " + fname);
        line=null;
     }
     
     try {
       din.close();
     } catch (Exception e) {
        System.out.println ("Could not close file " + fname);
        return null;
     } 

     // check if test have to be run in single Thread mode(S) or 
     // multi Thread mode (M)
     //
     String threadMode = propTable.getProperty("BW_THREADMODE");
     if (threadMode == null)
             threadMode = "S";

     for (int i=0; i<fileVect.size(); i++)
     {
         String s = (String)fileVect.elementAt(i);


         Class c=null;
         try {
	   System.out.println("############### Class name: "+s);
           c = Class.forName(s);
         } catch (ClassNotFoundException e) {
           System.out.println ("Could not find class " + s);
           continue;
         }

         Object classObj = null;
         try {
           classObj = c.newInstance();
         } catch (Exception e) {
           System.out.println ("Could not instantiate class " + s);
           continue;
         }

         // If single thread execution
         if (threadMode.compareTo("S") == 0)
         {
           try {
		System.out.println("################ Starting test ...");
              if (((BWBaseTest)classObj).execute(targetObj)) {
                 if (((BWBaseTest)classObj).isUnsupported())
                    txtPrint(s, "UNSUPPORTED METHOD");
                 else 
                    txtPrint(s, "PASSED");
		 System.out.println("################ passed");
              } else {
                 txtPrint(s, "FAILED");
		System.out.println("################ failed");
              }
           } catch (Exception e) {
		System.out.println("################ failed with exception: "+e);
              txtPrint(s, "FAILED: "+e);
           }

              // if any return type expected, then it is returned.
              // This is just a provision kept for latter use
              //
              //if (returnType == 1)
	      //{
              //  return (((BWBaseTest)classObj).returnObject());
	      //}
         } else {
              BWTestThread t = new BWTestThread(s);
           try {
              System.out.println("############## Starting test ...");
              if (t != null)
              {
                t.setTestObject(classObj, targetObj);
                t.start();
              }
           } catch (Exception e) {
              txtPrint(s, "FAILED: "+e);
           }         
        }

     }
     //txtPrint("Parent Thread Done", "PASSED");
     return null;
  }


  /**
   *******************************************************************
   *
   *  Read the property File and update the Property propTable
   *  the Property file BWProperties lists out all en. variables
   *  and their values.
   *
   *  @return void
   *
   *******************************************************************
   */
  public static void readPropertyFile()
  {
     if (propTable == null)
         return ;

     // Get Input Stream from Property file
     FileInputStream fin=null;
     try {
      fin = new FileInputStream("./" + PROPERTYFILE);
     } catch (Exception e) {
      System.out.println ("Security Exception:Could not create stream for file " + PROPERTYFILE);
      return;
     }


     try {
         propTable.load(fin);
     } catch (Exception e) {
         System.out.println("Could not load property file " + PROPERTYFILE);
	 return;
     }

     try {
         fin.close();
     } catch (Exception e) {
         System.out.println("Could not close " + PROPERTYFILE);
	 return;
     }


  }

  /**
   *
   ************************************************************************
   *  Routine which prints to log files and also to STDOUT
   *
   *  @param  msg    Message to be logged
   *
   *  @return void
   ************************************************************************
   *
   */
  public static void logErrPrint(String msg)
  {
      if (msg == null) return;
      System.out.println(msg);
      TestLoader.logPrint(msg);
  }


  /**
   *
   ************************************************************************
   *  Routine which prints to log files
   *
   *  @param  msg    Message to be logged
   *
   *  @return void
   ************************************************************************
   *
   */
  public static void logPrint(String msg)
  {
     if (msg == null) return;

     String logDir = propTable.getProperty("BW_LOGDIR");
     if (logDir == null) logDir = ".";

     String logFile = propTable.getProperty("BW_LOGFILE");
     if (logFile == null) logFile = LOGFILE;

     String fname = logDir + FILE_SEP  + logFile;

     // Get Output Stream from Log file
     RandomAccessFile raf=null;
     try {
      raf = new RandomAccessFile(fname, "rw");
     } catch (Exception e) {
      System.out.println ("Could not open file " + fname);
      return;
     }

     try {
	   long len = raf.length();
	   raf.seek(len);
           raf.write(msg.getBytes());
           raf.write("\n".getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + fname);
         return;
      }

     try {
       raf.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + fname);
       return;
     }
  }

  /**
   *
   ************************************************************************
   *  Routine which prints Header for HTML  file
   ************************************************************************
   *
   */
  public static void htmlPrintHeader()
  {
     String logDir = propTable.getProperty("BW_LOGDIR");
     if (logDir == null) logDir = ".";

     String logFile = LOGHTML;

     
    
     String fname = logDir + FILE_SEP +  logFile;

     File f=null;
     try {
       f = new File(logDir, logFile);
     } catch (Exception e) {
      System.out.println ("Could not get file Descriptor for file   " + fname);
      return;
     }
 
     if (f.exists())
     {
       File newf=null;
       String nom = logFile + ".bak";
       try {
         newf = new File(logDir, nom);
       } catch (Exception e) {
         System.out.println ("Could not get file Descriptor for file   " + nom);
         return;
       }
   
       try {
         f.renameTo(newf);
       } catch (Exception e) {
         System.out.println ("Could not rename file " + logFile + " to " + nom);
         return;
       }
     }

     // Get Output Stream from Log file
     RandomAccessFile raf=null;
     try {
      raf = new RandomAccessFile(fname, "rw");
     } catch (Exception e) {
      System.out.println ("Could not open file " + fname);
      return;
     }

     
     String msg=null;
     try {
	   raf.seek(0);
           Date dt = new Date();
           msg = "<html><head><title>\n";
           msg = msg + "DOMAPI Core Level 1 Test Status\n";
           msg = msg + "</title></head><body bgcolor=\"white\">\n";
           msg = msg + "<center><h1>\n";
           msg = msg + "DOM API Automated TestRun Results\n";
           msg = msg + dt.toString();
           msg = msg + "</h1></center>\n";
           msg = msg + "<hr noshade>";
           msg = msg + "<table bgcolor=\"#99FFCC\">\n";
           msg = msg + "<tr bgcolor=\"#FF6666\">\n";
           msg = msg + "<td>Test Case</td>\n";
           msg = msg + "<td>Result</td>\n";
           msg = msg + "</tr>\n";
           raf.write(msg.getBytes());
           raf.write("\n".getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + fname);
         return;
      }

     try {
       raf.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + fname);
       return;
     }
  }

  /**
   *
   ************************************************************************
   *  Routine which prints Footer for HTML  file
   ************************************************************************
   *
   */
  public static void htmlPrintFooter()
  {
     String logDir = propTable.getProperty("BW_LOGDIR");
     if (logDir == null) logDir = ".";

     String logFile = LOGHTML;

     
     String fname = logDir + FILE_SEP + logFile;

     // Get Output Stream from Log file
     RandomAccessFile raf=null;
     try {
      raf = new RandomAccessFile(fname, "rw");
     } catch (Exception e) {
      System.out.println ("Could not open file " + fname);
      return;
     }

     
     String msg=null;
     try {
	   long len = raf.length();
	   raf.seek(len);
           msg = "</table></body></html>\n";
           raf.write(msg.getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + fname);
         return;
      }

     try {
       raf.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + fname);
       return;
     }
  }



  /**
   *
   ************************************************************************
   *  Routine which prints to HTML files
   *
   *  @param  testCase    TestCase name (class name)
   *  @param  msg         Message to be logged
   *
   *  @return void
   ************************************************************************
   *
   */
  public static void htmlPrint(String testCase, String msg)
  {

     if (msg == null) return;
     if (testCase == null) return;

     String logDir = propTable.getProperty("BW_LOGDIR");
     if (logDir == null) logDir = ".";

     String logFile = LOGHTML;

     
     String fname = logDir + FILE_SEP  + logFile;

     // Get Output Stream from Log file
     RandomAccessFile raf=null;
     try {
      raf = new RandomAccessFile(fname, "rw");
     } catch (Exception e) {
      System.out.println ("Could not open file " + fname);
      return;
     }

     try {
           String msg1 = null;
	   long len = raf.length();
	   raf.seek(len);
           msg1 = "<tr><td>" + testCase + "</td>\n";
           msg1 = msg1 + "<td>" + msg + "</td>\n";
           msg1 = msg1 + "</tr>";
           raf.write(msg1.getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + fname);
         return;
      }

     try {
       raf.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + fname);
       return;
     }
  }


  /**
   *
   ************************************************************************
   *  Routine which prints to txt file
   *
   *  @param  testCase    TestCase name (class name)
   *  @param  msg         Message to be logged
   *
   *  @return void
   ************************************************************************
   *
   */
  public static void txtPrint(String testCase, String msg)
  {

     if (msg == null) return;
     if (testCase == null) return;

     String logDir = propTable.getProperty("BW_LOGDIR");
     if (logDir == null) logDir = ".";

     String logFile = TestLoader.LOGTXT;

     
     String fname = logDir + FILE_SEP + logFile;

     // Get Output Stream from Log file
     RandomAccessFile raf=null;
     try {
      raf = new RandomAccessFile(fname, "rw");
     } catch (Exception e) {
      System.out.println ("Could not open file " + fname);
      return;
     }

     try {
           String msg1 = null;
	   long len = raf.length();
	   raf.seek(len);
           msg1 = testCase + "=" + msg + "\n";
           raf.write(msg1.getBytes());
      } catch (IOException e) {
         System.out.println("ERROR: Could not write to File " + fname);
         return;
      }

     try {
       raf.close();
     } catch (Exception e) {
       System.out.println("ERROR: Could not close File " + fname);
       return;
     }
  }

  /*Implementing DocumentLoadListener interface*/
  public void endDocumentLoad(String url, int status, Document doc) 
  {
    System.out.println("################### Got Document: "+url);
    if ((!(url.endsWith(".html"))) && (!(url.endsWith(".xml")))) {
	System.out.println("################### Document is not HTML/XML ... "+url);
	return;
    }

    if (url.endsWith(".html"))
    {
      if (url.indexOf("test.html") == -1) {
        System.out.println("TestCases Tuned to run with test.html...");
        return;
      }
    }

    if (url.endsWith(".xml"))
    {
      if (url.indexOf("test.xml") == -1) {
        System.out.println("TestCases Tuned to run with test.xml...");
        return;
      }
    }
   
    Object obj = (Object) doc;

    setTarget(obj);
    System.out.println("################## Loading test ... ");
    try {
    	Object retobj = loadTest();
	System.out.println("################## test exited normally ... ");
    } catch (Exception e) {
    	System.out.println("################## test exited abnormally: \n" + e);	
    }

    doc = null;

  };
  public void startURLLoad(String url, String contentType, Document doc) {};
  public void endURLLoad(String url, int status, Document doc) {};
  public void progressURLLoad(String url, int progress, int progressMax, Document doc) {};
  public void statusURLLoad(String url, String message, Document doc) {};
  public void startDocumentLoad(String url) {};

  /*Overiding some Applet's methods */
  public void init() 
  {
    System.err.println("################## Regestring DocumentLoadListener !");
    DOMAccessor.addDocumentLoadListener((DocumentLoadListener)this);

    String testURL = propTable.getProperty("BW_HTMLTEST");
    if (testURL == null) {
      System.err.println("################# WARNING: BW_HTMLTEST property is not set ! Using file: protocol by default !");
      testURL="file:";
    }

    if (getParameter("test_type").equals("XML")) {	
      testURL = propTable.getProperty("BW_XMLTEST");
      if (testURL == null)
        testURL="file:";
      testURL+="/test.xml";
    } else if (getParameter("test_type").equals("HTML")) {
      testURL+="/test.html";
    } else {
      System.err.println("################ WARNING: Unrecognized test type (valid are HTML/XML):"+getParameter("test_type")+"\nLoading test.html by default.");
      testURL+="/test.html";
    }

    System.err.println("################## Loading "+testURL);
    try {
      getAppletContext().showDocument(new URL(testURL));
    } catch (Exception e) {
      System.err.println("############ Can't show test document: \nException: " + e.fillInStackTrace());
    }
  } 

}//end of class
