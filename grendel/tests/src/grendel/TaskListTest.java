/*
 * TaskListTest.java
 * JUnit based test
 *
 * Created on 16 October 2005, 20:54
 */

package grendel;

import junit.framework.*;
import grendel.messaging.ExceptionNotice;
import grendel.messaging.NoticeBoard;
import grendel.messaging.StringNotice;
import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintWriter;
import java.io.Reader;
import java.net.URI;
import java.net.URISyntaxException;
import java.net.URL;
import java.util.ArrayList;
import java.util.List;

/**
 *
 * @author hash9
 */
public class TaskListTest extends TestCase
{
  
  public TaskListTest(String testName)
  {
    super(testName);
  }

  public static Test suite()
  {
    TestSuite suite = new TestSuite(TaskListTest.class);
    
    return suite;
  }

  /**
   * Test of getHaltList method, of class grendel.TaskList.
   */
  public void testGetHaltList() throws Exception
  {
    System.out.println("getHaltList");

    TaskList result = TaskList.getHaltList();
    assertNotNull(result);

    System.out.println(result.toString());
  }

  /**
   * Test of getShutdownList method, of class grendel.TaskList.
   */
  public void testGetShutdownList() throws Exception
  {
    System.out.println("getShutdownList");
    
    TaskList result = TaskList.getShutdownList();
    assertNotNull(result);
    
    System.out.println(result.toString());
  }

  /**
   * Test of getStartupList method, of class grendel.TaskList.
   */
  public void testGetStartupList() throws Exception
  {
    System.out.println("getStartupList");
    
    TaskList result = TaskList.getStartupList();
    assertNotNull(result);
    
    System.out.println(result.toString());
  }
}
