/**
 ****************************************************************************
 *
 *   @author   Raju Pallath
 *   @version  1.0
 *
 *   this class loads all the Test cases and executes them and returns the 
 *   pass/fail status.
 *   The Factory class loads this class and executes all Test Cases
 *   listed in a file whose path is set by env. variable BW_TESTDIR
 *   and filename by itself is set in BW_TESTFILE
 *   This class loops thru' each file entry and tries to execute each test
 *   case. 
 *
 ****************************************************************************
 */

package org.mozilla.dom.test;

import java.lang.*;
import java.util.*;
import java.io.*;
import org.mozilla.dom.test.*;

public class TestLoader
{


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
    targetObj = obj;
    returnType = areturnType;
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

     String testDir = ".";
     String testFile = TESTFILE;

     // Set Test directory
     testDir = propTable.getProperty("BW_TESTDIR");
     if (testDir == null) testDir=".";

     // Set Test Filename
     testFile = propTable.getProperty("BW_TESTFILE");
     if (testFile == null) testFile = TESTFILE;
  
     String fname =  testDir + "/" + testFile;
     FileInputStream in = null;

     try {
        in = new FileInputStream(fname);
     } catch (SecurityException e) {
        System.out.println ("Security Exception:Could not create stream for file " + fname);
        return null;
     } catch (FileNotFoundException e) {
        System.out.println ("Could not create stream for file " + fname);
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
        System.out.println("could not read Line from file fname...");
        line=null;
     }
     
     try {
       din.close();
     } catch (Exception e) {
        System.out.println ("Could not close file " + fname);
        return null;
     } 

     for (int i=0; i<fileVect.size(); i++)
     {
         String s = (String)fileVect.elementAt(i);


         Class c=null;
         try {
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

         if (((BWBaseTest)classObj).execute(targetObj)) {
              txtPrint(s, "PASSED");
         } else {
              txtPrint(s, "FAILED");
         }

         if (returnType == 1)
	 {
           return (((BWBaseTest)classObj).returnObject());
	 }
     }
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

     
     String fname = logDir + "/" + logFile;

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

     
    
     String fname = logDir + "/" + logFile;

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

     
     String fname = logDir + "/" + logFile;

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

     
     String fname = logDir + "/" + logFile;

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

     
     String fname = logDir + "/" + logFile;

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

  private Object targetObj;
  private int returnType;
  private static String TESTFILE      = "BWTestClass.lst";
  private static String PROPERTYFILE  = "BWProperties";
  private static String LOGFILE       = "BWTest.log";
  private static String LOGHTML       = "BWTest.html";
  private static String LOGTXT        = "BWTest.txt";
  public static Properties propTable  = new Properties();
}
