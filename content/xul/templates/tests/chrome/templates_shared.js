/**
 * This script is used for testing XUL templates. Call test_template within
 * a load event handler.
 *
 * A test should have a root node with the datasources attribute with the
 * id 'root', and a few global variables defined in the test's XUL file:
 *
 *  testid: the testid, used when outputting test results
 *  expectedOutput: e4x data containing the expected output. It can optionally
 *                  be enclosed in an <output> element as most tests generate
 *                  more than one node of output.
 *  isTreeBuilder: true for dont-build-content trees, false otherwise
 *  queryType: 'rdf', 'xml', etc.
 *  needsOpen: true for menu tests where the root menu must be opened before
 *             comparing results
 *  notWorkingYet: true if this test isn't working yet, outputs todo results
 *  notWorkingYetDynamic: true if the dynamic changes portion of the test
 *                        isn't working yet, outputs todo results
 *  changes: an array of functions to perform in sequence to test dynamic changes
 *           to the datasource.
 *
 * If the <output> element has an unordered attribute set to true, the
 * children within it must all appear to match, but may appear in any order.
 * If the unordered attribute is not set, the children must appear in the same
 * order.
 *
 * If the 'changes' array is used, it should be an array of functions. Each
 * function will be called in order and a comparison of the output will be
 * performed. This allows changes to be made to the datasource to ensure that
 * the generated template output has been updated. Within the expected output
 * e4x data, the step attribute may be set to a number on an element to
 * indicate that an element only applies before or after a particular change.
 * If step is set to a positive number, that element will only exist after
 * that step in the list of changes made. If step is set to a negative number,
 * that element will only exist until that step. Steps are numbered starting
 * at 1. For example:
 *   <label value="Cat"/>
 *   <label step="2" value="Dog"/>
 *   <label step="-5" value="Mouse"/>
 * The first element will always exist. The second element will only appear
 * after the second change is made. The third element will only appear until
 * the fifth change and it will no longer be present at later steps.
 *
 * If the anyid attribute is set to true on an element in the expected output,
 * then the value of the id attribute on that element is not compared for a
 * match. This is used, for example, for xml datasources, where the ids set on
 * the generated output are pseudo-random.
 */

const ZOO_NS = "http://www.some-fictitious-zoo.com/";
const XUL_NS = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";
const debug = false;

var expectedConsoleMessages = [];
var expectLoggedMessages = null;

try {
  const RDF = Components.classes["@mozilla.org/rdf/rdf-service;1"].
                getService(Components.interfaces.nsIRDFService);
  const ContainerUtils = Components.classes["@mozilla.org/rdf/container-utils;1"].
                           getService(Components.interfaces.nsIRDFContainerUtils);
} catch(ex) { }

var xmlDoc;

function test_template()
{
  var root = document.getElementById("root");

  var ds;
  if (queryType == "rdf" && RDF) {
    var ioService = Components.classes["@mozilla.org/network/io-service;1"].
                      getService(Components.interfaces.nsIIOService);

    var src = window.location.href.replace(/test_tmpl.*xul/, "animals.rdf");
    ds = RDF.GetDataSourceBlocking(src);

    if (expectLoggedMessages) {
      Components.classes["@mozilla.org/consoleservice;1"].
                 getService(Components.interfaces.nsIConsoleService).reset();
    }

    if (root.getAttribute("datasources") == "rdf:null")
      root.setAttribute("datasources", "animals.rdf");
  }
  else if (queryType == "xml") {
    var src = window.location.href.replace(/test_tmpl.*xul/, "animals.xml");
    xmlDoc = new XMLHttpRequest();
    xmlDoc.open("get", src, false);
    xmlDoc.send(null);
  }

  // open menus if necessary
  if (needsOpen)
    root.open = true;

  if (expectLoggedMessages)
    expectLoggedMessages();

  checkResults(root, 0);

  if (changes.length) {
    var usedds = ds;
    // within these chrome tests, RDF datasources won't be modifiable unless
    // an in-memory datasource is used instead. Call copyRDFDataSource to
    // copy the datasource.
    if (queryType == "rdf")
      usedds = copyRDFDataSource(root, ds);
    if (needsOpen)
      root.open = true;
    setTimeout(iterateChanged, 0, root, usedds);
  }
  else {
    if (needsOpen)
      root.open = false;
    if (expectedConsoleMessages.length)
      compareConsoleMessages();
    SimpleTest.finish();
  }
}

function iterateChanged(root, ds)
{
  Components.classes["@mozilla.org/consoleservice;1"].
             getService(Components.interfaces.nsIConsoleService).reset();

  for (var c = 0; c < changes.length; c++) {
    changes[c](ds, root);
    checkResults(root, c + 1);
  }

  if (needsOpen)
    root.open = false;
  if (expectedConsoleMessages.length)
    compareConsoleMessages();
  SimpleTest.finish();
}

function checkResults(root, step)
{
  var output = expectedOutput.copy();
  setForCurrentStep(output, step);

  var error;
  var actualoutput = root;
  if (isTreeBuilder) {
    // convert the tree's view data into the equivalent DOM structure
    // for easier comparison
    actualoutput = treeViewToDOM(root);
    error = compareOutput(actualoutput, output.treechildren, false);
  }
  else {
    error = compareOutput(actualoutput, output, true);
  }

  var adjtestid = testid;
  if (step > 0)
    adjtestid += " dynamic step " + step;

  var stilltodo = ((step == 0 && notWorkingYet) || (step > 0 && notWorkingYetDynamic));
  if (stilltodo)
    todo(false, adjtestid);
  else
    ok(!error, adjtestid);

  if ((!stilltodo && error) || debug) {
    // for debugging, serialize the XML output
    var serializedXML = "";
    var rootNodes = actualoutput.childNodes;
    for (var n = 0; n < rootNodes.length; n++) {
      var node = rootNodes[n];
      if (node.localName != "template")
        serializedXML += ((new XMLSerializer()).serializeToString(node));
    }

    // remove the XUL namespace declarations to make the output more readable
    const nsrepl = new RegExp("xmlns=\"" + XUL_NS + "\" ", "g");
    serializedXML = serializedXML.replace(nsrepl, "");
    if (debug)
      dump("-------- " + adjtestid + "  " + error + ":\n" + serializedXML + "\n");
    if (error)
      is(serializedXML, "Same", "Error is: " + error);
  }
}

/**
 * Adjust the expected output to acccount for any step attributes.
 */
function setForCurrentStep(content, currentStep)
{
  var todelete = [];
  for each (var child in content) {
    var stepstr = child.@step.toString();
    var stepsarr = stepstr.split(",");
    for (var s = 0; s < stepsarr.length; s++) {
      var step = parseInt(stepsarr[s]);
      if ((step > 0 && step > currentStep) ||
          (step < 0 && -step <= currentStep)) {
        todelete.push(child);
      }
    }
  }

  // delete node using this bizarre syntax, because e4x has a non-sensical api
  for (var d = 0; d < todelete.length; d++)
    delete content.*[todelete[d].childIndex()];
  
  for each (var child in content) {
    delete child.@step;
    setForCurrentStep(child, currentStep);
  }
}

/**
 * Compares the 'actual' DOM output with the 'expected' output. This function
 * is called recursively, with isroot true if actual refers to the root of the
 * template. Returns a null string if they are equal and an error string if
 * they are not equal. This function is called recursively as it iterates
 * through each node in the DOM tree.
 */
function compareOutput(actual, expected, isroot)
{
  // The expected output may be enclosed in an <output> element if it contains
  // several nodes. However, the <output> element may be omitted if there is
  // only one node.
  if (expected.localName() != "output" && isroot) {
    // If there is no <output> element then only one node should have been
    // generated by the template. The number of children should be one
    // <template> node and one generated node.
    if (actual.childNodes.length != 2)
      return "incorrect child node count of root " +
             (actual.childNodes.length - 1) + " expected 1";
    return compareOutput(actual.lastChild, expected, false);
  }

  var t;

  // compare text nodes
  if (expected.nodeKind() == "text") {
    if (actual.nodeValue != expected.toString())
      return "Text " + actual.nodeValue + " doesn't match " + expected.toString();
    return "";
  }

  if (!isroot) {
    var anyid = false;
    // make sure that the tags match
    if (actual.localName != expected.localName())
      return "Tag name " + expected.localName() + " not found";

    // loop through the attributes in the expected node and compare their
    // values with the corresponding attribute on the actual node

    var expectedAttrs = expected.attributes();
    for (var a = 0; a < expectedAttrs.length(); a++) {
      var attr = expectedAttrs[a];
      expectedAttrs.length(); // not having this causes a script error
      var expectval = "" + attr;
      // skip checking the id when anyid="true", however make sure to
      // ensure that the id is actually present.
      if (attr.name() == "anyid" && expectval == "true") {
        anyid = true;
        if (!actual.hasAttribute("id"))
          return "expected id attribute";
      }
      else if (actual.getAttribute(attr.name()) != expectval) {
        return "attribute " + attr.name() + " is '" +
               actual.getAttribute(attr.name()) + "' instead of  '" + expectval + "'";
      }
    }

    // now loop through the actual attributes and make sure that there aren't
    // any extra attributes that weren't expected
    var length = actual.attributes.length;
    for (t = 0; t < length; t++) {
      var aattr = actual.attributes[t];
      var expectval = "" + expected.@[aattr.name];
      // ignore some attributes that don't matter
      if (expectval != actual.getAttribute(aattr.name) &&
          aattr.name != "staticHint" && aattr.name != "xmlns" &&
          (aattr.name != "id" || !anyid))
        return "extra attribute " + aattr.name;
    }
  }

  // ensure that the node has the right number of children. Subtract one for
  // the root node to account for the <template> node.
  length = actual.childNodes.length - (isroot ? 1 : 0);
  if (length != expected.children().length())
    return "incorrect child node count of " + actual.localName + " " + length +
           " expected " + expected.children().length();

  // if <output unordered="true"> is used, then the child nodes may be in any order
  var unordered = (expected.localName() == "output" && expected.@unordered == "true");

  // next, loop over the children and call compareOutput recursively on each one
  var adj = 0;
  for (t = 0; t < actual.childNodes.length; t++) {
    var actualnode = actual.childNodes[t];
    // skip the <template> element, and add one to the indices when looking
    // at the later nodes to account for it
    if (isroot && actualnode.localName == "template") {
      adj++;
    }
    else {
      var output = "unexpected";
      if (unordered) {
        var expectedChildren = expected.children();
        for (var e = 0; e < expectedChildren.length(); e++) {
          output = compareOutput(actualnode, expectedChildren[e], false);
          if (!output)
            break;
        }
      }
      else {
        output = compareOutput(actualnode, expected.children()[t - adj], false);
      }

      // an error was returned, so return early
      if (output)
        return output;
    }
  }

  return "";
}

/*
 * copy the datasource into an in-memory datasource so that it can be modified
 */
function copyRDFDataSource(root, sourceds)
{
  var sourceds;
  var dsourcesArr = [];
  var composite = root.database;
  var dsources = composite.GetDataSources();
  while (dsources.hasMoreElements()) {
    sourceds = dsources.getNext().QueryInterface(Components.interfaces.nsIRDFDataSource);
    dsourcesArr.push(sourceds);
  }

  for (var d = 0; d < dsourcesArr.length; d++)
    composite.RemoveDataSource(dsourcesArr[d]);

  var newds = Components.classes["@mozilla.org/rdf/datasource;1?name=in-memory-datasource"].
                createInstance(Components.interfaces.nsIRDFDataSource);

  var sourcelist = sourceds.GetAllResources();
  while (sourcelist.hasMoreElements()) {
    var source = sourcelist.getNext();
    var props = sourceds.ArcLabelsOut(source);
    while (props.hasMoreElements()) {
      var prop = props.getNext();
      if (prop instanceof Components.interfaces.nsIRDFResource) {
        var targets = sourceds.GetTargets(source, prop, true);
        while (targets.hasMoreElements())
          newds.Assert(source, prop, targets.getNext(), true);
      }
    }
  }

  composite.AddDataSource(newds);
  root.builder.rebuild();

  return newds;
}

/**
 * Converts a tree view (nsITreeView) into the equivalent DOM tree.
 * Returns the treechildren
 */
function treeViewToDOM(tree)
{
  var treechildren = document.createElement("treechildren");

  if (tree.view)
    treeViewToDOMInner(tree.columns, treechildren, tree.view, tree.builder, 0, 0);

  return treechildren;
}

function treeViewToDOMInner(columns, treechildren, view, builder, start, level)
{
  var end = view.rowCount;

  for (var i = start; i < end; i++) {
    if (view.getLevel(i) < level)
      return i - 1;

    var id = builder ? builder.getResourceAtIndex(i).Value : "id" + i;
    var item = document.createElement("treeitem");
    item.setAttribute("id", id);
    treechildren.appendChild(item);

    var row = document.createElement("treerow");
    item.appendChild(row);

    for (var c = 0; c < columns.length; c++) {
      var cell = document.createElement("treecell");
      var label = view.getCellText(i, columns[c]);
      if (label)
        cell.setAttribute("label", label);
      row.appendChild(cell);
    }

    if (view.isContainer(i)) {
      item.setAttribute("container", "true");
      item.setAttribute("empty", view.isContainerEmpty(i) ? "true" : "false");

      if (!view.isContainerEmpty(i) && view.isContainerOpen(i)) {
        item.setAttribute("open", "true");

        var innertreechildren = document.createElement("treechildren");
        item.appendChild(innertreechildren);

        i = treeViewToDOMInner(columns, innertreechildren, view, builder, i + 1, level + 1);
      }
    }
  }

  return i;
}

function expectConsoleMessage(ref, id, isNew, isActive, extra)
{
  var message = "In template with id root" +
                (ref ? " using ref " + ref : "") + "\n    " +
                (isNew ? "New " : "Removed ") + (isActive ? "active" : "inactive") +
                " result for query " + extra + ": " + id;
  expectedConsoleMessages.push(message);
}

function compareConsoleMessages()
{
   var consoleService = Components.classes["@mozilla.org/consoleservice;1"].
                          getService(Components.interfaces.nsIConsoleService);
   var out = {};
   consoleService.getMessageArray(out, {});
   var messages = out.value || [];
   is(messages.length, expectedConsoleMessages.length, "correct number of logged messages");
   for (var m = 0; m < messages.length; m++) {
     is(messages[m].message, expectedConsoleMessages.shift(), "logged message " + (m + 1));
   }
}

function copyToProfile(filename)
{
  if (Cc === undefined) {
    var Cc = Components.classes;
    var Ci = Components.interfaces;
  }

  var loader = Cc["@mozilla.org/moz/jssubscript-loader;1"]
                         .getService(Ci.mozIJSSubScriptLoader);
  loader.loadSubScript("chrome://mochikit/content/chrome-harness.js");

  var file = Cc["@mozilla.org/file/directory_service;1"]
                       .getService(Ci.nsIProperties)
                       .get("ProfD", Ci.nsIFile);
  file.append(filename);

  var parentURI = getResolvedURI(getRootDirectory(window.location.href));
  if (parentURI.JARFile) {
    parentURI = extractJarToTmp(parentURI);
  } else {
    var fileHandler = Cc["@mozilla.org/network/protocol;1?name=file"].
                      getService(Ci.nsIFileProtocolHandler);
    parentURI = fileHandler.getFileFromURLSpec(parentURI.spec);
  }

  parentURI = parentURI.QueryInterface(Ci.nsILocalFile);
  parentURI.append(filename);
  try {
    var retVal = parentURI.copyToFollowingLinks(file.parent, filename);
  } catch (ex) {
    //ignore this error as the file could exist already
  }
}
