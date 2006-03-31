/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
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
 * The Original Code is Mozilla JavaScript Shell project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

//**********************************************************************
// a startup script with some debugging functions for JSSh

//**********************************************************************
// Debugger interface
// if (!debugservice)
//   var debugservice = Components.classes["@mozilla.org/js/jsd/debugger-service;1"]
//                                .getService(Components.interfaces.jsdIDebuggerService);

//**********************************************************************
// General purpose commands

//----------------------------------------------------------------------
var _help_strings = {};

function help(func) {
  if (!func) {
    print("Usage: help(command);\n");
    print("Available commands are:\n");
    for (var i in _help_strings) print(i+" ");
    return;
  }
  if (typeof func == "function") {
    func = func.name;
  }
  var str = _help_strings[func];
  if (!str) {
    print("unknown command! Try help();\n");
    return;
  }
  print(str+"\n");
}

//----------------------------------------------------------------------
_help_strings.inspect = "inspect(obj) dumps information about obj";
function inspect(obj) {
  if(!obj) {
    dump(obj);
    return;
  }
  print("* Object type: "+typeof obj+"\n");
  if (typeof obj != "xml") {
    print("* Parent chain: ");
    try {
      var parent = obj.__parent__;
      while(parent) {
        try {
          print(parent);
        }
        catch(e) { dump("(?)"); }
        finally { dump(" "); }
        parent = parent.__parent__;
      }
    } catch(e) { dump("...(?)"); }
    print("\n");
  }
  switch(typeof obj) {
    case "object":
      print("* Prototype chain: ");
      try {
        var proto = obj.__proto__;
        while (proto) {
          try {
            print(proto);
          } catch(e) {
            // obj.__proto__.toString threw an exception!
            print("[proto.toString = "+obj.__proto__.toString+"]");
          }
          finally { dump(" "); }
          proto = proto.__proto__;
        }
      } catch(e) {dump("...(?)");}
      print("\n");
      print("* String value: "+ obj+"\n");
      print("* Members: ");
      for (var i in obj) {
        try {
          dump(i);
        }
        catch(e) { dump("(?)"); }
        finally { dump(" "); }
// xxx sometimes make moz hang. e.g. for inspect(debugservice)
//        try{
//          dump("("+(typeof obj[i])[0]+") ");
//        }
//        catch(e) { dump("(?) "); }
      }
      print("\n");
      break;
    case "function":
      print("* Arity: "+obj.length+"\n");
      print("* Function definition: ");
      print(obj);
      break;
    case "xml":
      print(obj.toXMLString()+"\n");
      break;
    default:
      print("* String value: "+ obj +"\n");
      break;
  }
}

//----------------------------------------------------------------------
_help_strings.subobjs = "subobjs(obj, type) list all subobjects of object obj that have type 'type' (default='object')";
function subobjs(obj, type) {
  if (!type) type="object";
  for (var o in obj) {
    try {
      if (typeof obj[o]==type)
        print(o+" ");
    }
    catch(e) {
      print(o+"(?) ");
    }
  }
  print("\n");
}

//----------------------------------------------------------------------
_help_strings.dumpIDL = "dumpIDL(iid) dumps idl code for interface iid.\niid can either be an interface, or a string of form 'Components.interfaces.nsIFoo' or just 'nsIFoo'";
function dumpIDL(iid) {
  var gen = Components.classes["@mozilla.org/interfaceinfotoidl;1"]
                      .createInstance(Components.interfaces.nsIInterfaceInfoToIDL);
  if (typeof iid == "string") {
    if (!/Components.interfaces./.test(iid)) {
      iid = "Components.interfaces."+iid;
    }
    iid = eval(iid);
  }
  dump(gen.generateIDL(iid, false, false));
}

//----------------------------------------------------------------------
_help_strings.getWindows = "getWindows() returns a list of all currently open windows";
function getWindows() {
  var wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                     .getService(Components.interfaces.nsIWindowMediator);
  var enumerator = wm.getEnumerator(null);
  var retval = [];
  while (enumerator.hasMoreElements())
    retval.push(enumerator.getNext());
  return retval;
}

//----------------------------------------------------------------------
_help_strings.findClass = "findClass(/regex/) finds registered classes matching /regex/";
function findClass(regex) {
  for (var c in Components.classes)
    if (regex.test(c)) print(c+" ");
}

//----------------------------------------------------------------------
_help_strings.findInterface = "findInterface(/regex/) finds registered interfaces matching /regex/";
function findInterface(regex) {
  for (var i in Components.interfaces)
    if (regex.test(i)) print(i+" ");
}

//----------------------------------------------------------------------
_help_strings.dumpStack = "dumpStack(offset, max_depth) displays a stack trace up to depth max_depth (default: 10) starting 'offset' frames below the current one";
function dumpStack(offset, max_depth) {
  if (!offset || offset<0) offset = 0;
  if (!max_depth) max_depth = 10;
  var frame = Components.stack;
  while(--max_depth && (frame=frame.caller)) {
    if (!offset)
      dump(frame+"\n");
    else
      --offset;
  }
}

//----------------------------------------------------------------------
_help_strings.jssh = "jssh(startupURI) runs a blocking interactive javascript shell";
function jssh(startupURI) {
 Components.classes["@mozilla.org/jssh-server;1"]
   .getService(Components.interfaces.nsIJSShServer)
   .runShell(getInputStream(), getOutputStream(), startupURI, true);
}

//----------------------------------------------------------------------
_help_strings.hex = "hex(str, octets_per_line) returns the hexadecimal representation of the given string. octets_per_line is optional and defaults to 4.";
function hex(str, octets_per_line) {
  var rv = "";
  if (!octets_per_line) octets_per_line = 4;
  for (var i=0, l=str.length; i<l; ++i) {
    var code = str.charCodeAt(i).toString(16);
    if (code.length == 1) code = "0"+code;
    rv += code + " ";
    if (i % octets_per_line == octets_per_line-1) rv += "\n";
  }
  return rv;
}

//**********************************************************************
// Function instrumentation:

// global hash to keep track of instrumented objs/funcs
if (!_instrumented_objects)
  var _instrumented_objects = {};

//----------------------------------------------------------------------
_help_strings.getInstrumented = "getInstrumented(obj, func) returns an object with instrumentation data for the given (obj, func). It can be used to change instrumentation functions or operation-flags for the instrumentation functions . Members include \n'old': old method that will be called.\n'before', 'after': before and after methods\nThe default before and after methods recognise the flags (set by defining/unset by deleting):\n'stack' : show stack trace\n'nargs' : don't show args\n'nresult' : don't show result/exception status on exiting.";
function getInstrumented(obj, func) {
  return _instrumented_objects[obj][func];
}

//----------------------------------------------------------------------
_help_strings.instrument = "instrument(obj, func, beforeHook, afterHook) instruments func on obj. obj and func need to be strings. Optional beforeHook and afterHook are the methods called before and after the instrumented method is being called. Signature for before hook functions is hook(obj, func, argv). Signature for after hook functions is hook(obj, func, argv, retval, exception). Return value of after hook will be taken as return value for function call";

// default hooks for instrumented functions:
function _defaultBeforeHook(obj, func, argv) {
  print("* Entering "+obj+"."+func+"\n");
  if (getInstrumented(obj,func)["nargs"]===undefined) {
    print("* Arguments("+argv.length+"): " );
    for (var i=0;i<argv.length;++i) print("'"+argv[i]+"' ");
    print("\n");
  }
  if (getInstrumented(obj,func)["stack"]!==undefined) {
    print("* Stack: \n");
    dumpStack(2);
  }
}

function _defaultAfterHook(obj, func, argv, retval, ex) {
  print("* Exiting "+obj+"."+func+"\n");
  if (getInstrumented(obj,func)["nresult"]===undefined){
    print("* Result: ");
    if (!ex)
      print(retval+"\n");
    else
      print("An exception was thrown!\n");
  }
  return retval;
}

function instrument(obj, func, beforeHook, afterHook) {
  if (typeof obj!="string"||typeof func!="string") {
    help("instrument");
    return;
  }
  if (!_instrumented_objects[obj])
    _instrumented_objects[obj] = {};
  if (_instrumented_objects[obj][func]) {
    print("obj.function is already instrumented!");
    return;
  }

  if (!beforeHook) beforeHook = _defaultBeforeHook;
  if (!afterHook) afterHook = _defaultAfterHook;
  
  _instrumented_objects[obj][func] = {};
  _instrumented_objects[obj][func]["old"] = eval(obj)[func];
  _instrumented_objects[obj][func]["before"] = beforeHook;
  _instrumented_objects[obj][func]["after"] = afterHook;
  eval(obj)[func] = function() {
    _instrumented_objects[obj][func]["before"](obj, func, arguments);
    var retval;
    var ex;
    try {
      retval = _instrumented_objects[obj][func]["old"].apply(this, arguments);
    }
    catch(e) {
      ex = e;
      throw (e);
    }
    finally {
      retval = _instrumented_objects[obj][func]["after"](obj, func, arguments, retval, ex);
    }
    return retval;
  }
}

//----------------------------------------------------------------------
_help_strings.dumpInstrumented = "dumpInstrumented() prints a list of all currently instrumented functions.";
function dumpInstrumented() {
  for (var obj in _instrumented_objects) {
    for (var method in _instrumented_objects[obj]) {
      dump(obj+"."+method+" ");
    }
  }
  print("\n");
}


//----------------------------------------------------------------------
_help_strings.uninstrument = "uninstrument(obj, func) uninstruments func on obj. obj and func need to be strings.";
function uninstrument(obj, func) {
  if (typeof obj!="string"||typeof func!="string") {
    help("uninstrument");
    return;
  }
  if (!_instrumented_objects[obj] || !_instrumented_objects[obj][func]) {
    print("obj.function is not instrumented!");
    return;
  }
  eval(obj)[func] = _instrumented_objects[obj][func]["old"];
  delete _instrumented_objects[obj][func];
}

//**********************************************************************
// DOM utils:

//----------------------------------------------------------------------
_help_strings.domNode = "domNode(base, child1, child2, ...) returns the dom node identified by base and the child offsets. The child offset can be of form 'a5' to identify the anonymous child at position 5.";
function domNode(base) {
  var node = base;
  for (var i=1; i<arguments.length; ++i) {
    if (typeof arguments[i] == "number") {
      node = node.childNodes[arguments[i]];
    }
    else if(arguments[i][0]=='a') {
      var idx = arguments[i].substring(1, arguments[i].length);
      node = node.ownerDocument.getAnonymousNodes(node)[idx];
    }
  }
  return node;
}
      

//----------------------------------------------------------------------
_help_strings.domDumpFull = "domDumpFull(node) dumps the *complete* DOM starting from node.";
function domDumpFull(node) {
  var serializer = Components.classes["@mozilla.org/xmlextras/xmlserializer;1"]
                             .getService(Components.interfaces.nsIDOMSerializer);
  print(serializer.serializeToString(node));
}

//----------------------------------------------------------------------
_help_strings.domDump = "domDump(node) dumps the DOM around 'node'";
function domDump(node){
  var wantChildren = false;
  var wantAnonChildren = false;
  switch(node.nodeType) {
    case Components.interfaces.nsIDOMNode.DOCUMENT_NODE:
      print("<#document>\n");
      wantChildren = true;
      break;
    case Components.interfaces.nsIDOMNode.ELEMENT_NODE:
      print("* Parent: <"+node.parentNode.nodeName+">\n");
      print("<"+node.nodeName);
      for (var i=0; i<node.attributes.length;++i) {
        var attr = node.attributes.item(i);
        print(" "+attr.nodeName+"='"+attr.nodeValue+"'");
      }
      print(">\n");
      wantChildren = true;
      wantAnonChildren = true;
      break;
    default:
      break;
  }
  if (wantChildren && node.hasChildNodes()) {
    print("* Children: \n");
    for (var i=0; i<node.childNodes.length;++i) {
      print(" ["+i+"]: <"+node.childNodes[i].nodeName+">\n");
    }
  }
  if (wantAnonChildren) {
    var anlist = node.ownerDocument.getAnonymousNodes(node);
    if (anlist && anlist.length) {
      print("* Anonymous Children: \n");
      for (var i=0; i<anlist.length;++i) {
        print(" A["+i+"]: <"+anlist[i].nodeName+">\n");
      }
    }
  }
}

//**********************************************************************
// RDF utils:

//----------------------------------------------------------------------
_help_strings.rdfDump = "rdfDump(url) dumps the rdf datasource at 'url' in the following form:\n\n"+
                        " <Resource> \n"+
                        "  |\n"+
                        "  |--[Resource]--> <Resource>\n"+
                        "  |\n"+
                        "  |--[Resource]--> {literal}\n\n"+
                        "If 'url' points to a remote datasource, it will be refreshed before dumping.";
function rdfDump(url) {
  var RDFService = Components.classes["@mozilla.org/rdf/rdf-service;1"]
    .getService(Components.interfaces.nsIRDFService);
  var ds = RDFService.GetDataSourceBlocking(url);

  // make sure the ds is up-to-date:
  try {
    var remote = ds.QueryInterface(Components.interfaces.nsIRDFRemoteDataSource);
    remote.Refresh(true);
  }
  catch(e) {
    // not a remote ds.
  }
  
  const RESOURCE_IID = Components.interfaces.nsIRDFResource;
  const LITERAL_IID = Components.interfaces.nsIRDFLiteral;
  const DATE_IID = Components.interfaces.nsIRDFDate;
  const INT_IID = Components.interfaces.nsIRDFInt;
  const BLOB_IID = Components.interfaces.nsIRDFBlob;
  
  var resources = ds.GetAllResources();
  while (resources.hasMoreElements()) {
    var resource = resources.getNext().QueryInterface(RESOURCE_IID);
    print("\n<" + resource.Value + ">\n");
    var arcs = ds.ArcLabelsOut(resource);
    while (arcs.hasMoreElements()) {
      var arc = arcs.getNext().QueryInterface(RESOURCE_IID);
      print(" |\n");
      print(" |--["+arc.Value +"]--> ");
      var targets = ds.GetTargets(resource,arc,true);
      while (targets.hasMoreElements()) {
        var target = targets.getNext();
        // target can be resource or literal string, date, int or blob:
        try {
          var target_as_resource = target.QueryInterface(RESOURCE_IID);
          print("<"+target_as_resource.Value+"> ");
          continue;
        } catch(e) {}
        try {
          var target_as_literal = target.QueryInterface(LITERAL_IID);
          print("{String=\""+target_as_literal.Value+"\"} ");
          continue;
        } catch(e) {}
        try {
          var target_as_date = target.QueryInterface(DATE_IID);
          print("{Date="+target_as_date.Value+"} ");
          continue;
        } catch(e) {}
        try {
          var target_as_int = target.QueryInterface(INT_IID);
          print("{Int="+target_as_int.Value+"} ");
          continue;
        } catch(e) {}
        try {
          var target_as_blob = target.QueryInterface(INT_BLOB);
          print("{Blob with length="+target_as_blob.length+"} ");
          continue;
        } catch(e) {}
      }
      print("\n");
    }
  }
}

//**********************************************************************
// Support for 'live' development:
//
// pushContext(obj) inserts the JSSh object into the prototype chain
// of obj and set obj as the current context object. This then gives
// the appearance as if obj is the current 'global' object. So
// e.g. variables that are set with 'var foo = ...' will end up in
// obj. Function calls 'bar()' map to 'obj->bar()'.
//
// XXX JSSh is inserted at the top of the prototype chain, between obj
// and its old prototype. This is so that we don't get JSSh functions
// appearing in *all* objects. (If we inserted JSSh at the bottom of
// the proto chain, its functions would be "inherited" by all objects
// that are rooted in the same object). The problem with this scheme
// is that JSSh can shadow certain functions, most notably
// QueryInterface, when JSSh is inserted between a wrapped native and
// its prototype.

// global context stack
if (!_context_objects)
  var _context_objects = [{obj:this,proto:this.__proto__}];

//----------------------------------------------------------------------
_help_strings.pushContext = "pushContext(obj)	installs the jssh context into the proto chain of 'obj' and sets 'obj' as the current context object";
function pushContext(obj) {
  var context = {"obj":obj, "proto":obj.__proto__};
  
  // sandwich jssh object between obj and its prototype:
  _context_objects[0].obj.__proto__ = obj.__proto__;
  obj.__proto__ = _context_objects[0].obj;

  // push context and set new context object:
  _context_objects.push(context);
  setContextObj(obj);
}

//----------------------------------------------------------------------
_help_strings.popContext = "popContext() restores the context object set before the last call to pushContext(.)";
function popContext() {
  if (_context_objects.length<2) {
    print("Context stack empty\n");
    return;
  }

  var context = _context_objects.pop();
  // remove jssh obj from proto chain:
  context.obj.__proto__ = context.proto;

  // restore jssh object's prototype:
  _context_objects[0].obj.__proto__ = _context_objects[_context_objects.length-1].proto;
  
  // switch to previous context:
  setContextObj(_context_objects[_context_objects.length-1].obj);
}

// override global 'exit()' function to pop the context stack:
exit = (function() {
  var _old_exit = exit;
  return function() {
    try {
      while(_context_objects.length>1)
        popContext();
    } catch(e) {}
    _old_exit();
  }})();

//----------------------------------------------------------------------
_help_strings.reloadXBLDocument = "reloadXBLDocument(url) reloads the xbl file 'url'. If defined, reloadXBLDocumentHook(url) is called before reloading.";
function reloadXBLDocument(url){
  try {
    reloadXBLDocumentHook(url);
  }catch(e){}

  var chromeCache = Components.classes["@mozilla.org/xul/xul-prototype-cache;1"].getService(Components.interfaces.nsIChromeCache);

  // Clear and reload the chrome file:
  chromeCache.flushXBLDocument(url);
  
  getWindows()[0].document.loadBindingDocument(url);
}

//----------------------------------------------------------------------
_help_strings.reloadXULDocument = "reloadXULDocument(url) reloads the xul file 'url'. If defined, reloadXULDocumentHook(url) is called before reloading.";
function reloadXULDocument(url){
  try {
    reloadXULDocumentHook(url);
  }catch(e){}

  var chromeCache = Components.classes["@mozilla.org/xul/xul-prototype-cache;1"].getService(Components.interfaces.nsIChromeCache);

  // Clear and reload the chrome file:
  chromeCache.flushXULPrototype(url);
}


//----------------------------------------------------------------------
_help_strings.time = "time(fct) returns execution time of 'fct' in milliseconds.";
function time(fct) {
  var start = new Date();
  fct();
  var end = new Date();
  return end.getTime() - start.getTime();
}

////////////////////////////////////////////////////////////////////////
// ZAP specific debug functions:

//----------------------------------------------------------------------
_help_strings.lastError = "lastError() returns the last verbose error from the zap verbose error service";
function lastError() {
  var errorService = Components.classes["@mozilla.org/zap/verbose-error-service;1"]
    .getService(Components.interfaces.zapIVerboseErrorService);
  return errorService.lastMessage;
}

//----------------------------------------------------------------------
_help_strings.dialogs = "dialogs() returns a list of currently active dialogs & associated info";
function dialogs() {
  var rv = "";
  var pool = getWindows()[0].wSipStack.wrappedJSObject.dialogPool;
  for (var d in pool) {
    rv += "callid="+pool[d].callID+"\n"+
      "state="+pool[d].dialogState+"\n"+
      "local tag="+pool[d].localTag+
      " remote tag="+pool[d].remoteTag+"\n"+
      "local uri="+pool[d].localURI.serialize()+"\n"+
      "remote uri="+pool[d].remoteURI.serialize()+"\n"+
      "remote target="+pool[d].remoteTarget.serialize()+"\n"+
      "local seq="+pool[d].localSequenceNumber+
      " remote seq="+pool[d].remoteSequenceNumber+
      "\n\n";
  }
  return rv;
}
