/* -*- Mode: C; tab-width: 4, indent-tabs-mode: nil; -*- */
/*
 * @(#)executeJCKWrapper
 *
 * Copyright Notice
 * This file contains proprietary information of Netscape Communications.
 * Copying or reproduction without prior written approval is prohibited.
 *
 * Copyright (c) 1997
 *
 */

/* Package Name */

/* Imports */
import java.io.PrintStream;
import java.lang.reflect.Method;

/**
 * executeJCKWrapper runs a given simple JCK test.
 *
 * @param      args      Command line arguements.
 *
 * @author     Patrick Dionisio   <11-04-97 1:00pm>
 */
public class executeJCKWrapper {

  /**
   * The method that runs the test.
   *
   * @param      args  The command line arguments.
   *             log   Stream to report messages to.
   *
   * @author     Patrick Dionisio    <11-04-97 1:00pm>
   */
  public void test(String[] args, PrintStream log) {

    String className = null;
    String[] executeArgs = { };

    int i = 0;

    /* Get the executeClass. */
    if (i < args.length) {
      className = args[i];
      i++;
    }

    /* Get any optional args. */
    if (i < args.length) {
      executeArgs = new String[args.length - i];
      System.arraycopy(args, i, executeArgs, 0, executeArgs.length);
    }

    System.out.println(className);

    try {

      Class c;
      c = Class.forName(className);

      Class[] runParamTypes = {executeArgs.getClass(), log.getClass()};

      Method runMethod = c.getMethod("run", runParamTypes);

      /* Invoke the test class. */
      Object[] runArgs = {executeArgs,log};
      Object result = runMethod.invoke(null, runArgs);

      /* Print the result. */
      switch (((Integer)result).intValue()) {
      case 0:
	System.out.println("STATUS:Passed.");
	break;

      case 1:
	System.out.println("STATUS:Check me.");
	break;

      case 2:
	System.out.println("STATUS:Failed.");
	break;

      default:
	System.out.println("STATUS:Check me");

      }

    }

    catch(VerifyError error) {

      /* Assume that the verifier is enabled, catch any verify errors. */
      System.out.println("STATUS:Passed.");

    }

    catch(Exception e) {

      /* Print out any exceptions. */
      System.out.println("STATUS:Failed.");
      System.out.println("Exception caught.");
      e.printStackTrace();

    }

  }

  /**
   * Main method.  Checks is args is valid before running the test.
   *
   * @param      args  The command line arguments.
   *
   * @author     Patrick Dionisio    <11-04-97 1:00pm>
   */
  public static void main(String[] args) {

    /* Check if the number of args is valid. */
    if (args.length >= 1) {

      executeJCKWrapper run = new executeJCKWrapper();
      run.test(args, System.out);

    } else {

      System.out.println("usage: java executeJCKWrapper classname arguments");

    }

  }

}
