// |reftest| pref(javascript.options.xml.content,true)
/* -*- Mode: java; tab-width:8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


var summary = "Do not crash when qn->uri is null";
var BUGNUMBER = 323338;
var actual = 'No Crash';
var expect = 'No Crash';

printBugNumber(BUGNUMBER);
START(summary);

testFunc();
testFunc();

function testFunc()
{
  var htmlXML =
   <html>
    <body>
     <div>
      <div id="summary" />
      <div id="desc" />
     </div>
    </body>
   </html>;
  var childs = htmlXML.children();

  var el = htmlXML.body.div..div.(function::attribute('id') == 'summary');
  el.div += <div>
              <strong>Prototype:</strong>
              Test
              <br />
             </div>;
}

TEST(1, expect, actual);

END();
