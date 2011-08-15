/*
Copyright (c) 2001-2005 World Wide Web Consortium,
(Massachusetts Institute of Technology, Institut National de
Recherche en Informatique et en Automatique, Keio University). All
Rights Reserved. This program is distributed under the W3C's Software
Intellectual Property License. This program is distributed in the
hope that it will be useful, but WITHOUT ANY WARRANTY; without even
the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
PURPOSE.
See W3C License http://www.w3.org/Consortium/Legal/ for more details.
*/

function assertNull(descr, actual) {
  return ok(actual === null, descr);
}


function assertNotNull(descr, actual) {
  return ok(actual !== null, descr);
}

function assertTrue(descr, actual) {
  return ok(actual === true, descr);
}

function assertFalse(descr, actual) {
  return ok(actual === false, descr);
}

function assertEquals(descr, expected, actual) {
  return is(actual, expected, descr);
}

  function assertSize(descr, expected, actual) {
    var actualSize;
    ok(actual !== null, descr);
    actualSize = actual.length;
    is(actualSize, expected, descr);
  }

  function assertEqualsAutoCase(context, descr, expected, actual) {
  	if (builder.contentType == "text/html") {
  	    if(context == "attribute") {
  	    	is(actual.toLowerCase(), expected.toLowerCase(), descr);
  	    } else {
  	        is(actual, expected.toUpperCase(), descr);
  	    }
  	} else {
  		is(actual, expected, descr); 
  	}
  }
  

  function assertEqualsCollectionAutoCase(context, descr, expected, actual) {
    //
    //  if they aren't the same size, they aren't equal
    is(actual.length, expected.length, descr);
    
    //
    //  if there length is the same, then every entry in the expected list
    //     must appear once and only once in the actual list
    var expectedLen = expected.length;
    var expectedValue;
    var actualLen = actual.length;
    var i;
    var j;
    var matches;
    for(i = 0; i < expectedLen; i++) {
        matches = 0;
        expectedValue = expected[i];
        for(j = 0; j < actualLen; j++) {
        	if (builder.contentType == "text/html") {
        		if (context == "attribute") {
        			if (expectedValue.toLowerCase() == actual[j].toLowerCase()) {
        				matches++;
        			}
        		} else {
        			if (expectedValue.toUpperCase() == actual[j]) {
        				matches++;
        			}
        		}
        	} else {
            	if(expectedValue == actual[j]) {
                	matches++;
                }
            }
        }
        if(matches == 0) {
            ok(false, descr + ": No match found for " + expectedValue);
        }
        if(matches > 1) {
            ok(false, descr + ": Multiple matches found for " + expectedValue);
        }
    }
  }

  function assertEqualsCollection(descr, expected, actual) {
    //
    //  if they aren't the same size, they aren't equal
    is(actual.length, expected.length, descr);
    //
    //  if there length is the same, then every entry in the expected list
    //     must appear once and only once in the actual list
    var expectedLen = expected.length;
    var expectedValue;
    var actualLen = actual.length;
    var i;
    var j;
    var matches;
    for(i = 0; i < expectedLen; i++) {
        matches = 0;
        expectedValue = expected[i];
        for(j = 0; j < actualLen; j++) {
            if(expectedValue == actual[j]) {
                matches++;
            }
        }
        if(matches == 0) {
            ok(false, descr + ": No match found for " + expectedValue);
        }
        if(matches > 1) {
            ok(false, descr + ": Multiple matches found for " + expectedValue);
        }
    }
  }


  function assertEqualsListAutoCase(context, descr, expected, actual) {
	var minLength = expected.length;
	if (actual.length < minLength) {
	    minLength = actual.length;
	}
    //
    for(var i = 0; i < minLength; i++) {
		assertEqualsAutoCase(context, descr, expected[i], actual[i]);
    }
    //
    //  if they aren't the same size, they aren't equal
    is(actual.length, expected.length, descr);
  }


  function assertEqualsList(descr, expected, actual) {
	var minLength = expected.length;
	if (actual.length < minLength) {
	    minLength = actual.length;
	}
    //
    for(var i = 0; i < minLength; i++) {
        if(expected[i] != actual[i]) {
			    is(actual[i], expected[i], descr);
        }
    }
    //
    //  if they aren't the same size, they aren't equal
    is(actual.length, expected.length, descr);
  }

  function assertInstanceOf(descr, type, obj) {
    if(type == "Attr") {
        is(2, obj.nodeType, descr);
        var specd = obj.specified;
    }
  }

  function assertSame(descr, expected, actual) {
    if(expected != actual) {
        is(expected.nodeType, actual.nodeType, descr);
        is(expected.nodeValue, actual.nodeValue, descr);
    }
  }

  function assertURIEquals(assertID, scheme, path, host, file, name, query, fragment, isAbsolute, actual) {
    //
    //  URI must be non-null
    ok(assertID && actual);

    var uri = actual;

    var lastPound = actual.lastIndexOf("#");
    var actualFragment = "";
    if(lastPound != -1) {
        //
        //   substring before pound
        //
        uri = actual.substring(0,lastPound);
        actualFragment = actual.substring(lastPound+1);
    }
    if(fragment != null) is(actualFragment, fragment, assertID);

    var lastQuestion = uri.lastIndexOf("?");
    var actualQuery = "";
    if(lastQuestion != -1) {
        //
        //   substring before pound
        //
        uri = actual.substring(0,lastQuestion);
        actualQuery = actual.substring(lastQuestion+1);
    }
    if(query != null) is(actualQuery, query, assertID);

    var firstColon = uri.indexOf(":");
    var firstSlash = uri.indexOf("/");
    var actualPath = uri;
    var actualScheme = "";
    if(firstColon != -1 && firstColon < firstSlash) {
        actualScheme = uri.substring(0,firstColon);
        actualPath = uri.substring(firstColon + 1);
    }

    if(scheme != null) {
        is(scheme, actualScheme, assertID);
    }

    if(path != null) {
        is(path, actualPath, assertID);
    }

    if(host != null) {
        var actualHost = "";
        if(actualPath.substring(0,2) == "//") {
            var termSlash = actualPath.substring(2).indexOf("/") + 2;
            actualHost = actualPath.substring(0,termSlash);
        }
        is(actualHost, host, assertID);
    }

    if(file != null || name != null) {
        var actualFile = actualPath;
        var finalSlash = actualPath.lastIndexOf("/");
        if(finalSlash != -1) {
            actualFile = actualPath.substring(finalSlash+1);
        }
        if (file != null) {
            is(actualFile, file, assertID);
        }
        if (name != null) {
            var actualName = actualFile;
            var finalDot = actualFile.lastIndexOf(".");
            if (finalDot != -1) {
                actualName = actualName.substring(0, finalDot);
            }
            is(actualName, name, assertID);
        }
    }

    if(isAbsolute != null) {
        is(actualPath.substring(0,1) == "/", isAbsolute, assertID);
    }
  }


// size() used by assertSize element
function size(collection)
{
  return collection.length;
}

function same(expected, actual)
{
  return expected === actual;
}

function getSuffix(contentType) {
    switch(contentType) {
        case "text/html":
        return ".html";

        case "text/xml":
        return ".xml";

        case "application/xhtml+xml":
        return ".xhtml";

        case "image/svg+xml":
        return ".svg";

        case "text/mathml":
        return ".mml";
    }
    return ".html";
}

function equalsAutoCase(context, expected, actual) {
	if (builder.contentType == "text/html") {
		if (context == "attribute") {
			return expected.toLowerCase() == actual;
		}
		return expected.toUpperCase() == actual;
	}
	return expected == actual;
}

function catchInitializationError(blder, ex) {
       if (blder == null) {
          alert(ex);
       } else {
	      blder.initializationError = ex;
	      blder.initializationFatalError = ex;
	   }
}

function checkInitialization(blder, testname) {
    if (blder.initializationError != null) {
        if (blder.skipIncompatibleTests) {
        	warn(testname + " not run:" + blder.initializationError);
        	return blder.initializationError;
        } else {
        	//
        	//   if an exception was thrown
        	//        rethrow it and do not run the test
            if (blder.initializationFatalError != null) {
        		throw blder.initializationFatalError;
        	} else {
        		//
        		//   might be recoverable, warn but continue the test
        		// XXX warn
        		warn(testname + ": " +  blder.initializationError);
        	}
        }
    }
    return null;
}
function createTempURI(scheme) {
   if (scheme == "http") {
   	  return "http://localhost:8080/webdav/tmp" + Math.floor(Math.random() * 100000) + ".xml";
   }
   return "file:///tmp/domts" + Math.floor(Math.random() * 100000) + ".xml";
}


function EventMonitor() {
  this.atEvents = new Array();
  this.bubbledEvents = new Array();
  this.capturedEvents = new Array();
  this.allEvents = new Array();
}

EventMonitor.prototype.handleEvent = function(evt) {
    switch(evt.eventPhase) {
       case 1:
       monitor.capturedEvents[monitor.capturedEvents.length] = evt;
       break;
       
       case 2:
       monitor.atEvents[monitor.atEvents.length] = evt;
       break;

       case 3:
       monitor.bubbledEvents[monitor.bubbledEvents.length] = evt;
       break;
    }
    monitor.allEvents[monitor.allEvents.length] = evt;
}

function DOMErrorImpl(err) {
  this.severity = err.severity;
  this.message = err.message;
  this.type = err.type;
  this.relatedException = err.relatedException;
  this.relatedData = err.relatedData;
  this.location = err.location;
}



function DOMErrorMonitor() {
  this.allErrors = new Array();
}

DOMErrorMonitor.prototype.handleError = function(err) {
    errorMonitor.allErrors[errorMonitor.allErrors.length] = new DOMErrorImpl(err);
}

DOMErrorMonitor.prototype.assertLowerSeverity = function(id, severity) {
    var i;
    for (i = 0; i < errorMonitor.allErrors.length; i++) {
        if (errorMonitor.allErrors[i].severity >= severity) {
           assertEquals(id, severity - 1, errorMonitor.allErrors[i].severity);
        }
    }
}

function UserDataNotification(operation, key, data, src, dst) {
    this.operation = operation;
    this.key = key;
    this.data = data;
    this.src = src;
    this.dst = dst;
}

function UserDataMonitor() {
	this.allNotifications = new Array();
}

UserDataMonitor.prototype.handle = function(operation, key, data, src, dst) {
    userDataMonitor.allNotifications[this.allNotifications.length] =
         new UserDataNotification(operation, key, data, src, dst);
}



function IFrameBuilder() {
    this.contentType = "text/html";
    this.supportedContentTypes = [ "text/html", 
        "text/xml",
        "image/svg+xml",
        "application/xhtml+xml" ];    

    this.supportsAsyncChange = false;
    this.async = true;
    this.fixedAttributeNames = [
        "validating",  "expandEntityReferences", "coalescing", 
        "signed", "hasNullString", "ignoringElementContentWhitespace", "namespaceAware", "ignoringComments", "schemaValidating"];

    this.fixedAttributeValues = [false,  true, false, true, true , false, false, true, false ];
    this.configurableAttributeNames = [ ];
    this.configurableAttributeValues = [ ];
    this.initializationError = null;
    this.initializationFatalError = null;
    this.skipIncompatibleTests = false;
}

IFrameBuilder.prototype.hasFeature = function(feature, version) {
    return document.implementation.hasFeature(feature, version);
}

IFrameBuilder.prototype.getImplementation = function() {
    return document.implementation;
}

IFrameBuilder.prototype.setContentType = function(contentType) {
    this.contentType = contentType;
    if (contentType == "text/html") {
        this.fixedAttributeValues[6] = false;
    } else {
        this.fixedAttributeValues[6] = true;
    }
}



IFrameBuilder.prototype.preload = function(frame, varname, url) {
  var suffix;
  if (this.contentType == "text/html" || 
      this.contentType == "application/xhtml+xml") {
      if (url.substring(0,5) == "staff" || url == "nodtdstaff" ||
	  url == "datatype_normalization") { 
  	  suffix = ".xml";
      }  
  }
  
  if (!suffix) suffix = getSuffix(this.contentType);
  
  var iframe = document.createElement("iframe");
  var srcname = url + suffix;
  iframe.setAttribute("name", srcname);
  iframe.setAttribute("src", fileBase + srcname);
  //
  //   HTML and XHTML have onload attributes that will invoke loadComplete
  //       
  if (suffix.indexOf("html") < 0) { 
     iframe.addEventListener("load", loadComplete, false);       
  }
  document.getElementsByTagName("body").item(0).appendChild(iframe);
  return 0; 
}

IFrameBuilder.prototype.load = function(frame, varname, url) {
  	var suffix;
  	if (url.substring(0,5) == "staff" || url == "nodtdstaff" || url == "datatype_normalization") { 
  	  suffix = ".xml";
  	}
  	if (!suffix) suffix = getSuffix(this.contentType);
    var name = url + suffix;
    var iframes = document.getElementsByTagName("iframe");
    for(var i = 0; i < iframes.length; i++) {
       if (iframes.item(i).getAttribute("name") == name) {
           var item = iframes.item(i);
           if (typeof(item.contentDocument) != 'undefined') {
               return item.contentDocument;
           }
           if (typeof(item.document) != 'undefined') {
           	   return item.document;
           }
           return null;
       }
    }
    return null;
}

IFrameBuilder.prototype.getImplementationAttribute = function(attr) {
    for (var i = 0; i < this.fixedAttributeNames.length; i++) {
        if (this.fixedAttributeNames[i] == attr) {
            return this.fixedAttributeValues[i];
        }
    }
    throw "Unrecognized implementation attribute: " + attr;
}



IFrameBuilder.prototype.setImplementationAttribute = function(attribute, value) {
    var supported = this.getImplementationAttribute(attribute);
    if (supported != value) {
        this.initializationError = "IFrame loader does not support " + attribute + "=" + value;
    }
}


IFrameBuilder.prototype.canSetImplementationAttribute = function(attribute, value) {
    var supported = this.getImplementationAttribute(attribute);
    return (supported == value);
}


function createBuilder(implementation) {
  if (implementation == null) {
  	return new IFrameBuilder();
  }
  switch(implementation) {
/*    case "msxml3":
    return new MSXMLBuilder("Msxml2.DOMDocument.3.0");
    
    case "msxml4":
    return new MSXMLBuilder("Msxml2.DOMDocument.4.0");*/

    case "mozillaXML":
    return new MozillaXMLBuilder();
/*
    case "svgplugin":
    return new SVGPluginBuilder();

    case "dom3ls":
    return new DOM3LSBuilder(); */
    
    case "iframe":
    return new IFrameBuilder();

    case "xmlhttprequest":
    return new XMLHttpRequestBuilder();
    
    default:
    alert ("unrecognized implementation " + implementation);
  }
  return new IFrameBuilder();
}

function checkFeature(feature, version)
{
  if (!builder.hasFeature(feature, version))
  {
    //
    //   don't throw exception so that users can select to ignore the precondition
    //
    builder.initializationError = "builder does not support feature " + feature + " version " + version;
  }
}

function createConfiguredBuilder() {
	var builder = null;
	var contentType = null;
	var i;
	var contentTypeSet = false;
	var parm = null;
	builder = new IFrameBuilder();
	return builder;
}


function preload(frame, varname, url) {
  return builder.preload(frame, varname, url);
}

function load(frame, varname, url) {
    return builder.load(frame, varname, url);
}

function getImplementationAttribute(attr) {
    return builder.getImplementationAttribute(attr);
}


function setImplementationAttribute(attribute, value) {
    builder.setImplementationAttribute(attribute, value);
}

function setAsynchronous(value) {
    if (builder.supportsAsyncChange) {
      builder.async = value;
    } else {
      update();
    }
}


function createXPathEvaluator(doc) {
    try {
        return doc.getFeature("XPath", null);
    }
    catch(ex) {
    }
    return doc;
}

function toLowerArray(src) {
   var newArray = new Array();
   var i;
   for (i = 0; i < src.length; i++) {
      newArray[i] = src[i].toLowerCase();
   }
   return newArray;
}

function MSXMLBuilder_onreadystatechange() {
    if (builder.parser.readyState == 4) {
        loadComplete();
    }
}



var fileBase = location.href;
if (fileBase.indexOf('?') != -1) {
   fileBase = fileBase.substring(0, fileBase.indexOf('?'));
}
fileBase = fileBase.substring(0, fileBase.lastIndexOf('/') + 1) + "files/";

function getResourceURI(name, scheme, contentType) {
    return fileBase + name + getSuffix(contentType);
}


function getImplementation() {
    return builder.getImplementation();
}

//sayrer override the SimpleTest logger
SimpleTest._logResult = function(test, passString, failString) {
  var msg = test.result ? passString : failString;
  msg += " | " + test.name;
  if (test.result) {
      if (test.todo)
          parentRunner.logger.error(msg)
      else
          parentRunner.logger.log(msg);
  } else {
      msg += " | " + test.diag;
      if (test.todo) {
        parentRunner.logger.log(msg)
      } else {
	// if (todoTests[docName]) {
        //  parentRunner.logger.log("expected error in todo testcase | " + test.name);
        //} else {
          parentRunner.logger.error(msg);
	  //}
      } 
  }
}

window.doc = window;  
SimpleTest.waitForExplicitFinish();
addLoadEvent(function(){ setUpPage(); });
function testFails (test) {
  if (!test.result) {
    test.todo = true;
    return true;
  }
  return false;
}

function markTodos() {
  if (todoTests[docName]) {
    // mark the failures as todos
    var tests = SimpleTest._tests;
    var failures = [];
    var o;
    for (var i = 0; i < tests.length; i++) {
      o = tests[i];
      if (testFails(o)) {
        failures.push(o);
      } 
    }
    // shouldn't be 0 failures
    todo(SimpleTest._tests != 0 && failures == 0, "test marked todo should fail somewhere");
  }
}

function runJSUnitTests() {
  builder = createConfiguredBuilder();
  try {
    var tests = exposeTestFunctionNames(); 
    for (var i = 0; i < tests.length; i++) {
      window[tests[i]](); 
   }   
  } catch (ex) {
    //if (todoTests[docName]) {
    //  todo(false, "Text threw exception: " + ex);
    //} else { 
      ok(false, "Test threw exception: " + ex);
    //}
  }
}
