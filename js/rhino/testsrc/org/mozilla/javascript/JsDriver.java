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
 * The Original Code is the Java port of jsDriver.pl.
 *
 * The Initial Developer of the Original Code is
 * David P. Caldwell.
 * Portions created by David P. Caldwell are Copyright (C) 
 * 2007 David P. Caldwell. All Rights Reserved.
 *
 *
 * Contributor(s):
 *   David P. Caldwell <inonit@inonit.com>
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

import java.io.*;
import java.lang.reflect.Array;
import java.util.*;

import org.w3c.dom.*;

import org.mozilla.javascript.tools.shell.*;

public class JsDriver {
	private JsDriver() {
	}
	
	private static String join(String[] list) {
		String rv = "";
		for (int i=0; i<list.length; i++) {
			rv += list[i];
			if (i+1 != list.length) {
				rv += ",";
			}
		}
		return rv;
	}
	
	private static class Tests {
		private File testDirectory;
		private String[] list;
		private String[] skip;
		
		Tests(File testDirectory, String[] list, String[] skip, boolean skipRhinoN) {
			this.testDirectory = testDirectory;
			this.list = list;
			ArrayList skips = new ArrayList();
			skips.addAll(Arrays.asList(skip));
			if (skipRhinoN) {
				try {
					Properties rhinoN = new Properties();
					rhinoN.load( new FileInputStream( new File(testDirectory, "rhino-n.tests") ) );
					skips.addAll( rhinoN.keySet() );
				} catch (IOException e) {
					throw new RuntimeException("Could not read rhino-n.tests", e);
				}
			}
			this.skip = (String[])skips.toArray(new String[0]);
		}
		
		private boolean matches(String[] patterns, String path) {
			for (int i=0; i<patterns.length; i++) {
				if (path.startsWith(patterns[i])) {
					return true;
				}
			}
			return false;			
		}
		
		private boolean matches(String path) {
			if (list.length == 0) return true;
			return matches(list, path);
		}
		
		private boolean excluded(String path) {
			if (skip.length == 0) return false;
			return matches(skip, path);
		}
		
		private void addFiles(List rv, String prefix, File directory) {
			File[] files = directory.listFiles();
			if (files == null) throw new RuntimeException("files null for " + directory);
			for (int i=0; i<files.length; i++) {
				String path = prefix + files[i].getName();
				if (ShellTest.DIRECTORY_FILTER.accept(files[i])) {
					addFiles(rv, path + "/", files[i]);
				} else {
					boolean isTopLevel = prefix.length() == 0;
					if (ShellTest.TEST_FILTER.accept(files[i]) && matches(path) && !excluded(path) && !isTopLevel) {
						rv.add(new Script(path, files[i]));
					}
				}
			}
		}
		
		static class Script {
			private String path;
			private File file;
			
			Script(String path, File file) {
				this.path = path;
				this.file = file;
			}
			
			String getPath() {
				return path;
			}
			
			File getFile() {
				return file;
			}
		}
		
		Script[] getFiles() {
			ArrayList rv = new ArrayList();
			addFiles(rv, "", testDirectory);
			return (Script[])rv.toArray(new Script[0]);
		}
	}
	
	private static class ConsoleStatus extends ShellTest.Status {
		private File jsFile;
		
		private Arguments.Console console;
		private boolean trace;
		
		private boolean failed;
		
		ConsoleStatus(Arguments.Console console, boolean trace) {
			this.console = console;
			this.trace = trace;
		}
		
		void running(File jsFile) {
			try {
				console.println("Running: " + jsFile.getCanonicalPath());
				this.jsFile = jsFile;
			} catch (IOException e) {
				throw new RuntimeException(e);
			}
		}
		
		void failed(String s) {
			console.println("Failed: " + jsFile + ": " + s);
			failed = true;
		}
		
		void threw(Throwable t) {
			console.println("Failed: " + jsFile + " with exception.");
			console.println(ShellTest.getStackTrace(t));
			failed = true;
		}

		void timedOut() {
			console.println("Failed: " + jsFile + ": timed out.");
			failed = true;
		}
		
		void exitCodesWere(int expected, int actual) {
			if (expected != actual) {
				console.println("Failed: " + jsFile + " expected " + expected + " actual " + actual);
				failed = true;
			}
		}
		
		void outputWas(String s) {
			if (!failed) {
				console.println("Passed: " + jsFile);
				if (trace) {
					console.println(s);
				}				
			}
		}
	}
	
	//	returns true if node was found, false otherwise
	private static boolean setContent(Element node, String id, String content) {
		if (node.getAttribute("id").equals(id)) {
			node.setTextContent(node.getTextContent() + "\n" + content);
			return true;
		} else {
			NodeList children = node.getChildNodes();
			for (int i=0; i<children.getLength(); i++) {
				if (children.item(i) instanceof Element) {
					Element e = (Element)children.item(i);
					boolean rv = setContent( e, id, content );
					if (rv) {
						return true;
					}
				}
			}
		}
		return false;
	}
	
	private static Element getElementById(Element node, String id) {
		if (node.getAttribute("id").equals(id)) {
			return node;
		} else {
			NodeList children = node.getChildNodes();
			for (int i=0; i<children.getLength(); i++) {
				if (children.item(i) instanceof Element) {
					Element rv = getElementById( (Element)children.item(i), id );
					if (rv != null) {
						return rv;
					}
				}
			}
		}
		return null;
	}
	
	private static class HtmlStatus extends ShellTest.Status {
		private String testPath;
		private String bugUrl;
		private String lxrUrl;
		private Document html;
		private Element failureHtml;
		
		private File file;
		private boolean failed;
		
		private String output;
		
		HtmlStatus(String lxrUrl, String bugUrl, String testPath, Document html, Element failureHtml) {
			this.testPath = testPath;
			this.bugUrl = bugUrl;
			this.lxrUrl = lxrUrl;
			this.html = html;
			this.failureHtml = failureHtml;
		}
		
		void running(File file) {
			this.file = file;
		}
		
		void failed(String s) {
			failed = true;
			setContent(failureHtml, "failureDetails.reason", "Failure reason: \n" + s);
		}
		
		void exitCodesWere(int expected, int actual) {
			if (expected != actual) {
				failed = true;
				setContent(failureHtml, "failureDetails.reason", "expected exit code " + expected + " but got " + actual);
			}
		}
		
		private String newlineLineEndings(String s) {
			StringBuffer rv = new StringBuffer();
			for (int i=0; i<s.length(); i++) {
				if (s.charAt(i) == '\r') {
					if (i+1<s.length() && s.charAt(i+1) == '\n') {
						//	just skip \r
					} else {
						//	Macintosh, substitute \n
						rv.append('\n');
					}
				} else {
					rv.append(s.charAt(i));
				}
			}
			return rv.toString();
		}
		
		void threw(Throwable e) {
			failed = true;
			setContent(failureHtml, "failureDetails.reason", "Threw Java exception:\n" + newlineLineEndings(ShellTest.getStackTrace(e)));
		}
		
		void timedOut() {
			failed = true;
			setContent(failureHtml, "failureDetails.reason", "Timed out.");
		}
		
		void outputWas(String s) {
			this.output = s;
		}
		
		private String getLinesStartingWith(String prefix) {
			BufferedReader r = new BufferedReader(new StringReader(output));
			String line = null;
			String rv = "";
			try {
				while( (line = r.readLine()) != null ) {
					if (line.startsWith(prefix)) {
						if (rv.length() > 0) {
							rv += "\n";
						}
						rv += line;
					}
				}
				return rv;
			} catch (IOException e) {
				throw new RuntimeException("Can't happen.");
			}
		}
		
		boolean failed() {
			return failed;
		}
		
		void finish() {
			if (failed) {
				getElementById(failureHtml, "failureDetails.status").setTextContent(getLinesStartingWith("STATUS:"));
				
				String bn = getLinesStartingWith("BUGNUMBER:");
				Element bnlink = getElementById(failureHtml, "failureDetails.bug.href");
				if (bn.length() > 0) {
					String number = bn.substring("BUGNUMBER: ".length());
					if (!number.equals("none")) {
						bnlink.setAttribute("href", bugUrl + number);
						getElementById(bnlink, "failureDetails.bug.number").setTextContent(number);
					} else {
						bnlink.getParentNode().removeChild(bnlink);
					}
				} else {
					bnlink.getParentNode().removeChild(bnlink);
				}
				
				getElementById(failureHtml, "failureDetails.lxr").setAttribute("href", lxrUrl + testPath);
				getElementById(failureHtml, "failureDetails.lxr.text").setTextContent(testPath);
				
				getElementById(html.getDocumentElement(), "retestList.text").setTextContent(
					getElementById(html.getDocumentElement(), "retestList.text").getTextContent()
					+ testPath
					+ "\n"
				);
				
				getElementById(html.getDocumentElement(), "failureDetails").appendChild(failureHtml);				
			}
		}
	}
	
	private static class Results {
		private ShellContextFactory factory;
		private Arguments arguments;
		private File output;
		private boolean trace;
		
		private Document html;
		private Element failureHtml;
		
		private int tests;
		private int failures;
		
		private Document parse(InputStream in) {
			try {
				javax.xml.parsers.DocumentBuilderFactory factory = javax.xml.parsers.DocumentBuilderFactory.newInstance();
				factory.setValidating(false);
				javax.xml.parsers.DocumentBuilder dom = factory.newDocumentBuilder();
				return dom.parse(in);
			} catch (Throwable t) {
				throw new RuntimeException("Parser failure", t);
			}
		}
		
		private Document getTemplate() {
			return parse(getClass().getResourceAsStream("results.html"));
		}
		
		Results(ShellContextFactory factory, Arguments arguments, File output, boolean trace) {
			this.factory = factory;
			this.arguments = arguments;
			this.output = output;
			this.trace = trace;
			
			this.html = getTemplate();
			this.failureHtml = getElementById(html.getDocumentElement(), "failureDetails.prototype");
			if (this.failureHtml == null) {
				try {
					javax.xml.transform.TransformerFactory.newInstance().newTransformer().transform(
						new javax.xml.transform.dom.DOMSource(html),
						new javax.xml.transform.stream.StreamResult(System.err)
					);
				} catch (Throwable t) {
					throw new RuntimeException(t);
				}
				throw new RuntimeException("No");
			}
			this.failureHtml.getParentNode().removeChild(this.failureHtml);
		}
		
		private void write(Document template) {
			try {
				javax.xml.transform.TransformerFactory.newInstance().newTransformer().transform(
					new javax.xml.transform.dom.DOMSource(template),
					new javax.xml.transform.stream.StreamResult( new FileOutputStream(output) )
				);
			} catch (IOException e) {
				arguments.getConsole().println("Could not write results file to " + output + ": ");
				e.printStackTrace(System.err);
			} catch (javax.xml.transform.TransformerConfigurationException e) {
				throw new RuntimeException("Parser failure", e);
			} catch (javax.xml.transform.TransformerException e) {
				throw new RuntimeException("Parser failure", e);
			}
		}
		
		void run(String path, File test, ShellTest.Parameters parameters) {
			ConsoleStatus cStatus = new ConsoleStatus(arguments.getConsole(), trace);
			HtmlStatus hStatus = new HtmlStatus(arguments.getLxrUrl(), arguments.getBugUrl(), path, html, (Element)failureHtml.cloneNode(true));
			ShellTest.Status status = ShellTest.Status.compose(cStatus, hStatus);
			try {
				ShellTest.run(factory, test, parameters, status);
			} catch (Exception e) {
				throw new RuntimeException(e);
			}
			tests++;
			if (hStatus.failed()) {
				failures++;
			}
			hStatus.finish();
		}
		
		private void set(Document document, String id, String value) {
			getElementById(document.getDocumentElement(), id).setTextContent(value);
		}
		
		void finish(Date start, Date end) {
			long elapsedMs = end.getTime() - start.getTime();
			set(html, "results.testlist", join(arguments.getTestList()));
			set(html, "results.skiplist", join(arguments.getSkipList()));
			String pct = new java.text.DecimalFormat("##0.00").format( (double)failures / (double)tests * 100.0 );
			set(html, "results.results", "Tests attempted: " + tests + " Failures: " + failures + " (" + pct + "%)");
			set(html, "results.platform", "java.home=" + System.getProperty("java.home")
				+ "\n" + "java.version=" + System.getProperty("java.version")
				+ "\n" + "os.name=" + System.getProperty("os.name")
			);
			set(html, "results.classpath", System.getProperty("java.class.path").replace(File.pathSeparatorChar, ' '));
			int elapsedSeconds = (int)(elapsedMs / 1000);
			int elapsedMinutes = elapsedSeconds / 60;
			elapsedSeconds = elapsedSeconds % 60;
			String elapsed = "" + elapsedMinutes + " minutes, " + elapsedSeconds + " seconds";
			set(html, "results.elapsed", elapsed);
			set(html, "results.time", new java.text.SimpleDateFormat("MMMM d yyyy h:mm:ss aa").format(new java.util.Date()));
			write(html);
		}
	}
	
	private static class ShellTestParameters extends ShellTest.Parameters {
		private int timeout;
		
		ShellTestParameters(int timeout) {
			this.timeout = timeout;
		}
		
		int getTimeoutMilliseconds() {
			return timeout;
		}
	}
	
	void run(Arguments arguments) throws Throwable {
		//	TODO	Allow rhino-n tests to be skipped
		
		if (arguments.help()) {
			System.out.println("See mozilla/js/tests/README-jsDriver.html; note that some options are not supported.");
			System.out.println("Consult the Java source code at testsrc/org/mozilla/javascript/JsDriver.java for details.");
			System.exit(0);
		}
		
		ShellContextFactory factory = new ShellContextFactory();
		factory.setOptimizationLevel(arguments.getOptimizationLevel());
		
		File path = arguments.getTestsPath();
		if (path == null) {
			path = new File("../tests");
		}
		if (!path.exists()) {
			throw new RuntimeException("JavaScript tests not found at " + path.getCanonicalPath());
		}
		Tests tests = new Tests(path, arguments.getTestList(), arguments.getSkipList(), !arguments.ignoreRhinoSkipList());
		Tests.Script[] all = tests.getFiles();
		arguments.getConsole().println("Running " + all.length + " tests.");
		
		File output = new File("rhino-test-results." + new java.text.SimpleDateFormat("yyyy.MM.dd.HH.mm.ss").format(new Date()) + ".html");
		if (arguments.getOutputFile() != null) {
			output = arguments.getOutputFile();
		}
		
		Results results = new Results(factory, arguments, output, arguments.trace());
		Date start = new Date();
		for (int i=0; i<all.length; i++) {
			results.run(all[i].getPath(), all[i].getFile(), new ShellTestParameters(arguments.getTimeout()));
		}
		Date end = new Date();
		results.finish(start, end);
	}
	
	public static void main(Arguments arguments) throws Throwable {
		JsDriver driver = new JsDriver();
		driver.run(arguments);
	}
	
	public static abstract class Arguments {
		//	List of jsDriver.pl arguments
		
		//	-b URL, --bugurl=URL
		public abstract String getBugUrl();
		
		//	-c PATH, --classpath=PATH
		//	Does not apply; we will use the VM's classpath
		
		//	-e TYPE ..., --engine=TYPE ...
		//	Does not apply, but we should provide the ability to test various optimization levels
		public abstract int getOptimizationLevel();
		
		//	-f FILE, --file=FILE
		public abstract File getOutputFile();
		
		//	-h, --help
		public abstract boolean help();
		
		//	-j PATH, --javapath=PATH
		//	Does not apply; we will use this JVM
		
		//	-k, --confail
		//	TODO	Currently this is ignored; not clear precisely what it means (perhaps we should not be logging ordinary
		//			pass/fail to the console currently?
		public abstract boolean logFailuresToConsole();
		
		//	-l FILE ..., --list=FILE ...
		public abstract String[] getTestList();
		
		//	-L FILE ..., --neglist=FILE ...
		public abstract String[] getSkipList();
		
		//	-p PATH, --testpath=PATH
		public abstract File getTestsPath();
		
		//	-s PATH, --shellpath=PATH
		//	Does not apply; we will use the Rhino shell with any classes given on the classpath
		
		//	-t, --trace
		public abstract boolean trace();
		
		//	-u URL, --lxrurl=URL
		public abstract String getLxrUrl();
		
		//
		//	New arguments
		//
		
		//	--ignore-rhino-n
		public abstract boolean ignoreRhinoSkipList();
		
		//	--timeoutms
		public abstract int getTimeout();
		
		public static abstract class Console {
			public abstract void print(String message);
			public abstract void println(String message);
		}
		
		public abstract Console getConsole();
	}
	
	private static class CommandLineArguments extends Arguments {
		private ArrayList options = new ArrayList();
		
		private Option bugUrl = new Option("b", "bugurl", false, false, "http://bugzilla.mozilla.org/show_bug.cgi?id=");
		private Option optimizationLevel = new Option("o", "optimization", false, false, "-1");
		private Option outputFile = new Option("f", "file", false, false, null);
		private Option help = new Option("h", "help", false, true, null);
		private Option logFailuresToConsole = new Option("k", "confail", false, true, null);
		private Option testList = new Option("l", "list", true, false, null);
		private Option skipList = new Option("L", "neglist", true, false, null);
		private Option testsPath = new Option("p", "testpath", false, false, null);
		private Option trace = new Option("t", "trace", false, true, null);
		private Option lxrUrl = new Option("u", "lxrurl", false, false, "http://lxr.mozilla.org/mozilla/source/js/tests/");
		
		private Option ignoreRhinoN = new Option(null, "ignore-rhino-n", false, true, null);
		private Option timeout = new Option(null, "timeout", false, false, "60000");
		
		private Option classpath = new Option("c", "classpath", false, false, null).ignored();
		private Option engine = new Option("e", "engine", false, false, null).ignored();
		private Option javapath = new Option("j", "javapath", false, false, null).ignored();
		private Option shellpath = new Option("s", "shellpath", false, false, null).ignored();
		
		private Console console = new Console() {
			public void print(String s) {
				System.out.print(s);
			}
			
			public void println(String s) {
				System.out.println(s);
			}
		};
		
		private class Option {
			private String letterOption;
			private String wordOption;
			private boolean array;
			private boolean flag;
			private boolean ignored;
			
			private ArrayList values = new ArrayList();
			
			//	array: can this option have multiple values?
			//	flag: is this option a simple true/false switch?
			Option(String letterOption, String wordOption, boolean array, boolean flag, String unspecified) {
				this.letterOption = letterOption;
				this.wordOption = wordOption;
				this.flag = flag;
				this.array = array;
				if (!flag && !array) {
					this.values.add(unspecified);
				}
				options.add(this);
			}
			
			Option ignored() {
				this.ignored = true;
				return this;
			}
			
			int getInt() {
				return Integer.parseInt( getValue() );
			}
			
			String getValue() {
				return (String)values.get(0);
			}
			
			boolean getSwitch() {
				return values.size() > 0;
			}
			
			File getFile() {
				if (getValue() == null) return null;
				return new File(getValue());
			}
			
			String[] getValues() {
				return (String[])values.toArray(new String[0]);
			}
			
			void process(List arguments) {
				String option = (String)arguments.get(0);
				String dashLetter = (letterOption == null) ? (String)null : "-" + letterOption;
				if (option.equals(dashLetter) || option.equals("--" + wordOption)) {
					arguments.remove(0);
					if (flag) {
						values.add(0, (String)null );
					} else if (array) {
						while( arguments.size() > 0 && !( (String)arguments.get(0) ).startsWith("-") ) {
							values.add( (String)arguments.remove(0) );
						}
					} else {
						values.set(0, arguments.remove(0));
					}
					if (ignored) {
						System.err.println("WARNING: " + option + " is ignored in the Java version of the test driver.");
					}
				}
			}
		}
		
		public String getBugUrl() {
			return bugUrl.getValue();
		}
		
		public int getOptimizationLevel() {
			return optimizationLevel.getInt();
		}
		
		public File getOutputFile() {
			return outputFile.getFile();
		}
		
		public boolean help() {
			return help.getSwitch();
		}
		
		public boolean logFailuresToConsole() {
			return logFailuresToConsole.getSwitch();
		}
		
		public String[] getTestList() {
			return testList.getValues();
		}
		
		public String[] getSkipList() {
			return skipList.getValues();
		}
		
		public File getTestsPath() {
			return testsPath.getFile();
		}
		
		public boolean trace() {
			return trace.getSwitch();
		}
		
		public String getLxrUrl() {
			return lxrUrl.getValue();
		}
		
		public boolean ignoreRhinoSkipList() {
			return ignoreRhinoN.getSwitch();
		}
		
		public int getTimeout() {
			return timeout.getInt();
		}
		
		public Console getConsole() {
			return console;
		}
		
		void process(List arguments) {
			while(arguments.size() > 0) {
				String option = (String)arguments.get(0);
				if (option.startsWith("--")) {
					//	preprocess --name=value options into --name value
					if (option.indexOf("=") != -1) {
						arguments.set(0, option.substring(option.indexOf("=")));
						arguments.add(1, option.substring(option.indexOf("=") + 1));
					}
				} else if (option.startsWith("-")) {
					//	could be multiple single-letter options, e.g. -kht, so preprocess them into -k -h -t
					if (option.length() > 2) {
						for (int i=2; i<option.length(); i++) {
							arguments.add(1, "-" + option.substring(i,i+1));
						}
						arguments.set(0, option.substring(0,2));
					}
				}
				int lengthBefore = arguments.size();
				for (int i=0; i<options.size(); i++) {
					if (arguments.size() > 0) {
						((Option)options.get(i)).process(arguments);
					}
				}
				if (arguments.size() == lengthBefore) {
					System.err.println("WARNING: Ignoring unrecognized option: " + arguments.remove(0));
				}
			}
		}
	}
	
	public static void main(String[] args) throws Throwable {
		ArrayList arguments = new ArrayList();
		arguments.addAll(Arrays.asList(args));
		CommandLineArguments clArguments = new CommandLineArguments();
		clArguments.process(arguments);
		main(clArguments);
	}
}
