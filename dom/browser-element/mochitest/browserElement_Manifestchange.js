/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that the onmozbrowsermanifestchange event works.
"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

function createHtml(manifest) {
  return 'data:text/html,<html xmlns:xml="http://www.w3.org/XML/1998/namespace"><head>' + manifest + '<body></body></html>';
}

function createManifest(href) {
  return '<link rel="manifest" href="' + href + '">';
}

function runTest() {
  var iframe1 = document.createElement('iframe');
  iframe1.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe1);

  // iframe2 is a red herring; we modify its manifest link elements but don't
  // listen for manifestchanges; we want to make sure that its manifestchange
  // events aren't picked up by the listener on iframe1.
  var iframe2 = document.createElement('iframe');
  iframe2.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe2);

  // iframe3 is another red herring.  It's not a mozbrowser, so we shouldn't
  // get any manifestchange events on it.
  var iframe3 = document.createElement('iframe');
  document.body.appendChild(iframe3);

  var numManifestChanges = 0;

  iframe1.addEventListener('mozbrowsermanifestchange', function(e) {

    numManifestChanges++;

    if (numManifestChanges == 1) {
      is(e.detail.href, 'manifest.1', 'manifest.1 matches');

      // We should recieve manifestchange events when the user creates new
      // manifests
      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.title='New title';",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe1)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=manifest href=manifest.2>')",
                                    /* allowDelayedLoad = */ false);

      SpecialPowers.getBrowserFrameMessageManager(iframe2)
                   .loadFrameScript("data:,content.document.head.insertAdjacentHTML('beforeend', '<link rel=manifest href=manifest.2>')",
                                    /* allowDelayedLoad = */ false);
    }
    else if (numManifestChanges == 2) {
      is(e.detail.href, 'manifest.2', 'manifest.2 matches');

      // Full new pages should trigger manifestchange events
      iframe1.src = createHtml(createManifest('manifest.3'));
    }
    else if (numManifestChanges == 3) {
      is(e.detail.href, 'manifest.3', 'manifest.3 matches');

      // Test setting a page with multiple manifest link elements
      iframe1.src = createHtml(createManifest('manifest.4a') + createManifest('manifest.4b'));
    }
    else if (numManifestChanges == 4) {
      is(e.detail.href, 'manifest.4a', 'manifest.4a matches');
      // 2 events will be triggered by previous test, wait for next
    }
    else if (numManifestChanges == 5) {
      is(e.detail.href, 'manifest.4b', 'manifest.4b matches');
      SimpleTest.finish();
    } else {
      ok(false, 'Too many manifestchange events.');
    }
  });

  iframe3.addEventListener('mozbrowsermanifestchange', function(e) {
    ok(false, 'Should not get a manifestchange event for iframe3.');
  });


  iframe1.src = createHtml(createManifest('manifest.1'));
  // We should not recieve manifest change events for either of the below iframes
  iframe2.src = createHtml(createManifest('manifest.1'));
  iframe3.src = createHtml(createManifest('manifest.1'));

}

addEventListener('testready', runTest);

