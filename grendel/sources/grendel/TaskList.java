/*
 * TaskList.java
 *
 * Created on 16 October 2005, 18:21
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel;

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
 * A task list containing a list of tasks.
 * @author Kieran Maclean
 */
public final class TaskList extends ArrayList<Task> implements List<Task>
{
  /**
   * Standard shutdown comment
   */
  public static final String COMMENT_SHUTDOWN = "This file lists the Tasks to be preformed at normal shutdown";

  /**
   * Standard startup comment
   */
  public static final String COMMENT_STARTUP = "This file lists the Tasks to be preformed on startup";

  /**
   * Standard halt comment
   */
  public static final String COMMENT_HALT = "This file lists the Tasks to be preformed at abnormal shutdown\n# Keep this file short!";

  /**
   * Standard shutdown tasklist resource path
   */
  public static final String FILE_SHUTDOWN = "/grendel/tasks.shutdown.tl";

  /**
   * Standard startup tasklist resource path
   */
  public static final String FILE_STARTUP = "/grendel/tasks.startup.tl";

  /**
   * Standard halt tasklist resource path
   */
  public static final String FILE_HALT = "/grendel/tasks.term.tl";

  /**
   * The halt TaskList.
   */
  private static TaskList halt_list = null;

  /**
   * The shutdown TaskList.
   */
  private static TaskList shutdown_list = null;

  /**
   * The startup TaskList.
   */
  private static TaskList startup_list = null;

  /**
   * Create a new empty TaskList.
   */
  public TaskList()
  {
  }

  /**
   * Create a task list from the given resource.
   * @param tasklist to read lines from and parse as a task list.
   * @throws java.io.IOException if an IOException occurs
   */
  public TaskList(String tasklist) throws IOException
  {
    this(new InputStreamReader(TaskList.class.getResourceAsStream(tasklist)));
  }

  /**
   * Create a task list from the given URL.
   * @param tasklist to read lines from and parse as a task list.
   * @throws java.net.URISyntaxException if an URISyntaxException occurs
   * @throws java.io.FileNotFoundException if the file cannot be found
   * @throws java.io.IOException if an IOException occurs
   */
  public TaskList(URL tasklist)
    throws URISyntaxException, FileNotFoundException, IOException
  {
    this(tasklist.toURI());
  }

  /**
   * Create a task list from the given URI.
   * @param tasklist to read lines from and parse as a task list.
   * @throws java.io.FileNotFoundException if the file cannot be found
   * @throws java.io.IOException if an IOException occurs
   */
  public TaskList(URI tasklist) throws FileNotFoundException, IOException
  {
    this(new File(tasklist));
  }

  /**
   * Create a task list from the given File.
   * @param tasklist to read lines from and parse as a task list.
   * @throws java.io.FileNotFoundException if the file cannot be found
   * @throws java.io.IOException if an IOException occurs
   */
  public TaskList(File tasklist) throws FileNotFoundException, IOException
  {
    this(new FileReader(tasklist));
  }

  /**
   * Create a task list from the given Reader.
   * This constructor actually parses the task list.
   * @param r to read lines from and parse as a task list.
   * @throws java.io.IOException if an IOException occurs
   */
  private TaskList(Reader r) throws IOException
  {
    BufferedReader br = new BufferedReader(r);

    while (br.ready())
    {
      String line = br.readLine().trim();

      if (!(line.charAt(0) == '#'))
      {
        if (line.charAt(0) == '&')
        {
          try
          {
            addAll(new TaskList(line.substring(1)));
          }
          catch (IOException ioe)
          {
            ioe.printStackTrace();
          }
        }
        else
        {
          try
          {
            add(line);
          }
          catch (IllegalAccessException ex)
          {
            ex.printStackTrace();
          }
          catch (ClassNotFoundException ex)
          {
            ex.printStackTrace();
          }
          catch (InstantiationException ex)
          {
            ex.printStackTrace();
          }
        }
      }
    }
  }

  /**
   * Get the single halt task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the halt TaskList.
   */
  public static TaskList getHaltList() throws IOException
  {
    if (halt_list == null)
    {
      halt_list = new TaskList(FILE_HALT);
    }

    return halt_list;
  }

  /**
   * Get the single shutdown task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the shutdown TaskList.
   */
  public static TaskList getShutdownList() throws IOException
  {
    if (shutdown_list == null)
    {
      shutdown_list = new TaskList(FILE_SHUTDOWN);
    }

    return shutdown_list;
  }

  /**
   * Get the single startup task list.
   * @throws java.io.IOException if an IOException occurs
   * @return the startup TaskList.
   */
  public static TaskList getStartupList() throws IOException
  {
    if (startup_list == null)
    {
      startup_list = new TaskList(FILE_STARTUP);
    }

    return startup_list;
  }

  /**
   * Run the TaskList.
   * Execute each task in the task list.
   * Consective calls to this method will only run failed tasks.
   * <em>Exceptions <strong>are</strong> published</em>.
   */
  public void run()
  {
    for (Task t : this)
    {
      try
      {
        if (!t.isRun())
        {
          t.execute();
        }
      }
      catch (Throwable th)
      {
        NoticeBoard.publish(new ExceptionNotice(th));
      }
    }
  }

  /**
   * Write this TaskList to a file.
   * @param f the file to save to.
   * @param comment the comment to save in the file.
   * @throws java.io.IOException if an IOException occurs
   */
  public void toFile(File f, String comment) throws IOException
  {
    PrintWriter pw = new PrintWriter(new FileWriter(f));
    pw.println("# ");
    pw.println("# This is a special file.");
    pw.println("# It's syntax is as follows:");
    pw.println(
      "# Lines starting with a # are comments as are lines with # in them.");
    pw.println(
      "# All other lines are fully qualified classnames that extend from Task");
    pw.println("# Lines will be trimmed.");
    pw.println("# ");
    pw.println("# " + comment);
    pw.println("# ");
    pw.println();

    for (Task t : this)
    {
      String task_class = t.getClass().getCanonicalName();

      if (!((task_class == null) || (task_class.equals(""))))
      {
        pw.println(task_class);
      }
      else
      {
        NoticeBoard.publish(new StringNotice("Invalid Class Type for task: " +
            t.toString()));
      }
    }
  }

  /**
   * Write this TaskList to the standard halt file.
   * @throws java.io.IOException if an IOException occurs
   * @throws java.net.URISyntaxException if an URISyntaxException occurs
   */
  public void toHaltFile() throws IOException, URISyntaxException
  {
    toFile(new File(TaskList.class.getResource(FILE_HALT).toURI()),
      COMMENT_HALT);
  }

  /**
   * Write this TaskList to the standard shutdown file.
   * @throws java.io.IOException if an IOException occurs
   * @throws java.net.URISyntaxException if an URISyntaxException occurs
   */
  public void toShutdownFile() throws IOException, URISyntaxException
  {
    toFile(new File(TaskList.class.getResource(FILE_SHUTDOWN).toURI()),
      COMMENT_SHUTDOWN);
  }

  /**
   * Write this TaskList to the standard startup file.
   * @throws java.io.IOException if an IOException occurs
   * @throws java.net.URISyntaxException if an URISyntaxException occurs
   */
  public void toStartupFile() throws IOException, URISyntaxException
  {
    toFile(new File(TaskList.class.getResource(FILE_STARTUP).toURI()),
      COMMENT_STARTUP);
  }

  /**
   * Add a task to the task list from a given class name
   * @param task_class the class name to add to the task list.
   * @throws java.lang.InstantiationException if there was an InstantiationException durring the initalisation of the task
   * @throws java.lang.IllegalAccessException if there was an IllegalAccessException durring the initalisation of the task
   * @throws java.lang.ClassNotFoundException if there was an ClassNotFoundException durring the initalisation of the task
   * @return true as per the contract of Collections.
   */
  private boolean add(String task_class)
    throws InstantiationException, IllegalAccessException, 
      ClassNotFoundException
  {
    Class c = Class.forName(task_class);

    return add((Task) c.newInstance());
  }
}
