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
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1997-1999
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

/**
 * checkParam.js
 *
 * The files given as arguments on the command line are assumed to be
 * Java source code files. This program checks to see that the @param
 * tags in the documentation comments match with the parameters for
 * the associated Java methods.
 * <p>
 * Any errors found are reported.
 *
 */
defineClass("File")

// Return true if "str" ends with "suffix".
function stringEndsWith(str, suffix) {
	return str.substring(str.length - suffix.length) == suffix;
}

/**
 * Perform processing once the end of a documentation comment is seen.
 *
 * Look for a parameter list following the end of the comment and
 * collect the parameters and compare to the @param entries.
 * Report any discrepancies.
 * @param f the current file
 * @param a an array of parameters from @param comments
 * @param line the string containing the comment end (in case the
 *        parameters are on the same line)
 */
function processCommentEnd(f, a, line) {
	while (line != null && !line.match(/\(/))
		line = f.readLine();
	while (line != null && !line.match(/\)/))
		line += f.readLine();
	if (line === null)
		return;
	var m = line.match(/\(([^\)]+)\)/);
	var args = m ? m[1].split(",") : [];
	if (a.length != args.length) {
		print('"' + f.name +
		      '"; line ' + f.lineNumber +
		      ' mismatch: had a different number' +
		      ' of @param entries and parameters.');
	} else {
		for (var i=0; i < a.length; i++) {
			if (!stringEndsWith(args[i], a[i])) {
				print('"' + f.name +
				      '"; line ' + f.lineNumber +
				      ' mismatch: had "' + a[i] +
				      '" and "' + args[i] + '".');
				break;
			}
		}
	}
}

/**
 * Process the given file, looking for mismatched @param lists and
 * parameter lists.
 * @param f the file to process
 */
function processFile(f) {
	var line;
	var m;
	var i = 0;
	var a = [];
      outer:
	while ((line = f.readLine()) != null) {
		if (line.match(/@param/)) {
			while (m = line.match(/@param[ 	]+([^ 	]+)/)) {
				a[i++] = m[1];
				line = f.readLine();
				if (line == null)
					break outer;
			}
		}
		if (i != 0 && line.match(/\*\//)) {
			processCommentEnd(f, a, line);
			i = 0;
			a = [];
		}
	}
	if (i != 0) {
		print('"' + f.name +
		      '"; line ' + f.lineNumber +
		      ' missing parameters at end of file.');
	}
}

// main script: process each file in arguments list

for (var i=0; i < arguments.length; i++) {
	var filename = String(arguments[i]);
	print("Checking " + filename + "...");
	var f = new File(filename);
	processFile(f);
}
print("done.");

