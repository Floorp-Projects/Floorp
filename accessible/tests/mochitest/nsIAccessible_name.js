function testName(aAccOrElmOrID, aName, aMsg)
{
  var msg = aMsg ? aMsg : "";

  var acc = getAccessible(aAccOrElmOrID);
  if (!acc) {
    ok(false, msg + "No accessible for " + aAccOrElmOrID + "!");
  }

  try {
    is(acc.name, aName, msg + "Wrong name of the accessible for " + aAccOrElmOrID);
  } catch (e) {
    ok(false, msg + "Can't get name of the accessible for " + aAccOrElmOrID);
  }
  return acc;
}

////////////////////////////////////////////////////////////////////////////////
// Name tests described by "namerules.xml" file.

var gNameRulesFileURL =
  "chrome://mochikit/content/a11y/accessible/namerules.xml";

var gRuleDoc = null;

/**
 * Start name tests. Run through markup elements and test names for test
 * element (see namerules.xml for details).
 */
function testNames()
{
  var request = new XMLHttpRequest();
  request.open("get", gNameRulesFileURL, false);
  request.send();

  gRuleDoc = request.responseXML;

  var markupElms = evaluateXPath(gRuleDoc, "//rules/rulesample/markup");
  for (var idx = 0; idx < markupElms.length; idx++)
    testNamesForMarkup(markupElms[idx]);
}

////////////////////////////////////////////////////////////////////////////////
// Private section.

/**
 * Process every 'markup' element and test names for it. Used by testNames
 * function.
 */
function testNamesForMarkup(aMarkupElm)
{
  var div = document.createElement("div");
  div.setAttribute("test", "test");

  var child = aMarkupElm.firstChild;
  while (child) {
    var newChild = document.importNode(child, true);
    div.appendChild(newChild);
    child = child.nextSibling;
  }
  document.body.appendChild(div);

  var serializer = new XMLSerializer();

  var expr = "//html/body/div[@test='test']/" + aMarkupElm.getAttribute("ref");
  var elms = evaluateXPath(document, expr, htmlDocResolver);

  var ruleId = aMarkupElm.getAttribute("ruleset");
  var ruleElms = getRuleElmsByRulesetId(ruleId);

  for (var idx = 0; idx < ruleElms.length; idx++)
    testNameForRule(elms[0], ruleElms[idx]);

  document.body.removeChild(div);
}

/**
 * Test name for current rule and current 'markup' element. Used by
 * testNamesForMarkup function.
 */
function testNameForRule(aElm, aRule)
{
  var attr = aRule.getAttribute("attr");
  if (attr) {
    var name = "";
    var attrValue = aElm.getAttribute(attr);
  
    var type = aRule.getAttribute("type");
    if (type == "string") {
      name = attrValue;

    } else if (type == "ref") {
      var ids = attrValue.split(/\s+/);///\,\s*/);
      for (var idx = 0; idx < ids.length; idx++) {
        var labelElm = getNode(ids[idx]);
        if (name != "")
          name += " ";

        name += labelElm.getAttribute("a11yname");
      }
    }

    var msg = "Attribute '" + attr + "' test. ";
    testName(aElm, name, msg);
    aElm.removeAttribute(attr);
    return;
  }

  var elm = aRule.getAttribute("elm");
  var elmattr = aRule.getAttribute("elmattr");
  if (elm && elmattr) {
    var filter = {
      acceptNode: function filter_acceptNode(aNode)
      {
        if (aNode.localName == this.mLocalName &&
            aNode.getAttribute(this.mAttrName) == this.mAttrValue)
          return NodeFilter.FILTER_ACCEPT;

        return NodeFilter.FILTER_SKIP;
      },

      mLocalName: elm,
      mAttrName: elmattr,
      mAttrValue: aElm.getAttribute("id")
    };

    var treeWalker = document.createTreeWalker(document.body,
                                               NodeFilter.SHOW_ELEMENT,
                                               filter, false);
    var labelElm = treeWalker.nextNode();
    var msg = "Element '" + elm + "' test.";
    testName(aElm, labelElm.getAttribute("a11yname"), msg);
    labelElm.parentNode.removeChild(labelElm);
    return;
  }

  var fromSubtree = aRule.getAttribute("fromsubtree");
  if (fromSubtree == "true") {
    var msg = "From subtree test.";
    testName(aElm, aElm.getAttribute("a11yname"), msg);
    while (aElm.firstChild)
      aElm.removeChild(aElm.firstChild);
    return;
  }
}

/**
 * Return array of 'rule' elements. Used in conjunction with
 * getRuleElmsFromRulesetElm() function.
 */
function getRuleElmsByRulesetId(aRulesetId)
{
  var expr = "//rules/ruledfn/ruleset[@id='" + aRulesetId + "']";
  var rulesetElm = evaluateXPath(gRuleDoc, expr);
  return getRuleElmsFromRulesetElm(rulesetElm[0]);
}

function getRuleElmsFromRulesetElm(aRulesetElm)
{
  var rulesetId = aRulesetElm.getAttribute("ref");
  if (rulesetId)
    return getRuleElmsByRulesetId(rulesetId);

  var ruleElms = [];

  var child = aRulesetElm.firstChild;
  while (child) {
    if (child.localName == "ruleset")
      ruleElms = ruleElms.concat(getRuleElmsFromRulesetElm(child));
    if (child.localName == "rule")
      ruleElms.push(child);

    child = child.nextSibling;
  }

  return ruleElms;
}

/**
 * Helper method to evaluate xpath expression.
 */
function evaluateXPath(aNode, aExpr, aResolver)
{
  var xpe = new XPathEvaluator();

  var resolver = aResolver;
  if (!resolver) {
    var node = aNode.ownerDocument == null ?
      aNode.documentElement : aNode.ownerDocument.documentElement;
    resolver = xpe.createNSResolver(node);
  }

  var result = xpe.evaluate(aExpr, aNode, resolver, 0, null);
  var found = [];
  var res;
  while (res = result.iterateNext())
    found.push(res);

  return found;
}

function htmlDocResolver(aPrefix) {
  var ns = {
    'html' : 'http://www.w3.org/1999/xhtml'
  };
  return ns[aPrefix] || null;
}
