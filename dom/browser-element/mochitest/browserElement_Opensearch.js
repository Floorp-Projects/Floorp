/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowseropensearch event works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function createHtml(link) {
  return 'data:text/html,<html><head>' + link + '<body></body></html>';
}

function createLink(name) {
  return '<link rel="search" title="Test OpenSearch" type="application/opensearchdescription+xml" href="http://example.com/' + name + '.xml">';
}

function runTest() {
  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe1);

  // iframe2 is a red herring; we modify its link but don't listen for
  // opensearch; we want to make sure that its opensearch events aren't
  // picked up by the listener on iframe1.
  var iframe2 = document.createElement('iframe');
  iframe2.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe2);

  // iframe3 is another red herring.  It's not a mozbrowser, so we shouldn't
  // get any opensearch events on it.
  var iframe3 = document.createElement('iframe');
  document.body.appendChild(iframe3);

  var numLinkChanges = 0;

  iframe1.addEventListener('mozbrowseropensearch', function(e) {

    numLinkChanges++;

    if (numLinkChanges == 1) {
      is(e.detail.title, 'Test OpenSearch');
      is(e.detail.href, 'http://example.com/mysearch.xml');

      // We should recieve opensearch events when the user creates new links
      // to a search engine, but only when we listen for them
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.title='New title';",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=SEARCH type=application/opensearchdescription+xml href=http://example.com/newsearch.xml>')",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe2)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=SEARCH type=application/opensearchdescription+xml href=http://example.com/newsearch.xml>')",
                                    /* allowDelayedLoad = */ false);
    }
    else if (numLinkChanges == 2) {
      is(e.detail.href, 'http://example.com/newsearch.xml');

      // Full new pages should trigger opensearch events
      iframe1.src = createHtml(createLink('3rdsearch'));
    }
    else if (numLinkChanges == 3) {
      is(e.detail.href, 'http://example.com/3rdsearch.xml');

      // the rel attribute can have various space seperated values, make
      // sure we only pick up correct values for 'opensearch'
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=someopensearch type=application/opensearchdescription+xml href=http://example.com/newsearch.xml>')",
                                    /* allowDelayedLoad = */ false);
      // Test setting a page with multiple links elements
      iframe1.src = createHtml(createLink('another') + createLink('search'));
    }
    else if (numLinkChanges == 4) {
      is(e.detail.href, 'http://example.com/another.xml');
      // 2 events will be triggered by previous test, wait for next
    }
    else if (numLinkChanges == 5) {
      is(e.detail.href, 'http://example.com/search.xml');

      // Make sure opensearch check is case insensitive
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=SEARCH type=application/opensearchdescription+xml href=http://example.com/ucasesearch.xml>')",
                                    /* allowDelayedLoad = */ false);
    }
    else if (numLinkChanges == 6) {
      is(e.detail.href, 'http://example.com/ucasesearch.xml');
      SimpleTest.finish();
    } else {
      ok(false, 'Too many opensearch events.');
    }
  });

  iframe3.addEventListener('mozbrowseropensearch', function(e) {
    ok(false, 'Should not get a opensearch event for iframe3.');
  });


  iframe1.src = createHtml(createLink('mysearch'));
  // We should not recieve opensearch change events for either of the below iframes
  iframe2.src = createHtml(createLink('mysearch'));
  iframe3.src = createHtml(createLink('mysearch'));

}

addEventListener('testready', runTest);
