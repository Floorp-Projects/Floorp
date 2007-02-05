/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Rhino code, released May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 * Attila Szegedi
 * David P. Caldwell <inonit@inonit.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * the GNU General Public License Version 2 or later (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above. If
 * you wish to allow use of your version of this file only under the terms of
 * the GPL and not to allow others to use your version of this file under the
 * MPL, indicate your decision by deleting the provisions above and replacing
 * them with the notice and other provisions required by the GPL. If you do
 * not delete the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */

package org.mozilla.javascript;

import java.io.BufferedReader;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.File;
import java.io.FileFilter;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.PrintStream;
import java.net.URL;
import java.util.Arrays;
import java.util.Properties;

import junit.framework.Assert;
import junit.framework.TestCase;
import junit.framework.TestSuite;

import org.mozilla.javascript.tools.shell.Global;
import org.mozilla.javascript.tools.shell.Main;
import org.mozilla.javascript.tools.shell.ShellContextFactory;

/**
 * Executes the tests in the js/tests directory, much like jsDriver.pl does.
 * Excludes tests found in the js/tests/rhino-n.tests file.
 * @author Attila Szegedi
 */
public class StandardTests extends TestSuite
{
    public static TestSuite suite() throws Exception
    {
        TestSuite suite = new TestSuite("Standard JavaScript tests");
		
		File testDir = null;
		if (System.getProperty("mozilla.js.tests") != null) {
			testDir = new File(System.getProperty("mozilla.js.tests"));
		} else {		
			URL url = StandardTests.class.getResource(".");
			String path = url.getFile();
			int jsIndex = path.lastIndexOf("/js");
			if(jsIndex == -1)
			{
				throw new IllegalStateException("You aren't running the tests from within the standard mozilla/js directory structure");
			}
			path = path.substring(0, jsIndex + 3).replace('/', File.separatorChar);
			testDir = new File(path, "tests");
		}
        if(!testDir.isDirectory())
        {
            throw new FileNotFoundException(testDir + " is not a directory");
        }
        Properties excludes = new Properties();
        InputStream in = new FileInputStream(new File(testDir, "rhino-n.tests"));
        try
        {
            excludes.load(in);
        }
        finally
        {
            in.close();
        }
        for(int i = -1; i < 2; ++i)
        {
            TestSuite optimizationLevelSuite = new TestSuite("Optimization level " + i);
            addSuites(optimizationLevelSuite, testDir, excludes, i);
            suite.addTest(optimizationLevelSuite);
        }
        return suite;
    }
    
    private static void addSuites(TestSuite topLevel, File testDir, Properties excludes, int optimizationLevel)
    {
        File[] subdirs = testDir.listFiles(ShellTest.DIRECTORY_FILTER);
        Arrays.sort(subdirs);
        for (int i = 0; i < subdirs.length; i++)
        {
            File subdir = subdirs[i];
            String name = subdir.getName();
            TestSuite testSuite = new TestSuite(name);
            addCategories(testSuite, subdir, name + "/", excludes, optimizationLevel);
            topLevel.addTest(testSuite);
        }
    }
    
    private static void addCategories(TestSuite suite, File suiteDir, String prefix, Properties excludes, int optimizationLevel)
    {
        File[] subdirs = suiteDir.listFiles(ShellTest.DIRECTORY_FILTER);
        Arrays.sort(subdirs);
        for (int i = 0; i < subdirs.length; i++)
        {
            File subdir = subdirs[i];
            String name = subdir.getName();
            TestSuite testCategory = new TestSuite(name);
            addTests(testCategory, subdir, prefix + name + "/", excludes, optimizationLevel);
            suite.addTest(testCategory);
        }
    }
    
    private static void addTests(TestSuite suite, File suiteDir, String prefix, Properties excludes, int optimizationLevel)
    {
        File[] jsFiles = suiteDir.listFiles(ShellTest.TEST_FILTER);
        Arrays.sort(jsFiles);
        for (int i = 0; i < jsFiles.length; i++)
        {
            File jsFile = jsFiles[i];
            String name = jsFile.getName();
            if(excludes.containsKey(prefix + name))
            {
                continue;
            }
            suite.addTest(new JsTestCase(jsFile, optimizationLevel));
        }
    }
	
	private static class JunitStatus extends ShellTest.Status {
		final void running(File jsFile) {
			//	do nothing
		}
		
		final void failed(String s) {
			Assert.fail(s);
		}
		
		final void exitCodesWere(int expected, int actual) {
			Assert.assertEquals("Unexpected exit code", expected, actual);
		}
		
		final void outputWas(String s) {
			System.out.print(s);
		}
		
		final void threw(Throwable t) {
			Assert.fail(ShellTest.getStackTrace(t));
		}
		
		final void timedOut() {
			failed("Timed out.");
		}
	}
	
    private static final class JsTestCase extends TestCase
    {
        private final File jsFile;
        private final int optimizationLevel;
        
        JsTestCase(File jsFile, int optimizationLevel)
        {
            super(jsFile.getName() + (optimizationLevel == 1 ? "-compiled" : "-interpreted"));
            this.jsFile = jsFile;
            this.optimizationLevel = optimizationLevel;
        }
        
        public int countTestCases()
        {
            return 1;
        }

		private static class ShellTestParameters extends ShellTest.Parameters {
			int getTimeoutMilliseconds() {
				if (System.getProperty("mozilla.js.tests.timeout") != null) {
					return Integer.parseInt(System.getProperty("mozilla.js.tests.timeout"));
				}
				return 60000;
			}
		}
	
        public void runBare() throws Exception
        {
			final ShellContextFactory shellContextFactory = new ShellContextFactory();
			shellContextFactory.setOptimizationLevel(optimizationLevel);
			ShellTest.run(shellContextFactory, jsFile, new ShellTestParameters(), new JunitStatus());
        }
    }
}
