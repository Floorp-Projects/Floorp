/* Any copyright is dedicated to the public domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

// Bug 764718 - Test that mozbrowsererror works for a security error.

"use strict";

SimpleTest.waitForExplicitFinish();
browserElementTestHelpers.setEnabledPref(true);
browserElementTestHelpers.addPermission();

var iframe = null;
function runTest() {
  iframe = document.createElement('iframe');
  iframe.setAttribute('mozbrowser', 'true');
  document.body.appendChild(iframe);

  checkForDnsError();
}

function checkForDnsError() {
  iframe.addEventListener("mozbrowsererror", function onDnsError(e) {
    iframe.removeEventListener(e.type, onDnsError);
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "dnsNotFound", "Event's detail has a |type| param with the value '" + e.detail.type + "'.");

    checkForExpiredCertificateError();
  });

  iframe.src = "http://this_is_not_a_domain.example.com";
}

function checkForExpiredCertificateError() {
  iframe.addEventListener("mozbrowsererror", function onCertError(e) {
    iframe.removeEventListener(e.type, onCertError);
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "certerror", "Event's detail has a |type| param with the value '" + e.detail.type + "'.");

    checkForNoCertificateError();
  });

  iframe.src = "https://expired.example.com";
}


function checkForNoCertificateError() {
  iframe.addEventListener("mozbrowsererror", function onCertError(e) {
    iframe.removeEventListener(e.type, onCertError);
    ok(true, "Got mozbrowsererror event.");
    ok(e.detail.type == "certerror", "Event's detail has a |type| param with the value '" + e.detail.type + "'.");

    SimpleTest.finish();
  });

  iframe.src = "https://nocert.example.com";
}

addEventListener('testready', runTest);
