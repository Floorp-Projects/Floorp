/*
 * Task.java
 *
 * Created on 16 October 2005, 18:15
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */
package grendel;


/**
 * A Task to be run at a given point.
 * A Task may only be run once.
 * @author hash9
 */
public abstract class Task
{
  /**
   * Stores if the task has been run.
   */
  private boolean run = false;

  /** Creates a new instance of Task */
  public Task()
  {
  }

  /**
   * Execute the task.
   * @throws java.lang.IllegalStateException If the task has already been run
   * @throws java.lang.Exception If the task raises and Exception
   */
  public final void execute() throws IllegalStateException, Exception
  {
    if (run)
    {
      throw new IllegalStateException("This task has already been run");
    }

    run();
    run = true;
  }

  /**
   * Whether the task has been run.
   * @return true if the task has been run
   */
  public final boolean isRun()
  {
    return run;
  }

  /**
   * Sub classes should override this method to provide task logic.
   * @throws java.lang.Exception If an Exception occurs durring the task
   */
  protected abstract void run() throws Exception;
}
