/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */

Cu.importGlobalProperties(["DOMParser"]);

function run_test()
{
  var domParser = new DOMParser();
  var aDom = domParser.parseFromString("<root><feed><entry/><entry/></feed></root>",
                                       "application/xml");
  var feedList = aDom.getElementsByTagName("feed");
  Assert.notEqual(feedList, null);
  Assert.equal(feedList.length, 1);
  Assert.notEqual(feedList[0], null);
  Assert.equal(feedList[0].tagName, "feed");
  var entry = feedList[0].getElementsByTagName("entry");
  Assert.notEqual(entry, null);
}
