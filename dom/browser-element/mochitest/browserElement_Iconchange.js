/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowsericonchange event works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function createHtml(link) {
  return 'data:text/html,<html><head>' + link + '<body></body></html>';
}

function createLink(name) {
  return '<link rel="icon" type="image/png" href="http://example.com/' + name + '.png">';
}

function runTest() {
  var iframe1 = document.createElement('iframe');
  SpecialPowers.wrap(iframe1).mozbrowser = true;
  document.body.appendChild(iframe1);

  // iframe2 is a red herring; we modify its favicon but don't listen for
  // iconchanges; we want to make sure that its iconchange events aren't
  // picked up by the listener on iframe1.
  var iframe2 = document.createElement('iframe');
  SpecialPowers.wrap(iframe2).mozbrowser = true;
  document.body.appendChild(iframe2);

  // iframe3 is another red herring.  It's not a mozbrowser, so we shouldn't
  // get any iconchange events on it.
  var iframe3 = document.createElement('iframe');
  document.body.appendChild(iframe3);

  var numIconChanges = 0;

  iframe1.addEventListener('mozbrowsericonchange', function(e) {

    numIconChanges++;

    if (numIconChanges == 1) {
      is(e.detail, 'http://example.com/myicon.png');

      // We should recieve iconchange events when the user creates new links
      // to a favicon, but only when we listen for them
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.title='New title';",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=ICON href=http://example.com/newicon.png>')",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe2)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=ICON href=http://example.com/newicon.png>')",
                                    /* allowDelayedLoad = */ false);
    }
    else if (numIconChanges == 2) {
      is(e.detail, 'http://example.com/newicon.png');

      // Full new pages should trigger iconchange events
      iframe1.src = createHtml(createLink('3rdicon'));
    }
    else if (numIconChanges == 3) {
      is(e.detail, 'http://example.com/3rdicon.png');

      // the rel attribute can have various space seperated values, make
      // sure we only pick up correct values for 'icon'
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=shortcuticon href=http://example.com/newicon.png>')",
                                    /* allowDelayedLoad = */ false);
      // Test setting a page with multiple links elements
      iframe1.src = createHtml(createLink('another') + createLink('icon'));
    }
    else if (numIconChanges == 4) {
      is(e.detail, 'http://example.com/another.png');
      // 2 events will be triggered by previous test, wait for next
    }
    else if (numIconChanges == 5) {
      is(e.detail, 'http://example.com/icon.png');

      // Make sure icon check is case insensitive
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=ICON href=http://example.com/ucaseicon.png>')",
                                    /* allowDelayedLoad = */ false);
    }
    else if (numIconChanges == 6) {
      is(e.detail, 'http://example.com/ucaseicon.png');
      SimpleTest.finish();
    } else {
      ok(false, 'Too many iconchange events.');
    }
  });

  iframe3.addEventListener('mozbrowsericonchange', function(e) {
    ok(false, 'Should not get a iconchange event for iframe3.');
  });


  iframe1.src = createHtml(createLink('myicon'));
  // We should not recieve icon change events for either of the below iframes
  iframe2.src = createHtml(createLink('myicon'));
  iframe3.src = createHtml(createLink('myicon'));

}

addEventListener('testready', runTest);
