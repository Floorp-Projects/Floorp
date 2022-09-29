/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function test_getCreditCardLogo() {
  const { CreditCard } = ChromeUtils.importESModule(
    "resource://gre/modules/CreditCard.sys.mjs"
  );
  // Credit card logos can be either PNG or SVG
  // so we construct an array that includes both of these file extensions
  // and test to see if the logo from getCreditCardLogo matches.
  for (let network of CreditCard.getSupportedNetworks()) {
    const PATH_PREFIX = "chrome://formautofill/content/third-party/cc-logo-";
    let actual = CreditCard.getCreditCardLogo(network);
    Assert.ok(
      [".png", ".svg"].map(x => PATH_PREFIX + network + x).includes(actual)
    );
  }
  let genericLogo = CreditCard.getCreditCardLogo("null");
  Assert.equal(
    genericLogo,
    "chrome://formautofill/content/icon-credit-card-generic.svg"
  );
});
