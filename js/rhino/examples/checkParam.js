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

print("here");
for (var i=0; i < arguments.length; i++) {
	var filename = String(arguments[i]);
	print("Checking " + filename + "...");
	var f = new File(filename);
	processFile(f);
}
print("done.");

