/**
 * Process a JavaScript source file and process special comments
 * to produce an HTML file of documentation, similar to javadoc.
 * @author Norris Boyd
 * @see rhinotip.jar
 * @lastmodified xx
 * @version 1.2 Roland Pennings: Allow multiple files for a function. 
 * @version 1.3 Roland Pennings: Removes ../.. from the input directory name
 */
defineClass("File")

var functionDocArray = [];
var inputDirName = "";
var indexFileArray = [];
var indexFile = "";
var indexFileName = "index_files";
var indexFunctionArray = [];
var indexFunction = "";
var indexFunctionName = "index_functions";
var FileList = [];
var DirList = [];
var outputdir = null;
var debug = 0;



/**
 * Process JavaScript source file <code>f</code>, writing jsdoc to 
 * file <code>out</code>.
 * @param f input file
 * @param fname name of the input file (without the path)
 * @param inputdir directory of the input file
 * @param out output file
 */
function processFile(f, fname, inputdir, out) {
	var s;
	var firstLine = true;
	indexFileArray[fname] = "";
	
    // write the header of the output file
	out.writeLine('<HTML><HEADER><TITLE>' + fname + '</TITLE><BODY>');
	if (inputdir != null) {
	  outstr = '<a name=\"_top_\"></a><pre><a href=\"' + indexFile + '\">Index Files</a> '; 
	  outstr += '<a href=\"' + indexFunction + '\">Index Functions</a></pre><hr>';
      out.writeLine(outstr);
	}

    // process the input file
	var comment = "";
	while ((s = f.readLine()) != null) {
      var m = s.match(/\/\*\*(.*)/);
	  if (m != null) {
		  // Found a comment start.
		  s = "*" + m[1];
		  do {
			m = s.match(/(.*)\*\//);
			if (m != null) {
			  // Found end of comment.
			  comment += m[1];
			  break;
			}
			// Strip leading whitespace and "*".
			comment += s.replace(/^\s*\*/, "");
			s = f.readLine();
		  } while (s != null);	

    	  if (debug)
          print("Found comment " + comment);

		  if (firstLine) {
			// We have a comment for the whole file.
			out.writeLine('<H1>File ' + fname + '</H1>');
			out.writeLine(processComment(comment,firstLine,fname));
			out.writeLine('<HR>');
			firstLine = false;
			comment = "";
			continue;
		  }
	  }
	  // match the beginning of the function
	  // NB we also match functions without a comment!
	  // if we have two comments one after another only the last one will be taken
	  m = s.match(/^\s*function\s+((\w+)|(\w+)(\s+))\(([^)]*)\)/);
	  if (m != null)
	  {
        // Found a function start
		var htmlText = processFunction(m[1], m[2], comment);

		// Save the text in a global variable, so we
		// can write out a table of contents first.
		functionDocArray[functionDocArray.length] = 
				{name:m[1], text:htmlText};
			
		// Store the function also in the indexFunctionArray 
		// so we can have a seperate file with the function table of contents
		if (indexFunctionArray[m[1]]) {
 			//  print("ERROR: function: " + m[1] + " is defined more than once!");
			// Allow multiple files for a function
			  with (indexFunctionArray[m[1]]) {
			    filename = filename + "|" + fname;
				// print("filename = " + filename);
			  }
		} else {
			  indexFunctionArray[m[1]] = 
					{filename:fname};
        }
		//reset comment 
		comment = "";
      }  
	  firstLine = false;
	}

	// Write table of contents.
	for (var i=0; i < functionDocArray.length; i++) {
		with (functionDocArray[i]) {
			out.writeLine('function <A HREF=#' + name +
				      '>' + name + '</A><BR>');
		}
	}
	out.writeLine('<HR>');

	// Now write the saved function documentation.
	for (i=0; i < functionDocArray.length; i++) {
		with (functionDocArray[i]) {
			out.writeLine('<A NAME=' + name + '>');
			out.writeLine(text);
		}
	}
	out.writeLine('</BODY></HTML>');

	// Now clean up the doc array
	functionDocArray = [];  
}

/**
 * Process function and associated comment.
 * @param name the name of the function
 * @param args the args of the function as a single string
 * @param comment the text of the comment
 * @return a string for the HTML text of the documentation
 */
function processFunction(name, args, comment) {
   if (debug)
    print("Processing " + name + " " + args + " " + comment);
	return "<H2>Function " + name + "</H2>" +
		"<PRE>" +
		"function " + name + "(" + args + ")" +
		"</PRE>" +
		processComment(comment,0,name) +
		"<P><BR><BR>";
}




/**
 * Process comment.
 * @param comment the text of the comment
 * @param firstLine shows if comment is at the beginning of the file
 * @param fname name of the file (without path)
 * @return a string for the HTML text of the documentation
 */
function processComment(comment,firstLine,fname) {
	var tags = {};
	// Use the "lambda" form of regular expression replace,
	// where the replacement object is a function rather 
	// than a string. The function is called with the 
	// matched text and any parenthetical matches as
	// arguments, and the result of the function used as the
	// replacement text. 
	// Here we use the function to build up the "tags" object,
	// which has a property for each "@" tag that is the name
	// of the tag, and whose value is an array of the
	// text following that tag.
	comment = comment.replace(/@(\w+)\s+([^@]*)/g,
				  function (s, name, text) {
					var a = tags[name] || [];
					a.push(text);
					tags[name] = a;
					return "";
				  });
				  
	// if we have a comment at the beginning of a file
	// store the comment for the index file 			  
	if (firstLine) {
	  indexFileArray[fname] = comment;
	}

	var out = comment + '<P>';
	if (tags["param"]) {
		// Create a table of parameters and their descriptions.
		var array = tags["param"];
		var params = "";
		for (var i=0; i < array.length; i++) {
			var m = array[i].match(/(\w+)\s+(.*)/);
			params += '<TR><TD><I>'+m[1]+'</I></TD>' + 
			          '<TD>'+m[2]+'</TD></TR>';
		}
		out += '<TABLE WIDTH="90%" BORDER=1>';
		out += '<TR BGCOLOR=0xdddddddd>';
		out += '<TD><B>Parameter</B></TD>';
		out += '<TD><B>Description</B></TD></TR>';
		out += params;
		out += '</TABLE><P>';
	}
	if (tags["return"]) {
		out += "<DT><B>Returns:</B><DD>";
		out += tags["return"][0] + "</DL><P>";
	}
	if (tags["author"]) {
		// List the authors together, separated by commas.
		out += '<DT><B>Author:</B><DD>';
		var array = tags["author"];
		for (var i=0; i < array.length; i++) {
			out += array[i];
			if (i+1 < array.length)
				out += ", ";
		}
		out += '</DL><P>';
	}
	if (tags["version"]) {
	    // Show the version.
	    out += '<DT><B>Version:</B><DD>';
	    var array = tags["version"];
	    for (var i=0; i < array.length; i++) {
		   out += array[i];
		   if (i+1 < array.length)
			   out += "<BR><DD>";
		}
		out += '</DL><P>';
	}
	if (tags["see"]) {
	    // List the see modules together, separated by <BR>.
	    out += '<DT><B>Dependencies:</B><DD>';
	    var array = tags["see"];
	    for (var i=0; i < array.length; i++) {
		   out += array[i];
		   if (i+1 < array.length)
			   out += "<BR><DD>";
		}
		out += '</DL><P>';
	}
	if (tags["lastmodified"]) {
	    // Shows a last modified description with client-side js.
	    out += '<DT><B>Last modified:</B><DD>';
		out += '<script><!--\n';
		out += 'document.writeln(document.lastModified);\n';
		out += '// ---></script>\n';		
		out += '</DL><P>';
	}

	// additional tags can be added here (i.e., "if (tags["see"])...")
	return out;
}

/**
 * Create an html output file
 * @param outputdir directory to put the file
 * @param htmlfile name of the file
*/ 
function CreateOutputFile(outputdir,htmlfile)
{ 	 
  if (outputdir==null)
  {
    var outname = htmlfile;
  }
  else
  {
    var separator = Packages.java.io.File.separator;
    var outname = outputdir + separator + htmlfile.substring(htmlfile.lastIndexOf(separator),htmlfile.length);
  }
  print("output file: " + outname);
  return new File(outname);
}

/**
 * Process a javascript file. Puts the generated HTML file in the outdir  
 * @param filename name of the javascript file
 * @inputdir input directory of the file (default null)
 */
function processJSFile(filename,inputdir) 
{
  if (debug) print("filename = " + filename + " inputdir = " + inputdir);

  if (!filename.match(/\.js$/)) { 
	print("Expected filename to end in '.js'; had instead " + 
	  filename + ". I don't treat the file.");
  } else { 
    if (inputdir==null)
	{
	  var inname = filename;
    }
	else
	{
      var separator = Packages.java.io.File.separator;
      var inname = inputdir + separator + filename;
    }
    print("Processing file " + inname);

	var f = new File(inname);
 	 
    // create the output file
    var htmlfile = filename.replace(/\.js$/, ".html");

	var out = CreateOutputFile(outputdir,htmlfile);
	
    processFile(f, filename, inputdir, out);
	out.close();
  }
}

/**
 * Generate index files containing links to the processed javascript files
 * and the generated functions
 */
function GenerateIndex(dirname)
{
  // construct the files index file
  var out = CreateOutputFile(outputdir,indexFile);
  
  // write the beginning of the file
  out.writeLine('<HTML><HEADER><TITLE>File Index - directory: ' + dirname + '</TITLE><BODY>');
  out.writeLine('<H1>File Index - directory: ' + dirname + '</H1>\n');
  out.writeLine('<TABLE WIDTH="90%" BORDER=1>');
  out.writeLine('<TR BGCOLOR=0xdddddddd>');
  out.writeLine('<TD><B>File</B></TD>');
  out.writeLine('<TD><B>Description</B></TD></TR>');

  var separator = Packages.java.io.File.separator;

  // sort the index file array
  var SortedFileArray = [];
  for (var fname in indexFileArray)
    SortedFileArray.push(fname);  
  SortedFileArray.sort();

  for (var i=0; i < SortedFileArray.length; i++) {
    var fname = SortedFileArray[i];
  	var htmlfile = fname.replace(/\.js$/, ".html");
    out.writeLine('<TR><TD><A HREF=\"' + htmlfile + '\">' + fname + '</A></TD></TD><TD>');
	if (indexFileArray[fname])
	  out.writeLine(indexFileArray[fname]);
	else
	  out.writeLine('No comments');
	out.writeLine('</TD></TR>\n');
  }
  out.writeLine('</TABLE></BODY></HTML>');
  out.close();
  
  // construct the functions index file
  var out = CreateOutputFile(outputdir,indexFunction);
    
  // write the beginning of the file
  out.writeLine('<HTML><HEADER><TITLE>Function Index - directory: ' + dirname + '</TITLE><BODY>');
  out.writeLine('<H1>Function Index - directory: ' + dirname + '</H1>\n');
  out.writeLine('<TABLE WIDTH="90%" BORDER=1>');
  out.writeLine('<TR BGCOLOR=0xdddddddd>');
  out.writeLine('<TD><B>Function</B></TD>');
  out.writeLine('<TD><B>Files</B></TD></TR>');
  
  // sort the function array
  var SortedFunctionArray = [];
  for (var functionname in indexFunctionArray)
    SortedFunctionArray.push(functionname);  
  SortedFunctionArray.sort();

  for (var j=0; j < SortedFunctionArray.length; j++) {
    var funcname = SortedFunctionArray[j];
    with (indexFunctionArray[funcname]) {
	 var outstr = '<TR><TD>' + funcname + '</TD><TD>';
	 var filelst = filename.split("|");
	 for (var i in filelst) {
	   var htmlfile = filelst[i].replace(/\.js$/, ".html");
	   outstr += '<A HREF=\"' + htmlfile + '#' + funcname + '\">' + filelst[i] + '</A>&nbsp;';
	 }
	 outstr += '</TD></TR>';
	 out.writeLine(outstr);
    }
  }
  out.writeLine('</TABLE></BODY></HTML>');
  out.close();
}


/**
 * prints the options for JSDoc
*/
function PrintOptions()
{
  print("You can use the following options:\n");
  print("-d: specify an output directory for the generated html files\n");
  print("-i: processes all files in an input directory (you can specify several directories)\n");
  quit();
}


// Main Script
// first read the arguments 
if (! arguments)
  PrintOptions();
  
for (var i=0; i < arguments.length; i++) {
  if (debug) print("argument: + \'" + arguments[i] + "\'");
  if (arguments[i].match(/^\-/)) {
   if (String(arguments[i])=="-d"){
    // output directory for the generated html files

    outputdir = String(arguments[i+1]);
	if (debug) print("outputdir: + \'" + outputdir + "\'");

    i++;  
   }
   else if (String(arguments[i])=="-i"){
    // process all files in an input directory

    DirList.push(String(arguments[i+1]));
if (debug) print("inputdir: + \'" + arguments[i+1] + "\'");
     i++;
   }
   else {
    print("Unknown option: " + arguments[i] + "\n");
	PrintOptions();
   }
  }
  else
  {
    // we have a single file
	if (debug) print("file: + \'" + arguments[i] + "\'");

	FileList.push(String(arguments[i]));
  }
}  
	
// first handle the single files
for (var i in FileList) 
  processJSFile(FileList[i],null);

// then handle the input directories
for (var j in DirList) {
  var inputdir = String(DirList[j]);
  
  print("Process input directory: " + inputdir);

  // clean up index arrays
  var indexFileArray = [];
  var indexFunctionArray = [];

  // for the directory name get rid of ../../ or ..\..\
  inputDirName = inputdir.replace(/\.\.\/|\.\.\\/g,"");
  
  indexFile = indexFileName + "_" + inputDirName + ".html";
  indexFunction = indexFunctionName + "_" + inputDirName + ".html";
  
print("indexFile = " + indexFile);
print("indexFunction = " + indexFunction);
	
  // read the files in the directory
  var DirFile = new java.io.File(inputdir);
  var lst = DirFile.list();
  var separator = Packages.java.io.File.separator;

  for (var i=0; i < lst.length; i++)
  {
    processJSFile(String(lst[i]),inputdir);
  }  

  // generate the index files for the input directory
  GenerateIndex(inputDirName);
}



