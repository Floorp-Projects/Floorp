/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/*globals async, is, SimpleTest, browserElementTestHelpers*/

// Bug 119580 - getStructuredData tests
'use strict';
SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

const EMPTY_URL = 'file_empty.html';
const MICRODATA_URL = 'file_microdata.html';
const MICRODATA_ITEMREF_URL = 'file_microdata_itemref.html';
const MICRODATA_BAD_ITEMREF_URL = 'file_microdata_bad_itemref.html';
const MICROFORMATS_URL = 'file_microformats.html';

var test1 = async(function* () {
  var structuredData = yield requestStructuredData(EMPTY_URL);
  is(structuredData.items && structuredData.items.length, 0,
     'There should be 0 items.');
});

var test2 = async(function* () {
  var structuredData = yield requestStructuredData(MICRODATA_URL);
  is(structuredData.items && structuredData.items.length, 2,
     'There should be two items.');
  is(structuredData.items[0].type[0], 'http://schema.org/Recipe',
     'Can get item type.');
  is(structuredData.items[0].properties['datePublished'][0], '2009-05-08',
     'Can get item property.');
  is(structuredData.items[1]
                   .properties["aggregateRating"][0]
                   .properties["ratingValue"][0],
     '4', 'Can get nested item property.');
});

var test3 = async(function* () {
  var structuredData = yield requestStructuredData(MICROFORMATS_URL);
  is(structuredData.items && structuredData.items.length, 2,
     'There should be two items.');
  is(structuredData.items[0].type[0], 'http://microformats.org/profile/hcard',
     'Got hCard object.');
  is(structuredData.items[0]
                   .properties["adr"][0]
                   .properties["country-name"][0],
     'France', 'Can read hCard properties.');
  is(structuredData.items[0]
                   .properties["adr"][0]
                   .properties["type"]
                   .includes('home') &&
     structuredData.items[0]
                   .properties["adr"][0]
                   .properties["type"]
                   .includes('postal'),
     true, 'Property can contain multiple values.');
  is(structuredData.items[0]
                   .properties["geo"][0],
     '48.816667;2.366667', 'Geo value is formatted as per WHATWG spec.');

  is(structuredData.items[1].type[0],
     'http://microformats.org/profile/hcalendar#vevent',
     'Got hCalendar object.');
  is(structuredData.items[1]
                   .properties["dtstart"][0],
     '2005-10-05', 'Can read hCalendar properties');
});

var test4 = async(function* () {
  var structuredData = yield requestStructuredData(MICRODATA_ITEMREF_URL);
  is(structuredData.items[0].properties["license"][0],
     'http://www.opensource.org/licenses/mit-license.php', 'itemref works.');
  is(structuredData.items[1].properties["license"][0],
     'http://www.opensource.org/licenses/mit-license.php',
     'Two items can successfully share an itemref.');
});

var test5 = async(function* () {
  var structuredData = yield requestStructuredData(MICRODATA_BAD_ITEMREF_URL);
  is(structuredData.items[0]
                   .properties["band"][0]
                   .properties["cycle"][0]
                   .properties["band"][0],
     'ERROR', 'Cyclic reference should be detected as an error.');
});

Promise
  .all([test1(), test2(), test3(), test4(), test5()])
  .then(SimpleTest.finish);

function requestStructuredData(url) {
  var iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  iframe.src = url;
  document.body.appendChild(iframe);
  return new Promise((resolve, reject) => {
    iframe.addEventListener('mozbrowserloadend', function loadend() {
      iframe.removeEventListener('mozbrowserloadend', loadend);
      SimpleTest.executeSoon(() => {
        var req = iframe.getStructuredData();
        req.onsuccess = (ev) => {
          document.body.removeChild(iframe);
          resolve(JSON.parse(req.result));
        };
        req.onerror = (ev) => {
          document.body.removeChild(iframe);
          reject(new Error(req.error));
        };
      });
    });
  });
}
