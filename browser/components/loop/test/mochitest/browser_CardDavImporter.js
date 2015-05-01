/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {CardDavImporter} = Cu.import("resource:///modules/loop/CardDavImporter.jsm", {});

const kAuth = {
  "method": "basic",
  "user": "username",
  "password": "p455w0rd"
}


// "pid" for "provider ID"
let vcards = [
    "VERSION:3.0\n" +
    "N:Smith;John;;;\n" +
    "FN:John Smith\n" +
    "EMAIL;TYPE=work:john.smith@example.com\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid1\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "N:Smith;Jane;;;\n" +
    "FN:Jane Smith\n" +
    "EMAIL:jane.smith@example.com\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid2\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "N:García Fernández;Miguel Angel;José Antonio;Mr.;Jr.\n" +
    "FN:Mr. Miguel Angel José Antonio\n  García Fernández, Jr.\n" +
    "EMAIL:mike@example.org\n" +
    "EMAIL;PREF=1;TYPE=work:miguel.angel@example.net\n" +
    "EMAIL;TYPE=home;UNKNOWNPARAMETER=frotz:majacf@example.com\n" +
    "TEL:+3455555555\n" +
    "TEL;PREF=1;TYPE=work:+3455556666\n" +
    "TEL;TYPE=home;UNKNOWNPARAMETER=frotz:+3455557777\n" +
    "ADR:;Suite 123;Calle Aduana\\, 29;MADRID;;28070;SPAIN\n" +
    "ADR;TYPE=work:P.O. BOX 555;;;Washington;DC;20024-00555;USA\n" +
    "ORG:Acme España SL\n" +
    "TITLE:President\n" +
    "BDAY:1965-05-05\n" +
    "NOTE:Likes tulips\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid3\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "N:Jones;Bob;;;\n" +
    "EMAIL:bob.jones@example.com\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid4\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "N:Jones;Davy;Randall;;\n" +
    "EMAIL:davy.jones@example.com\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid5\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "EMAIL:trip@example.com\n" +
    "NICKNAME:Trip\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid6\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "EMAIL:acme@example.com\n" +
    "ORG:Acme, Inc.\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid7\n" +
    "END:VCARD\n",

    "VERSION:3.0\n" +
    "EMAIL:anyone@example.com\n" +
    "REV:2011-07-12T14:43:20Z\n" +
    "UID:pid8\n" +
    "END:VCARD\n",
];


const monkeyPatchImporter = function(importer) {
  // Set up the response bodies
  let listPropfind =
    '<?xml version="1.0" encoding="UTF-8"?>\n' +
    '<d:multistatus xmlns:card="urn:ietf:params:xml:ns:carddav"\n' +
    '               xmlns:d="DAV:">\n' +
    ' <d:response>\n' +
    '  <d:href>/carddav/abook/</d:href>\n' +
    '  <d:propstat>\n' +
    '   <d:status>HTTP/1.1 200 OK</d:status>\n' +
    '  </d:propstat>\n' +
    '  <d:propstat>\n' +
    '   <d:status>HTTP/1.1 404 Not Found</d:status>\n' +
    '   <d:prop>\n' +
    '    <d:getetag/>\n' +
    '   </d:prop>\n' +
    '  </d:propstat>\n' +
    ' </d:response>\n';

  let listReportMultiget =
    '<?xml version="1.0" encoding="UTF-8"?>\n' +
    '<d:multistatus xmlns:card="urn:ietf:params:xml:ns:carddav"\n' +
    '               xmlns:d="DAV:">\n';

  vcards.forEach(vcard => {
    let uid = /\nUID:(.*?)\n/.exec(vcard);
    listPropfind +=
      ' <d:response>\n' +
      '  <d:href>/carddav/abook/' + uid + '</d:href>\n' +
      '  <d:propstat>\n' +
      '   <d:status>HTTP/1.1 200 OK</d:status>\n' +
      '   <d:prop>\n' +
      '    <d:getetag>"2011-07-12T07:43:20.855-07:00"</d:getetag>\n' +
      '   </d:prop>\n' +
      '  </d:propstat>\n' +
      ' </d:response>\n';

    listReportMultiget +=
      ' <d:response>\n' +
      '  <d:href>/carddav/abook/' + uid + '</d:href>\n' +
      '  <d:propstat>\n' +
      '   <d:status>HTTP/1.1 200 OK</d:status>\n' +
      '   <d:prop>\n' +
      '    <d:getetag>"2011-07-12T07:43:20.855-07:00"</d:getetag>\n' +
      '    <card:address-data>' + vcard + '</card:address-data>\n' +
      '   </d:prop>\n' +
      '  </d:propstat>\n' +
      ' </d:response>\n';
  });

  listPropfind += "</d:multistatus>\n";
  listReportMultiget += "</d:multistatus>\n";

  importer._davPromise = function(method, url, auth, depth, body) {
    return new Promise((resolve, reject) => {

      if (auth.method != "basic" ||
          auth.user != kAuth.user ||
          auth.password != kAuth.password) {
        reject(new Error("401 Auth Failure"));
        return;
      }

      let request = method + " " + url + " " + depth;
      let xmlParser = new DOMParser();
      let responseXML;
      switch (request) {
        case "PROPFIND https://example.com/.well-known/carddav 1":
          responseXML = xmlParser.parseFromString(listPropfind, "text/xml");
          break;
        case "REPORT https://example.com/carddav/abook/ 1":
          responseXML = xmlParser.parseFromString(listReportMultiget, "text/xml");
          break;
        default:
          reject(new Error("404 Not Found"));
          return;
      }
      resolve({"responseXML": responseXML});
    });
  }.bind(importer);
  return importer;
}

add_task(function* test_CardDavImport() {
  let importer = monkeyPatchImporter(new CardDavImporter());
  yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.com",
        "auth": kAuth.method,
        "user": kAuth.user,
        "password": kAuth.password
      }, (err, result) => { err ? reject(err) : resolve(result); }, mockDb);
  });
  info("Import succeeded");

  Assert.equal(vcards.length, Object.keys(mockDb._store).length,
               "Should import all VCards into database");

  // Basic checks
  let c = mockDb._store[1];
  Assert.equal(c.name[0], "John Smith", "Full name should match");
  Assert.equal(c.givenName[0], "John", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "work", "Email type should match");
  Assert.equal(c.email[0].value, "john.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, false, "Pref should match");
  Assert.equal(c.id, "pid1@example.com", "UID should match and be scoped to provider");

  c = mockDb._store[2];
  Assert.equal(c.name[0], "Jane Smith", "Full name should match");
  Assert.equal(c.givenName[0], "Jane", "Given name should match");
  Assert.equal(c.familyName[0], "Smith", "Family name should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "jane.smith@example.com", "Email should match");
  Assert.equal(c.email[0].pref, false, "Pref should match");
  Assert.equal(c.id, "pid2@example.com", "UID should match and be scoped to provider");

  // Check every field
  c = mockDb._store[3];
  Assert.equal(c.name[0], "Mr. Miguel Angel José Antonio García Fernández, Jr.", "Full name should match");
  Assert.equal(c.givenName[0], "Miguel Angel", "Given name should match");
  Assert.equal(c.additionalName[0], "José Antonio", "Other name should match");
  Assert.equal(c.familyName[0], "García Fernández", "Family name should match");
  Assert.equal(c.email.length, 3, "Email count should match");
  Assert.equal(c.email[0].type, "other", "Email type should match");
  Assert.equal(c.email[0].value, "mike@example.org", "Email should match");
  Assert.equal(c.email[0].pref, false, "Pref should match");
  Assert.equal(c.email[1].type, "work", "Email type should match");
  Assert.equal(c.email[1].value, "miguel.angel@example.net", "Email should match");
  Assert.equal(c.email[1].pref, true, "Pref should match");
  Assert.equal(c.email[2].type, "home", "Email type should match");
  Assert.equal(c.email[2].value, "majacf@example.com", "Email should match");
  Assert.equal(c.email[2].pref, false, "Pref should match");
  Assert.equal(c.tel.length, 3, "Phone number count should match");
  Assert.equal(c.tel[0].type, "other", "Phone type should match");
  Assert.equal(c.tel[0].value, "+3455555555", "Phone number should match");
  Assert.equal(c.tel[0].pref, false, "Pref should match");
  Assert.equal(c.tel[1].type, "work", "Phone type should match");
  Assert.equal(c.tel[1].value, "+3455556666", "Phone number should match");
  Assert.equal(c.tel[1].pref, true, "Pref should match");
  Assert.equal(c.tel[2].type, "home", "Phone type should match");
  Assert.equal(c.tel[2].value, "+3455557777", "Phone number should match");
  Assert.equal(c.tel[2].pref, false, "Pref should match");
  Assert.equal(c.adr.length, 2, "Address count should match");
  Assert.equal(c.adr[0].pref, false, "Pref should match");
  Assert.equal(c.adr[0].type, "other", "Type should match");
  Assert.equal(c.adr[0].streetAddress, "Calle Aduana, 29 Suite 123", "Street address should match");
  Assert.equal(c.adr[0].locality, "MADRID", "Locality should match");
  Assert.equal(c.adr[0].postalCode, "28070", "Post code should match");
  Assert.equal(c.adr[0].countryName, "SPAIN", "Country should match");
  Assert.equal(c.adr[1].pref, false, "Pref should match");
  Assert.equal(c.adr[1].type, "work", "Type should match");
  Assert.equal(c.adr[1].streetAddress, "P.O. BOX 555", "Street address should match");
  Assert.equal(c.adr[1].locality, "Washington", "Locality should match");
  Assert.equal(c.adr[1].region, "DC", "Region should match");
  Assert.equal(c.adr[1].postalCode, "20024-00555", "Post code should match");
  Assert.equal(c.adr[1].countryName, "USA", "Country should match");
  Assert.equal(c.org[0], "Acme España SL", "Org should match");
  Assert.equal(c.jobTitle[0], "President", "Title should match");
  Assert.equal(c.note[0], "Likes tulips", "Note should match");
  let bday = new Date(c.bday);
  Assert.equal(bday.getUTCFullYear(), 1965, "Birthday year should match");
  Assert.equal(bday.getUTCMonth(), 4, "Birthday month should match");
  Assert.equal(bday.getUTCDate(), 5, "Birthday day should match");
  Assert.equal(c.id, "pid3@example.com", "UID should match and be scoped to provider");

  // Check name synthesis
  c = mockDb._store[4];
  Assert.equal(c.name[0], "Jones, Bob", "Full name should be synthesized correctly");
  c = mockDb._store[5];
  Assert.equal(c.name[0], "Jones, Davy Randall", "Full name should be synthesized correctly");
  c = mockDb._store[6];
  Assert.equal(c.name[0], "Trip", "Full name should be synthesized correctly");
  c = mockDb._store[7];
  Assert.equal(c.name[0], "Acme, Inc.", "Full name should be synthesized correctly");
  c = mockDb._store[8];
  Assert.equal(c.name[0], "anyone@example.com", "Full name should be synthesized correctly");

  // Check that a re-import doesn't cause contact duplication.
  yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.com",
        "auth": kAuth.method,
        "user": kAuth.user,
        "password": kAuth.password
      }, (err, result) => { err ? reject(err) : resolve(result); }, mockDb);
  });
  Assert.equal(vcards.length, Object.keys(mockDb._store).length,
               "Second import shouldn't increase DB size");

  // Check that errors are propagated back to caller
  let error = yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.com",
        "auth": kAuth.method,
        "user": kAuth.user,
        "password": "invalidpassword"
      }, (err, result) => { err ? resolve(err) : reject(new Error("Should have failed")); }, mockDb);
  });
  Assert.equal(error.message, "401 Auth Failure", "Auth error should propagate");

  error = yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.invalid",
        "auth": kAuth.method,
        "user": kAuth.user,
        "password": kAuth.password
      }, (err, result) => { err ? resolve(err) : reject(new Error("Should have failed")); }, mockDb);
  });
  Assert.equal(error.message, "404 Not Found", "Not found error should propagate");

  let tmp = mockDb.getByServiceId;
  mockDb.getByServiceId = function(serviceId, callback) {
    callback(new Error("getByServiceId failed"));
  };
  error = yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.com",
        "auth": kAuth.method,
        "user": kAuth.user,
        "password": kAuth.password
      }, (err, result) => { err ? resolve(err) : reject(new Error("Should have failed")); }, mockDb);
  });
  Assert.equal(error.message, "getByServiceId failed", "Database error should propagate");
  mockDb.getByServiceId = tmp;

  error = yield new Promise ((resolve, reject) => {
    info("Initiating import");
    importer.startImport({
        "host": "example.com",
      }, (err, result) => { err ? resolve(err) : reject(new Error("Should have failed")); }, mockDb);
  });
  Assert.equal(error.message, "No authentication specified", "Missing parameters should generate error");
})
