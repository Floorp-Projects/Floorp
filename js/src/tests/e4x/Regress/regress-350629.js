// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


//-----------------------------------------------------------------------------
var BUGNUMBER     = "350629";
var summary = ".toXMLString can include invalid generated prefixes";
var actual, expect;

printBugNumber(BUGNUMBER);
START(summary);

/**************
 * BEGIN TEST *
 **************/

var failed = false;

function extractPrefix(el, attrName, attrVal)
{
  var str = el.toXMLString();
  var regex = new RegExp(' (.+?):' + attrName + '="' + attrVal + '"');
  return str.match(regex)[1];
}

function assertValidPrefix(p, msg)
{
  if (!isXMLName(p) ||
      0 == p.search(/xml/i))
    throw msg;
}

var el, n, p;

try
{
  // last component is invalid prefix
  el = <foo/>;
  n = new Namespace("http://foo/bar.xml");
  el.@n::fiz = "eit";
  p = extractPrefix(el, "fiz", "eit");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);

  // last component is invalid prefix (different case)
  el = <foo/>;
  n = new Namespace("http://foo/bar.XML");
  el.@n::fiz = "eit";
  p = extractPrefix(el, "fiz", "eit");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);

  // last component is invalid prefix (but not "xml"/"xmlns")
  el = <foo/>;
  n = new Namespace("http://foo/bar.xmln");
  el.@n::baz = "quux";
  p = extractPrefix(el, "baz", "quux");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);


  // generated prefix with no valid prefix component in namespace URI
  el = <foo/>;
  n = new Namespace("xml:///");
  el.@n::bike = "cycle";
  p = extractPrefix(el, "bike", "cycle");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);


  // generated prefix with no valid prefix component in namespace URI w/further
  // collision
  el = <aaaaa:foo xmlns:aaaaa="http://baz/"/>;
  n = new Namespace("xml:///");
  el.@n::bike = "cycle";
  p = extractPrefix(el, "bike", "cycle");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);



  // XXX this almost certainly shouldn't work, so if it fails at some time it
  //     might not be a bug!  it's only here because it *is* currently a
  //     possible failure point for prefix generation
  el = <foo/>;
  n = new Namespace(".:/.././.:/:");
  el.@n::biz = "17";
  p = extractPrefix(el, "biz", "17");
  assertValidPrefix(p, "namespace " + n.uri + " generated invalid prefix " + p);
}
catch (ex)
{
  failed = ex;
}

expect = false;
actual = failed;

TEST(1, expect, actual);
