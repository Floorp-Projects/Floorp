/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* global structuredClone */

const PROFILE = {
  id: "Default",
  name: "Default",
};

const PAYMENT_METHODS = [
  {
    name_on_card: "Name Name",
    card_number: "4532962432748929", // Visa
    expiration_month: 3,
    expiration_year: 2027,
  },
  {
    name_on_card: "Name Name Name",
    card_number: "5359908373796416", // Mastercard
    expiration_month: 5,
    expiration_year: 2028,
  },
  {
    name_on_card: "Name",
    card_number: "346624461807588", // AMEX
    expiration_month: 4,
    expiration_year: 2026,
  },
];

let OSKeyStoreTestUtils;
add_setup(async function os_key_store_setup() {
  ({ OSKeyStoreTestUtils } = ChromeUtils.importESModule(
    "resource://testing-common/OSKeyStoreTestUtils.sys.mjs"
  ));
  OSKeyStoreTestUtils.setup();
  registerCleanupFunction(async function cleanup() {
    await OSKeyStoreTestUtils.cleanup();
  });
});

let rootDir = do_get_file("chromefiles/", true);

function checkCardsAreEqual(importedCard, testCard, id) {
  const CC_NUMBER_RE = /^(\*+)(.{4})$/;

  Assert.equal(
    importedCard["cc-name"],
    testCard.name_on_card,
    "The two logins ID " + id + " have the same name on card"
  );

  let matches = CC_NUMBER_RE.exec(importedCard["cc-number"]);
  Assert.notEqual(matches, null);
  Assert.equal(importedCard["cc-number"].length, testCard.card_number.length);
  Assert.equal(testCard.card_number.endsWith(matches[2]), true);
  Assert.notEqual(importedCard["cc-number-encrypted"], "");

  Assert.equal(
    importedCard["cc-exp-month"],
    testCard.expiration_month,
    "The two logins ID " + id + " have the same expiration month"
  );
  Assert.equal(
    importedCard["cc-exp-year"],
    testCard.expiration_year,
    "The two logins ID " + id + " have the same expiration year"
  );
}

add_task(async function setup_fakePaths() {
  let pathId;
  if (AppConstants.platform == "macosx") {
    pathId = "ULibDir";
  } else if (AppConstants.platform == "win") {
    pathId = "LocalAppData";
  } else {
    pathId = "Home";
  }
  registerFakePath(pathId, rootDir);
});

add_task(async function test_credit_cards() {
  if (!OSKeyStoreTestUtils.canTestOSKeyStoreLogin()) {
    todo_check_true(
      OSKeyStoreTestUtils.canTestOSKeyStoreLogin(),
      "Cannot test OS key store login on official builds."
    );
    return;
  }

  let loginCrypto;
  let profilePathSegments;

  let mockMacOSKeychain = {
    passphrase: "bW96aWxsYWZpcmVmb3g=",
    serviceName: "TESTING Chrome Safe Storage",
    accountName: "TESTING Chrome",
  };
  if (AppConstants.platform == "macosx") {
    let { ChromeMacOSLoginCrypto } = ChromeUtils.importESModule(
      "resource:///modules/ChromeMacOSLoginCrypto.sys.mjs"
    );
    loginCrypto = new ChromeMacOSLoginCrypto(
      mockMacOSKeychain.serviceName,
      mockMacOSKeychain.accountName,
      mockMacOSKeychain.passphrase
    );
    profilePathSegments = [
      "Application Support",
      "Google",
      "Chrome",
      "Default",
    ];
  } else if (AppConstants.platform == "win") {
    let { ChromeWindowsLoginCrypto } = ChromeUtils.importESModule(
      "resource:///modules/ChromeWindowsLoginCrypto.sys.mjs"
    );
    loginCrypto = new ChromeWindowsLoginCrypto("Chrome");
    profilePathSegments = ["Google", "Chrome", "User Data", "Default"];
  } else {
    throw new Error("Not implemented");
  }

  let target = rootDir.clone();
  let defaultFolderPath = PathUtils.join(target.path, ...profilePathSegments);
  let webDataPath = PathUtils.join(defaultFolderPath, "Web Data");
  let localStatePath = defaultFolderPath.replace("Default", "");

  await IOUtils.makeDirectory(defaultFolderPath, {
    createAncestor: true,
    ignoreExisting: true,
  });

  // Copy Web Data database into Default profile
  const sourcePathWebData = do_get_file(
    "AppData/Local/Google/Chrome/User Data/Default/Web Data"
  ).path;
  await IOUtils.copy(sourcePathWebData, webDataPath);

  const sourcePathLocalState = do_get_file(
    "AppData/Local/Google/Chrome/User Data/Local State"
  ).path;
  await IOUtils.copy(sourcePathLocalState, localStatePath);

  let dbConn = await Sqlite.openConnection({ path: webDataPath });

  for (let card of PAYMENT_METHODS) {
    let encryptedCardNumber = await loginCrypto.encryptData(card.card_number);
    let cardNumberEncryptedValue = new Uint8Array(
      loginCrypto.stringToArray(encryptedCardNumber)
    );

    let cardCopy = structuredClone(card);

    cardCopy.card_number_encrypted = cardNumberEncryptedValue;
    delete cardCopy.card_number;

    await dbConn.execute(
      `INSERT INTO credit_cards
                           (name_on_card, card_number_encrypted, expiration_month, expiration_year)
                           VALUES (:name_on_card, :card_number_encrypted, :expiration_month, :expiration_year)
                          `,
      cardCopy
    );
  }

  await dbConn.close();

  let migrator = await MigrationUtils.getMigrator("chrome");
  if (AppConstants.platform == "macosx") {
    Object.assign(migrator, {
      _keychainServiceName: mockMacOSKeychain.serviceName,
      _keychainAccountName: mockMacOSKeychain.accountName,
      _keychainMockPassphrase: mockMacOSKeychain.passphrase,
    });
  }
  Assert.ok(
    await migrator.isSourceAvailable(),
    "Sanity check the source exists"
  );

  Services.prefs.setBoolPref(
    "browser.migrate.chrome.payment_methods.enabled",
    false
  );
  Assert.ok(
    !(
      (await migrator.getMigrateData(PROFILE)) &
      MigrationUtils.resourceTypes.PAYMENT_METHODS
    ),
    "Should be able to disable migrating payment methods"
  );
  // Clear the cached resources now so that a re-check for payment methods
  // will look again.
  delete migrator._resourcesByProfile[PROFILE.id];

  Services.prefs.setBoolPref(
    "browser.migrate.chrome.payment_methods.enabled",
    true
  );

  Assert.ok(
    (await migrator.getMigrateData(PROFILE)) &
      MigrationUtils.resourceTypes.PAYMENT_METHODS,
    "Should be able to enable migrating payment methods"
  );

  let { formAutofillStorage } = ChromeUtils.importESModule(
    "resource://autofill/FormAutofillStorage.sys.mjs"
  );
  await formAutofillStorage.initialize();

  await promiseMigration(
    migrator,
    MigrationUtils.resourceTypes.PAYMENT_METHODS,
    PROFILE
  );

  let cards = await formAutofillStorage.creditCards.getAll();

  Assert.equal(
    cards.length,
    PAYMENT_METHODS.length,
    "Check there are still the same number of credit cards after re-importing the data"
  );
  Assert.equal(
    cards.length,
    MigrationUtils._importQuantities.cards,
    "Check telemetry matches the actual import."
  );

  for (let i = 0; i < PAYMENT_METHODS.length; i++) {
    checkCardsAreEqual(cards[i], PAYMENT_METHODS[i], i + 1);
  }
});
