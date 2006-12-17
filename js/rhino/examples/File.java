/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Rhino code, released
 * May 6, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-2000
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Norris Boyd
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

import org.mozilla.javascript.*;
import java.io.*;
import java.util.Vector;

/**
 * Define a simple JavaScript File object.
 *
 * This isn't intended to be any sort of definitive attempt at a
 * standard File object for JavaScript, but instead is an example
 * of a more involved definition of a host object.
 *
 * Example of use of the File object:
 * <pre>
 * js> defineClass("File")
 * js> file = new File("myfile.txt");
 * [object File]
 * js> file.writeLine("one");                       <i>only now is file actually opened</i>
 * js> file.writeLine("two");
 * js> file.writeLine("thr", "ee");
 * js> file.close();                                <i>must close file before we can reopen for reading</i>
 * js> var a = file.readLines();                    <i>creates and fills an array with the contents of the file</i>
 * js> a;
 * one,two,three
 * js>
 * </pre>
 *
 *
 * File errors or end-of-file signaled by thrown Java exceptions will
 * be wrapped as JavaScript exceptions when called from JavaScript,
 * and may be caught within JavaScript.
 *
 * @author Norris Boyd
 */
public class File extends ScriptableObject {

    /**
     * The zero-parameter constructor.
     *
     * When Context.defineClass is called with this class, it will
     * construct File.prototype using this constructor.
     */
    public File() {
    }

    /**
     * The Java method defining the JavaScript File constructor.
     *
     * If the constructor has one or more arguments, and the
     * first argument is not undefined, the argument is converted
     * to a string as used as the filename.<p>
     *
     * Otherwise System.in or System.out is assumed as appropriate
     * to the use.
     */
    public static Scriptable jsConstructor(Context cx, Object[] args,
                                           Function ctorObj,
                                           boolean inNewExpr)
    {
        File result = new File();
        if (args.length == 0 || args[0] == Context.getUndefinedValue()) {
            result.name = "";
            result.file = null;
        } else {
            result.name = Context.toString(args[0]);
            result.file = new java.io.File(result.name);
        }
        return result;
    }

    /**
     * Returns the name of this JavaScript class, "File".
     */
    public String getClassName() {
        return "File";
    }

    /**
     * Get the name of the file.
     *
     * Used to define the "name" property.
     */
    public String jsGet_name() {
        return name;
    }

    /**
     * Read the remaining lines in the file and return them in an array.
     *
     * Implements a JavaScript function.<p>
     *
     * This is a good example of creating a new array and setting
     * elements in that array.
     *
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object
     */
    public Object jsFunction_readLines()
        throws IOException
    {
        Vector v = new Vector();
        String s;
        while ((s = jsFunction_readLine()) != null) {
            v.addElement(s);
        }
        Object[] lines = new Object[v.size()];
        v.copyInto(lines);

        Scriptable scope = ScriptableObject.getTopLevelScope(this);
        Context cx = Context.getCurrentContext();
        return cx.newObject(scope, "Array", lines);
    }

    /**
     * Read a line.
     *
     * Implements a JavaScript function.
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object, or EOFException if the object
     *            reached the end of the file
     */
    public String jsFunction_readLine() throws IOException {
        return getReader().readLine();
    }

    /**
     * Read a character.
     *
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object, or EOFException if the object
     *            reached the end of the file
     */
    public String jsFunction_readChar() throws IOException {
        int i = getReader().read();
        if (i == -1)
            return null;
        char[] charArray = { (char) i };
        return new String(charArray);
    }

    /**
     * Write strings.
     *
     * Implements a JavaScript function. <p>
     *
     * This function takes a variable number of arguments, converts
     * each argument to a string, and writes that string to the file.
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object
     */
    public static void jsFunction_write(Context cx, Scriptable thisObj,
                                        Object[] args, Function funObj)
        throws IOException
    {
        write0(thisObj, args, false);
    }

    /**
     * Write strings and a newline.
     *
     * Implements a JavaScript function.
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object
     *
     */
    public static void jsFunction_writeLine(Context cx, Scriptable thisObj,
                                            Object[] args, Function funObj)
        throws IOException
    {
        write0(thisObj, args, true);
    }

    public int jsGet_lineNumber()
        throws FileNotFoundException
    {
        return getReader().getLineNumber();
    }

    /**
     * Close the file. It may be reopened.
     *
     * Implements a JavaScript function.
     * @exception IOException if an error occurred while accessing the file
     *            associated with this object
     */
    public void jsFunction_close() throws IOException {
        if (reader != null) {
            reader.close();
            reader = null;
        } else if (writer != null) {
            writer.close();
            writer = null;
        }
    }

    /**
     * Finalizer.
     *
     * Close the file when this object is collected.
     */
    protected void finalize() {
        try {
            jsFunction_close();
        }
        catch (IOException e) {
        }
    }

    /**
     * Get the Java reader.
     */
    public Object jsFunction_getReader() {
        if (reader == null)
            return null;
        // Here we use toObject() to "wrap" the BufferedReader object
        // in a Scriptable object so that it can be manipulated by
        // JavaScript.
        Scriptable parent = ScriptableObject.getTopLevelScope(this);
        return Context.javaToJS(reader, parent);
    }

    /**
     * Get the Java writer.
     *
     * @see File#jsFunction_getReader
     *
     */
    public Object jsFunction_getWriter() {
        if (writer == null)
            return null;
        Scriptable parent = ScriptableObject.getTopLevelScope(this);
        return Context.javaToJS(writer, parent);
    }

    /**
     * Get the reader, checking that we're not already writing this file.
     */
    private LineNumberReader getReader() throws FileNotFoundException {
        if (writer != null) {
            throw Context.reportRuntimeError("already writing file \""
                                             + name
                                             + "\"");
        }
        if (reader == null)
            reader = new LineNumberReader(file == null
                                        ? new InputStreamReader(System.in)
                                        : new FileReader(file));
        return reader;
    }

    /**
     * Perform the guts of write and writeLine.
     *
     * Since the two functions differ only in whether they write a
     * newline character, move the code into a common subroutine.
     *
     */
    private static void write0(Scriptable thisObj, Object[] args, boolean eol)
        throws IOException
    {
        File thisFile = checkInstance(thisObj);
        if (thisFile.reader != null) {
            throw Context.reportRuntimeError("already writing file \""
                                             + thisFile.name
                                             + "\"");
        }
        if (thisFile.writer == null)
            thisFile.writer = new BufferedWriter(
                thisFile.file == null ? new OutputStreamWriter(System.out)
                                      : new FileWriter(thisFile.file));
        for (int i=0; i < args.length; i++) {
            String s = Context.toString(args[i]);
            thisFile.writer.write(s, 0, s.length());
        }
        if (eol)
            thisFile.writer.newLine();
    }

    /**
     * Perform the instanceof check and return the downcasted File object.
     *
     * This is necessary since methods may reside in the File.prototype
     * object and scripts can dynamically alter prototype chains. For example:
     * <pre>
     * js> defineClass("File");
     * js> o = {};
     * [object Object]
     * js> o.__proto__ = File.prototype;
     * [object File]
     * js> o.write("hi");
     * js: called on incompatible object
     * </pre>
     * The runtime will take care of such checks when non-static Java methods
     * are defined as JavaScript functions.
     */
    private static File checkInstance(Scriptable obj) {
        if (obj == null || !(obj instanceof File)) {
            throw Context.reportRuntimeError("called on incompatible object");
        }
        return (File) obj;
    }

    /**
     * Some private data for this class.
     */
    private String name;
    private java.io.File file;  // may be null, meaning to use System.out or .in
    private LineNumberReader reader;
    private BufferedWriter writer;
}

